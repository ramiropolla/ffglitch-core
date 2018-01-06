
/* command-line options */
static const char *input_fname;
static const char *output_fname;
static const char *script_fname;
static const char *script_params;
/* TODO use avformat_match_stream_specifier */
static int selected_features[FFEDIT_FEAT_LAST];
static int selected_features_idx[FFEDIT_FEAT_LAST];
static int features_selected;
static int do_benchmark;

/*********************************************************************/
typedef struct {
    json_ctx_t *jctx;
    json_t *ffedit_sd[FFEDIT_FEAT_LAST];
    int64_t pkt_pos;
    int stream_index;
    int used;
} FFEditJSONQueueElement;

#define MAX_QUEUE 8
typedef struct {
    FFEditJSONQueueElement elements[MAX_QUEUE];
    COND_TYPE cond_empty;
    COND_TYPE cond_full;
    MUTEX_TYPE mutex;
    int abort_request;
} FFEditJSONQueue;

/*********************************************************************/
typedef struct {
    int s_init;

    const char *s_fname;

    const AVCodec **decs;
    int          nb_decs;

    FFEditJSONQueue jq_in;
    FFEditJSONQueue jq_out;

    COND_TYPE s_cond;
    MUTEX_TYPE s_mutex;
    THREAD_TYPE thread;
} FFEditScriptFuncContext;

/*********************************************************************/
static int parse_idx(char *str)
{
    ssize_t idx = -1;
    char *p = strchr(str, ':');
    if ( p != NULL )
    {
        char *endptr;
        *p++ = '\0';

        idx = strtol(p, &endptr, 10);
        if ( idx < 0 || p == endptr )
            return -2;
    }
    return idx;
}

static int opt_feature_internal(const char *arg)
{
    enum FFEditFeature feature;
    char str_feature[0x100];
    ssize_t idx;

    strncpy(str_feature, arg, sizeof(str_feature) - 1);
    idx = parse_idx(str_feature);
    if ( idx == -2 )
        return AVERROR(EINVAL);

    feature = ffe_str_to_feat(str_feature);
    if ( feature != FFEDIT_FEAT_LAST )
    {
        selected_features[feature] = 1;
        selected_features_idx[feature] = idx;
        features_selected = 1;
        return 0;
    }

    return AVERROR(EINVAL);
}

/*********************************************************************/
static int element_sort_fn(const void *j1, const void *j2)
{
    const FFEditJSONQueueElement *el1 = (FFEditJSONQueueElement *) j1;
    const FFEditJSONQueueElement *el2 = (FFEditJSONQueueElement *) j2;
    int64_t diff64 = el1->pkt_pos - el2->pkt_pos;
    return (int) diff64;
}

static void add_frame_to_ffedit_json_queue(
        FFEditJSONQueue *jq,
        AVFrame *iframe,
        int stream_index)
{
    int idx;

    MUTEX_LOCK(jq->mutex);

    do
    {
        if ( jq->abort_request )
            break;
        for ( idx = 0; idx < MAX_QUEUE; idx++ )
        {
            FFEditJSONQueueElement *element = &jq->elements[idx];
            if ( !element->used )
            {
                memcpy(element->ffedit_sd, iframe->ffedit_sd, sizeof(json_t *)*FFEDIT_FEAT_LAST);
                if ( iframe->jctx != NULL )
                {
                    element->jctx = iframe->jctx;
                    iframe->jctx = NULL;
                }
                element->pkt_pos = iframe->pkt_pos;
                element->stream_index = stream_index;
                element->used = 1;
                qsort(jq->elements, MAX_QUEUE, sizeof(FFEditJSONQueueElement), element_sort_fn);
                break;
            }
        }
        if ( idx == MAX_QUEUE )
            COND_WAIT(jq->cond_full, jq->mutex);
        else
            COND_SIGNAL(jq->cond_empty);
    } while ( idx == MAX_QUEUE );

    MUTEX_UNLOCK(jq->mutex);
}

/* if ipkt->pos is -1, get the first packet, otherwise get the packet
 * that matches ipkt->pos */
static void get_from_ffedit_json_queue(FFEditJSONQueue *jq, AVPacket *ipkt)
{
    int idx;

    MUTEX_LOCK(jq->mutex);

    do
    {
        if ( jq->abort_request )
        {
            /* poison pill */
            ipkt->pos = -2;
            break;
        }
        for ( idx = 0; idx < MAX_QUEUE; idx++ )
        {
            FFEditJSONQueueElement *element = &jq->elements[idx];
            if ( element->used
              && (ipkt->pos == -1 || element->pkt_pos == ipkt->pos) )
            {
                memcpy(ipkt->ffedit_sd, element->ffedit_sd, sizeof(json_t *)*FFEDIT_FEAT_LAST);
                if ( element->jctx != NULL )
                {
                    ipkt->jctx = element->jctx;
                    element->jctx = NULL;
                }
                ipkt->pos = element->pkt_pos;
                ipkt->stream_index = element->stream_index;
                element->used = 0;
                break;
            }
        }
        if ( idx == MAX_QUEUE )
            COND_WAIT(jq->cond_empty, jq->mutex);
        else
            COND_SIGNAL(jq->cond_full);
    } while ( idx == MAX_QUEUE );

    MUTEX_UNLOCK(jq->mutex);
}

static void flush_ffedit_json_queue(FFEditJSONQueue *jq)
{
    int idx;

    MUTEX_LOCK(jq->mutex);
    for ( idx = 0; idx < MAX_QUEUE; idx++ )
    {
        FFEditJSONQueueElement *element = &jq->elements[idx];
        if ( idx == 0 )
        {
            /* one poison pill for everyone! */
            element->jctx = NULL;
            element->pkt_pos = -2;
            element->stream_index = -1;
            element->used = 1;
        }
        else
        {
            element->used = 0;
        }
    }
    jq->abort_request = 1;
    COND_SIGNAL(jq->cond_full);
    COND_SIGNAL(jq->cond_empty);
    MUTEX_UNLOCK(jq->mutex);
}

/*********************************************************************/
static void sfc_init(FFEditScriptFuncContext *sfc, const char *s_fname)
{
    /* set script_name */
    sfc->s_fname = s_fname;

    /* initialize queues */
    MUTEX_INIT(sfc->jq_in.mutex);
    COND_INIT(sfc->jq_in.cond_empty);
    COND_INIT(sfc->jq_in.cond_full);
    MUTEX_INIT(sfc->jq_out.mutex);
    COND_INIT(sfc->jq_out.cond_empty);
    COND_INIT(sfc->jq_out.cond_full);

    /* initialize setup mutex */
    sfc->s_init = 0;
    MUTEX_INIT(sfc->s_mutex);
    COND_INIT(sfc->s_cond);

    /* init rest of resources */
    sfc->decs = NULL;
    sfc->nb_decs = 0;
}

static void sfc_setup(FFEditScriptFuncContext *sfc, THREAD_RET_TYPE (*func)(void *))
{
    /* start is_importing_script thread */
    THREAD_CREATE(sfc->thread, func, sfc);

    /* wait for script setup */
    MUTEX_LOCK(sfc->s_mutex);
    while ( !sfc->s_init )
        COND_WAIT(sfc->s_cond, sfc->s_mutex);
    MUTEX_UNLOCK(sfc->s_mutex);
}

static void sfc_resume(FFEditScriptFuncContext *sfc)
{
    MUTEX_LOCK(sfc->s_mutex);
    sfc->s_init = 0;
    COND_SIGNAL(sfc->s_cond);
    MUTEX_UNLOCK(sfc->s_mutex);
}

static void sfc_destroy(FFEditScriptFuncContext *sfc)
{
    /* wait for thread to finish */
    THREAD_JOIN(sfc->thread);

    /* destroy queues */
    MUTEX_DESTROY(sfc->jq_in.mutex);
    COND_DESTROY(sfc->jq_in.cond_empty);
    COND_DESTROY(sfc->jq_in.cond_full);
    MUTEX_DESTROY(sfc->jq_out.mutex);
    COND_DESTROY(sfc->jq_out.cond_empty);
    COND_DESTROY(sfc->jq_out.cond_full);

    /* destroy setup mutex */
    MUTEX_DESTROY(sfc->s_mutex);
    COND_DESTROY(sfc->s_cond);

    /* free rest of resources */
    av_freep(&sfc->decs);
}

/*********************************************************************/
static void ffedit_common_setup(
        FFEditScriptFuncContext *sfc,
        FFScriptContext *script,
        FFScriptObject *setup_func)
{
    json_ctx_t jctx;
    json_t *features;
    json_t *args;
    json_t *o_fname;
    FFScriptObject *frame = NULL;
    size_t len;
    int ret;

    /* prepare args */
    json_ctx_start(&jctx, 1);
    features = json_dynamic_array_new(&jctx);
    args = json_object_new(&jctx);
    for ( size_t i = 0; i < FFEDIT_FEAT_LAST; i++ )
    {
        if ( selected_features[i] != 0 )
        {
            const char *feat_str = ffe_feat_to_str(i);
            json_t *feature = json_string_new(&jctx, feat_str);
            json_dynamic_array_add(features, feature);
        }
    }
    json_dynamic_array_done(&jctx, features);
    json_object_add(args, "features", features);
    if ( input_fname != NULL )
    {
        json_t *i_fname = json_string_new(&jctx, input_fname);
        json_object_add(args, "input", i_fname);
    }
    if ( output_fname != NULL )
    {
        o_fname = json_string_new(&jctx, output_fname);
        json_object_add(args, "output", o_fname);
    }
    if ( script_params != NULL )
    {
        json_t *params = json_parse(&jctx, script_params);
        if ( params == NULL )
        {
            const char *fname = "<params>";
            json_error_ctx_t jectx;
            json_error_parse(&jectx, script_params);
            av_log(ffe_class, AV_LOG_FATAL, "%s:%d:%d: %s\n",
                   fname, (int) jectx.line, (int) jectx.offset, jectx.str);
            av_log(ffe_class, AV_LOG_FATAL, "%s:%d:%s\n", fname, (int) jectx.line, jectx.buf);
            av_log(ffe_class, AV_LOG_FATAL, "%s:%d:%s\n", fname, (int) jectx.line, jectx.column);
            json_error_free(&jectx);
            exit(1);
        }
        json_object_add(args, "params", params);
    }
    json_object_done(&jctx, args);

    /* convert args from json */
    frame = ff_script_from_json(script, args);

    /* call setup() */
    ret = ff_script_call_func(script, NULL, setup_func, frame, NULL);
    if ( ret < 0 )
    {
        av_log(script, AV_LOG_FATAL, "Error calling setup() function in %s\n", script->script_fname);
        exit(1);
    }

    /* convert args back to json */
    args = ff_script_to_json(&jctx, script, frame);

    /* check returned pix_fmt */
    o_fname = json_object_get(args, "output");
    if ( o_fname != NULL )
    {
        const char *new_output_fname;
        if ( JSON_TYPE(o_fname->flags) != JSON_TYPE_STRING )
        {
            av_log(script, AV_LOG_FATAL, "args[\"output\"] returned from setup() must be a string!\n");
            exit(1);
        }
        new_output_fname = json_string_get(o_fname);
        if ( (output_fname != NULL) && (strcmp(output_fname, new_output_fname) != 0) )
        {
            av_log(script, AV_LOG_WARNING, "args[\"output\"] has changed the value that was passed in the command line!\n");
            av_log(script, AV_LOG_WARNING, " before: %s\n", output_fname);
            av_log(script, AV_LOG_WARNING, " after : %s\n", new_output_fname);
        }
        av_freep(&output_fname);
        output_fname = av_strdup(new_output_fname);
    }

    /* parse returned features */
    features = json_object_get(args, "features");
    if ( features == NULL || JSON_TYPE(features->flags) != JSON_TYPE_ARRAY )
    {
        av_log(script, AV_LOG_FATAL, "args[\"features\"] returned from setup() must be an array!\n");
        exit(1);
    }
    len = json_array_length(features);
    for ( size_t i = 0; i < len; i++ )
    {
        json_t *feature = json_array_get(features, i);
        const char *feat_str;
        if ( JSON_TYPE(feature->flags) != JSON_TYPE_STRING )
        {
            av_log(script, AV_LOG_FATAL, "elements in args[\"features\"] returned from setup() must be strings!\n");
            exit(1);
        }
        feat_str = json_string_get(feature);
        ret = opt_feature_internal(feat_str);
        if ( ret < 0 )
        {
            av_log(script, AV_LOG_ERROR, "Failed to set value '%s' for option 'features': %s\n", feat_str, av_err2str(ret));
            exit(1);
        }
    }

    /* free stuff */
    ff_script_free_obj(script, frame);
    json_ctx_free(&jctx);
}

/*********************************************************************/
static int64_t benchmark_convert_to_quickjs;
static int64_t benchmark_script_call;
static int64_t benchmark_convert_from_quickjs;
static THREAD_RET_TYPE script_func(void *arg)
{
    int64_t t0;
    int64_t t1;
    FFEditScriptFuncContext *sfc = (FFEditScriptFuncContext *) arg;
    AVPacket ipkt;
    AVFrame iframe;
    FFScriptContext *script;
    FFScriptObject *setup_func;
    FFScriptObject *glitch_frame;
    FFScriptObject **streams = NULL;
    int           nb_streams = 0;

    /* init script */
    script = ff_script_init(sfc->s_fname, FFSCRIPT_FLAGS_AOI_CACHE);
    if ( script == NULL )
        exit(1);

    /* get functions */
    setup_func = ff_script_get_func(script, "setup", 0);
    glitch_frame = ff_script_get_func(script, "glitch_frame", 1);

    /* glitch_frame() is mandatory */
    if ( glitch_frame == NULL )
    {
        if ( setup_func != NULL )
            ff_script_free_obj(script, setup_func);
        ff_script_uninit(&script);
        exit(1);
    }

    /* call setup function */
    if ( setup_func != NULL )
    {
        ffedit_common_setup(sfc, script, setup_func);
        ff_script_free_obj(script, setup_func);
    }

    /* script setup finished */
    MUTEX_LOCK(sfc->s_mutex);
    sfc->s_init = 1;
    COND_SIGNAL(sfc->s_cond);
    MUTEX_UNLOCK(sfc->s_mutex);

    /* wait to resume script */
    MUTEX_LOCK(sfc->s_mutex);
    while ( sfc->s_init )
        COND_WAIT(sfc->s_cond, sfc->s_mutex);
    MUTEX_UNLOCK(sfc->s_mutex);

    /* prepare the arguments for each stream */
    {
        json_ctx_t jctx;
        json_ctx_start(&jctx, 1);
        for ( size_t i = 0; i < sfc->nb_decs; i++ )
        {
            json_t *jstream = json_object_new(&jctx);
            json_t *jcodec = json_string_new(&jctx, sfc->decs[i]->name);
            json_t *jstream_index = json_int_new(&jctx, i);
            json_object_add(jstream, "codec", jcodec);
            json_object_add(jstream, "stream_index", jstream_index);
            json_object_done(&jctx, jstream);
            GROW_ARRAY(streams, nb_streams);
            streams[i] = (FFScriptObject *) ff_script_from_json(script, jstream);
        }
        json_ctx_free(&jctx);
    }

    while ( 42 )
    {
        json_t *jargs;
        json_t *jframe;
        FFScriptObject *frame;
        FFScriptObject *stream;
        int ret;

        /* get earliest packet from input queue */
        ipkt.pos = -1;
        get_from_ffedit_json_queue(&sfc->jq_in, &ipkt);

        /* check for poison pill */
        if ( ipkt.pos == -2 )
            break;

        if ( do_benchmark )
            t0 = av_gettime_relative();

        /* convert to quickjs */
        jargs = json_object_new(ipkt.jctx);
        for ( size_t i = 0; i < FFEDIT_FEAT_LAST; i++ )
            if ( ipkt.ffedit_sd[i] != NULL )
                json_object_add(jargs, ffe_feat_to_str(i), ipkt.ffedit_sd[i]);
        json_object_done(ipkt.jctx, jargs);
        frame = (FFScriptObject *) ff_script_from_json(script, jargs);
        /* free jctx from ipkt, we no longer need it */
        json_ctx_free(ipkt.jctx);
        av_freep(&ipkt.jctx);

        if ( do_benchmark )
        {
            t1 = av_gettime_relative();
            benchmark_convert_to_quickjs += (t1 - t0);
            t0 = t1;
        }

        /* call glitch_frame() with data */
        stream = streams[ipkt.stream_index];
        ret = ff_script_call_func(script, NULL, glitch_frame, frame, stream, NULL);
        if ( ret < 0 )
            exit(1);

        if ( do_benchmark )
        {
            t1 = av_gettime_relative();
            benchmark_script_call += (t1 - t0);
            t0 = t1;
        }

        /* convert back from python/quickjs */
        iframe.jctx = av_mallocz(sizeof(json_ctx_t));
        json_ctx_start(iframe.jctx, 1);
        jframe = ff_script_to_json(iframe.jctx, script, frame);
        for ( size_t i = 0; i < FFEDIT_FEAT_LAST; i++ )
        {
            const char *key = ffe_feat_to_str(i);
            iframe.ffedit_sd[i] = json_object_get(jframe, key);
        }
        iframe.pkt_pos = ipkt.pos;
        ff_script_free_obj(script, frame);

        if ( do_benchmark )
        {
            t1 = av_gettime_relative();
            benchmark_convert_from_quickjs += (t1 - t0);
            t0 = t1;
        }

        /* add back to output queue */
        add_frame_to_ffedit_json_queue(&sfc->jq_out, &iframe, ipkt.stream_index);
    }

    ff_script_free_obj(script, glitch_frame);

    for ( size_t i = 0; i < sfc->nb_decs; i++ )
        ff_script_free_obj(script, streams[i]);
    av_freep(&streams);

    /* uninit script */
    ff_script_uninit(&script);

    return THREAD_RET_OK;
}

/*********************************************************************/
static int check_seekability(
        AVFormatContext *fctx,
        FFEditOutputContext **pffo_fmt,
        FFEditOutputContext *ffo_codec)
{
    int raw_input_stream = ((fctx->iformat->flags & AVFMT_FFEDIT_RAWSTREAM) != 0);
    int nonseekable_input = (fctx->pb != NULL) && ((fctx->pb->seekable & AVIO_SEEKABLE_NORMAL) == 0);
    int nonseekable_output = ((ffo_codec->o_pb->seekable & AVIO_SEEKABLE_NORMAL) == 0);
    av_log(ffe_class, AV_LOG_DEBUG, "raw_input_stream %d\n", raw_input_stream);
    av_log(ffe_class, AV_LOG_DEBUG, "nonseekable_input %d\n", nonseekable_input);
    av_log(ffe_class, AV_LOG_DEBUG, "nonseekable_output %d\n", nonseekable_output);
    if ( (nonseekable_input || nonseekable_output) && !raw_input_stream )
    {
        av_log(ffe_class, AV_LOG_FATAL, "Input or output stream are non-seekable (probably a pipe),\n");
        av_log(ffe_class, AV_LOG_FATAL, "but the input stream is not a raw format (%s)!\n", fctx->iformat->long_name);
        return -1;
    }
    if ( raw_input_stream )
    {
        /* if the input stream is raw, there will be no fixups in the
         * format, therefore we don't need to keep ffo_fmt, and we can
         * write the output directly. */
        ffe_output_freep(pffo_fmt);
        fctx->ectx = NULL;
        ffe_set_directwrite(ffo_codec);
    }
    return 0;
}
