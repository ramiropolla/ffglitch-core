
#include "libavutil/json.h"

#include "config.h"

#include "libavutil/opt.h"
#include "libavutil/avassert.h"
#include "libavutil/avstring.h"
#include "libavutil/bprint.h"
#include "libavutil/ffversion.h"
#include "libavutil/sha.h"
#include "libavutil/time.h"
#include "libavutil/thread.h"
#include "libavformat/ffedit.h"
#include "libavcodec/ffedit.h"

#include "cmdutils.h"

const char program_name[] = "ffedit";
const int program_birth_year = 2000; // FFmpeg, that is

#define TIME_JSON_PARSE 0
#define TIME_JSON_FPUTS 0

/* command-line options */
static const char *input_fname;
static const char *output_fname;
static const char *apply_fname;
static const char *export_fname;
static const char *script_fname;
static int script_is_js;
static int script_is_py;
/* TODO use avformat_match_stream_specifier */
static int selected_features[FFEDIT_FEAT_LAST];
static int selected_features_idx[FFEDIT_FEAT_LAST];
static int features_selected;
static int test_mode;
static int file_overwrite;
static const char *threads;

typedef struct {
    json_ctx_t *jctxs;
    int      nb_jctxs;
    json_t *jroot;
    json_t *jstreams0;
    json_t **jstreams;
    int   nb_jstreams;
    json_t **jstframes;
    int   nb_jstframes;

    FILE *export_fp;

    size_t *frames_idx;
} JSONFile;

typedef struct {
    json_ctx_t jctx;
    json_t *ffedit_sd[32];
    int64_t pkt_pos;
    int used;
} JSONQueueElement;

#define MAX_QUEUE 4
typedef struct {
    JSONQueueElement elements[MAX_QUEUE];
    pthread_cond_t cond_empty;
    pthread_cond_t cond_full;
    pthread_mutex_t mutex;
} JSONQueue;

typedef struct {
    AVFormatContext *fctx;
    AVCodec **decs;
    int    nb_decs;
    AVCodecContext **dctxs;
    int           nb_dctxs;

    int is_exporting;
    int is_applying; // implies importing

    int is_exporting_script;
    int is_applying_script;

    FFEditOutputContext *ectx;

    JSONFile *jf;

    int fret;

    int frame_number;

    /* for scripting */
    JSONQueue *jq_in;
    JSONQueue *jq_out;

    const char *s_fname;
    char *s_buf;
    size_t s_size;
    pthread_cond_t s_cond;
    pthread_mutex_t s_mutex;
    int s_init;
} FFFile;

static int opt_feature(void *optctx, const char *opt, const char *arg);

/* forward declarations */
static char *read_file(const char *fname, size_t *psize);
static void sha1sum(char *shasumstr, const char *buf, size_t size);
static int sort_by_pkt_pos(const void *j1, const void *j2);
static void add_frame_to_ffedit_json_queue(JSONQueue *jq, AVFrame *iframe);
static void get_from_ffedit_json_queue(JSONQueue *jq, AVPacket *ipkt);

#include "ffedit_quickjs.c"
#include "ffedit_python.c"

static JSONFile *read_ffedit_json_file(const char *_apply_fname)
{
#if TIME_JSON_PARSE
    int64_t diff;
    int64_t t0;
    int64_t t1;
#endif

    JSONFile *jf = NULL;
    size_t size;
    char *buf;

    jf = av_mallocz(sizeof(JSONFile));

    buf = read_file(_apply_fname, &size);
    if ( buf == NULL )
    {
        av_log(NULL, AV_LOG_FATAL,
                "Could not open json file %s\n", _apply_fname);
        exit(1);
    }

    GROW_ARRAY(jf->jctxs, jf->nb_jctxs);
    json_ctx_start(&jf->jctxs[0]);

#if TIME_JSON_PARSE
    t0 = av_gettime_relative();
#endif

    jf->jroot = json_parse(&jf->jctxs[0], buf);

#if TIME_JSON_PARSE
    t1 = av_gettime_relative();
    diff = (t1 - t0);
    printf("time taken in json_parse() %" PRId64 "\n", diff);
#endif

    if ( jf->jroot == NULL )
    {
        json_error_ctx_t jectx;
        json_error_parse(&jectx, buf);
        av_log(NULL, AV_LOG_FATAL, "%s:%d:%d: %s\n",
               _apply_fname, (int) jectx.line, (int) jectx.offset, jectx.str);
        av_log(NULL, AV_LOG_FATAL, "%s:%d:%s\n", _apply_fname, (int) jectx.line, jectx.buf);
        av_log(NULL, AV_LOG_FATAL, "%s:%d:%s\n", _apply_fname, (int) jectx.line, jectx.column);
        json_error_free(&jectx);
        exit(1);
    }
    jf->jstreams0 = json_object_get(jf->jroot, "streams");

    size = json_array_length(jf->jstreams0);
    for ( size_t i = 0; i < size; i++ )
    {
        GROW_ARRAY(jf->jstreams, jf->nb_jstreams);
        GROW_ARRAY(jf->jstframes, jf->nb_jstframes);
        jf->jstreams[i] = json_array_get(jf->jstreams0, i);
        jf->jstframes[i] = json_object_get(jf->jstreams[i], "frames");
    }

    jf->frames_idx = av_calloc(jf->nb_jstframes, sizeof(size_t));

    av_free(buf);

    return jf;
}

static void reset_frames_idx_ffedit_json_file(JSONFile *jf)
{
    memset(jf->frames_idx, 0x00, jf->nb_jstframes * sizeof(size_t));
}

static JSONFile *prepare_ffedit_json_file(
        const char *_export_fname,
        const char *_input_fname,
        int *_selected_features)
{
    JSONFile *jf = NULL;
    json_t *jfname = NULL;
    json_t *jfeatures = NULL;
    size_t size;
    char *buf;

    jf = av_mallocz(sizeof(JSONFile));

    // TODO check if file exists before overwritting
    jf->export_fp = fopen(_export_fname, "w");
    if ( jf->export_fp == NULL )
    {
        av_log(NULL, AV_LOG_FATAL,
                "Could not open json file %s\n", _export_fname);
        av_free(jf);
        return NULL;
    }

    GROW_ARRAY(jf->jctxs, jf->nb_jctxs);
    json_ctx_start(&jf->jctxs[0]);
    jf->jroot = json_object_new(&jf->jctxs[0]);

    if ( test_mode == 0 )
    {
        json_t *jversion = json_string_new(&jf->jctxs[0], FFMPEG_VERSION);
        json_object_add(jf->jroot, "ffedit_version", jversion);
    }

    jfname = json_string_new(&jf->jctxs[0], av_basename(_input_fname));
    json_object_add(jf->jroot, "filename", jfname);

    buf = read_file(_input_fname, &size);
    if ( buf != NULL )
    {
        json_t *jshasum = NULL;
        char shasumstr[41];

        sha1sum(shasumstr, buf, size);

        av_free(buf);

        jshasum = json_string_new(&jf->jctxs[0], shasumstr);
        json_object_add(jf->jroot, "sha1sum", jshasum);
    }

    jfeatures = json_dynamic_array_new(&jf->jctxs[0]);
    for ( size_t i = 0; i < FFEDIT_FEAT_LAST; i++ )
    {
        if ( _selected_features[i] )
        {
            const char *feat_str = ffe_feat_to_str(i);
            json_t *jfeature = json_string_new(&jf->jctxs[0], feat_str);
            json_dynamic_array_add(jfeatures, jfeature);
        }
    }
    json_dynamic_array_done(&jf->jctxs[0], jfeatures);
    json_object_add(jf->jroot, "features", jfeatures);

    jf->jstreams0 = json_dynamic_array_new(&jf->jctxs[0]);
    json_object_add(jf->jroot, "streams", jf->jstreams0);

    return jf;
}

static void add_stream_to_ffedit_json_file(JSONFile *jf, size_t i)
{
    json_t *jstream = json_object_new(&jf->jctxs[0]);
    json_t *jframes = json_dynamic_array_new(&jf->jctxs[0]);

    GROW_ARRAY(jf->jstreams, jf->nb_jstreams);
    GROW_ARRAY(jf->jstframes, jf->nb_jstframes);

    jf->jstreams[i] = jstream;
    jf->jstframes[i] = jframes;

    json_object_add(jstream, "frames", jframes);
    json_dynamic_array_add(jf->jstreams0, jstream);
}

static void add_frame_to_ffedit_json_file(
        JSONFile *jf,
        AVFrame *iframe,
        int stream_index)
{
    json_t *jframes = jf->jstframes[stream_index];
    json_t *jframe = json_object_new(&jf->jctxs[0]);
    json_t *jpkt_pos = json_int_new(&jf->jctxs[0], iframe->pkt_pos);
    json_t *jpts = json_int_new(&jf->jctxs[0], iframe->pts);
    json_t *jdts = json_int_new(&jf->jctxs[0], iframe->pkt_dts);

    json_object_add(jframe, "pkt_pos", jpkt_pos);
    json_object_add(jframe, "pts", jpts);
    json_object_add(jframe, "dts", jdts);

    for ( size_t i = 0; i < FFEDIT_FEAT_LAST; i++ )
    {
        const char *key = ffe_feat_to_str(i);
        json_t *jso = iframe->ffedit_sd[i];
        if ( jso != NULL )
            json_object_add(jframe, key, jso);
    }
    json_object_done(&jf->jctxs[0], jframe);

    json_dynamic_array_add(jframes, jframe);
}

static void get_from_ffedit_json_file(
        JSONFile *jf,
        AVPacket *ipkt,
        int stream_index)
{
    size_t idx = jf->frames_idx[stream_index];
    json_t *jframes = jf->jstframes[stream_index];
    json_t *jframe = json_array_get(jframes, idx);

#if 0
    // TODO check pkt_pos
    int pkt_pos_from_json = json_int_val(json_object_get(jframe, "pkt_pos"));
    if ( pkt_pos_from_json != ipkt->pos )
        exit(1);
#endif

    for ( size_t i = 0; i < FFEDIT_FEAT_LAST; i++ )
    {
        const char *key = ffe_feat_to_str(i);
        ipkt->ffedit_sd[i] = json_object_get(jframe, key);
    }
    ipkt->jctx = &jf->jctxs[0];

    jf->frames_idx[stream_index]++;
}

static int element_sort_fn(const void *j1, const void *j2)
{
    const JSONQueueElement *el1 = (JSONQueueElement *) j1;
    const JSONQueueElement *el2 = (JSONQueueElement *) j2;
    int64_t diff64 = el1->pkt_pos - el2->pkt_pos;
    return (int) diff64;
}

static void add_frame_to_ffedit_json_queue(JSONQueue *jq, AVFrame *iframe)
{
    int idx;

    pthread_mutex_lock(&jq->mutex);

    do
    {
        for ( idx = 0; idx < MAX_QUEUE; idx++ )
        {
            JSONQueueElement *element = &jq->elements[idx];
            if ( !element->used )
            {
                memcpy(element->ffedit_sd, iframe->ffedit_sd, sizeof(element->ffedit_sd));
                if ( iframe->jctx != NULL )
                {
                    memcpy(&element->jctx, iframe->jctx, sizeof(element->jctx));
                    av_freep(&iframe->jctx);
                }
                element->pkt_pos = iframe->pkt_pos;
                element->used = 1;
                qsort(jq->elements, MAX_QUEUE, sizeof(JSONQueueElement), element_sort_fn);
                break;
            }
        }
        if ( idx == MAX_QUEUE )
            pthread_cond_wait(&jq->cond_full, &jq->mutex);
        else
            pthread_cond_signal(&jq->cond_empty);
    } while ( idx == MAX_QUEUE );

    pthread_mutex_unlock(&jq->mutex);
}

static void get_from_ffedit_json_queue(JSONQueue *jq, AVPacket *ipkt)
{
    int idx = MAX_QUEUE;

    pthread_mutex_lock(&jq->mutex);

    do
    {
        for ( idx = 0; idx < MAX_QUEUE; idx++ )
        {
            JSONQueueElement *element = &jq->elements[idx];
            if ( element->used
              && (ipkt->pos == -1 || element->pkt_pos == ipkt->pos) )
            {
                memcpy(ipkt->ffedit_sd, element->ffedit_sd, sizeof(ipkt->ffedit_sd));
                ipkt->jctx = av_mallocz(sizeof(json_ctx_t));
                memcpy(ipkt->jctx, &element->jctx, sizeof(json_ctx_t));
                ipkt->pos = element->pkt_pos;
                element->used = 0;
                break;
            }
        }
        if ( idx == MAX_QUEUE )
            pthread_cond_wait(&jq->cond_empty, &jq->mutex);
        else
            pthread_cond_signal(&jq->cond_full);
    } while ( idx == MAX_QUEUE );

    pthread_mutex_unlock(&jq->mutex);
}

static void close_ffedit_json_file(JSONFile *jf, FFFile *fff)
{
    /* write output json file if needed */
    if ( jf->export_fp != NULL && jf->jstframes != NULL )
    {
#if TIME_JSON_FPUTS
        int64_t diff;
        int64_t t0;
        int64_t t1;
#endif

        /* close json_ctx_t dynamic objects */
        for ( size_t i = 0; i < jf->nb_jstreams; i++ )
            json_object_done(&jf->jctxs[0], jf->jstreams[i]);
        json_object_done(&jf->jctxs[0], jf->jroot);

        for ( size_t i = 0; i < fff->fctx->nb_streams; i++ )
        {
            json_dynamic_array_done(&jf->jctxs[0], jf->jstframes[i]);
            json_array_sort(jf->jstframes[i], sort_by_pkt_pos);
        }

#if TIME_JSON_FPUTS
        t0 = av_gettime_relative();
#endif

        json_fputs(jf->export_fp, jf->jroot);

#if TIME_JSON_FPUTS
        t1 = av_gettime_relative();
        diff = (t1 - t0);
        printf("time taken in json_fputs() %" PRId64 "\n", diff);
#endif

        fclose(jf->export_fp);
    }

    /* free json-c objects */
    if ( jf->jroot != NULL )
    {
        for ( size_t i = 0; i < jf->nb_jctxs; i++ )
            json_ctx_free(&jf->jctxs[i]);
        av_freep(&jf->jctxs);
    }

    /* free temporary variables */
    if ( jf->frames_idx != NULL )
        av_freep(&jf->frames_idx);
}

static char nibble2char(uint8_t c)
{
    return c + (c > 9 ? ('a'-10) : '0');
}

static char *read_file(const char *fname, size_t *psize)
{
    size_t size;
    char *buf;
    FILE *fp;

    fp = fopen(fname, "rb");
    if ( fp == NULL )
        return NULL;

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    buf = av_malloc(size+1);
    buf[size] = '\0';

    fread(buf, size, 1, fp);
    fclose(fp);

    *psize = size;

    return buf;
}

static void sha1sum(char *shasumstr, const char *buf, size_t size)
{
    struct AVSHA *sha;
    uint8_t shasum[20];

    sha = av_sha_alloc();
    av_sha_init(sha, 160);
    av_sha_update(sha, buf, size);
    av_sha_final(sha, shasum);
    av_free(sha);

    for ( size_t i = 0; i < 20; i++ )
    {
        shasumstr[i*2+0] = nibble2char(shasum[i] >> 4);
        shasumstr[i*2+1] = nibble2char(shasum[i] & 0xF);
    }
    shasumstr[40] = '\0';
}

static const OptionDef options[] = {
    CMDUTILS_COMMON_OPTIONS
    { "i", HAS_ARG | OPT_STRING, { &input_fname }, "input file (may be omitted)", "file" },
    { "o", HAS_ARG | OPT_STRING, { &output_fname }, "output file (may be omitted)", "file" },
    { "a", HAS_ARG | OPT_STRING, { &apply_fname }, "apply data", "json file" },
    { "e", HAS_ARG | OPT_STRING, { &export_fname }, "export data", "json file" },
    { "s", HAS_ARG | OPT_STRING, { &script_fname }, "run script", "javascript or python file" },
    { "f", HAS_ARG, { .func_arg = opt_feature }, "select feature (optionally specify stream with feat:#)", "feature" },
    { "t", OPT_BOOL, { &test_mode }, "test mode (bitexact output)" },
    { "y", OPT_BOOL, { &file_overwrite }, "overwrite output files" },
    { "threads", HAS_ARG | OPT_STRING, { &threads }, "set the number of threads" },
    { NULL, },
};

static void show_usage(void)
{
    av_log(NULL, AV_LOG_INFO, "Simple media glitcher\n");
    av_log(NULL, AV_LOG_INFO, "usage: %s [options] input_file [output_file]\n", program_name);
    av_log(NULL, AV_LOG_INFO, "\n");
}

static void print_example(const char *desc, const char *str)
{
    av_log(NULL, AV_LOG_INFO, "\t%s:\n", desc);
    av_log(NULL, AV_LOG_INFO, "\t\tffedit %s\n", str);
}

void show_help_default(const char *opt, const char *arg)
{
    av_log_set_callback(log_callback_help);
    show_usage();
    show_help_options(options, "Main options:", 0, OPT_EXPERT, 0);
    av_log(NULL, AV_LOG_INFO, "Examples:\n");
    print_example("check for supported glitches",
                  "input.avi");
    print_example("replicate file (for testing)",
                  "input.mpg output.mpg");
    print_example("export all supported data to a json file",
                  "input.avi -e data.json");
    print_example("export quantized DC coeffs from an mjpeg AVI a json file",
                  "input.mjpeg -f q_dc -e data.json");
    print_example("apply all glitched data from json file",
                  "input.avi -a data.json output.avi");
    print_example("apply only quantized DC coeffs from json file",
                  "input.jpg -f q_dc -a data.json output.jpg");
    print_example("export motion vectors from stream 0 to a json file",
                  "input.mpg -f mv:0 -e data.json");
}

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

static int opt_feature(void *optctx, const char *opt, const char *arg)
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

static void opt_filenames(void *optctx, const char *filename)
{
    if ( input_fname == NULL )
    {
        input_fname = av_strdup(filename);
    }
    else if ( output_fname == NULL )
    {
        output_fname = av_strdup(filename);
    }
    else
    {
        av_log(NULL, AV_LOG_FATAL,
               "Too many filenames (at most one input and one output)\n");
        exit(1);
    }
}

static int sort_by_pkt_pos(const void *j1, const void *j2)
{
    json_t *const *jso1;
    json_t *const *jso2;
    size_t i1;
    size_t i2;

    jso1 = (json_t *const *) j1;
    jso2 = (json_t *const *) j2;
    if ( !*jso1 && !*jso2 )
        return 0;
    if ( !*jso1 )
        return -1;
    if ( !*jso2 )
        return 1;

    i1 = json_int_val(json_object_get(*jso1, "pkt_pos"));
    i2 = json_int_val(json_object_get(*jso2, "pkt_pos"));

    return i1 - i2;
}

static void fff_close_files(FFFile *fff)
{
    if ( fff->s_buf != NULL )
        av_free(fff->s_buf);

    if ( fff->jf != NULL )
        close_ffedit_json_file(fff->jf, fff);

    /* write glitched file if needed */
    if ( fff->ectx != NULL )
    {
        ffe_output_flush(fff->ectx);
        ffe_output_freep(&fff->ectx);
    }

    /* close input file */
    for ( size_t i = 0; i < fff->nb_dctxs; i++ )
        avcodec_free_context(&fff->dctxs[i]);
    if ( fff->fctx != NULL )
        avformat_close_input(&fff->fctx);
}

static int fff_terminate(FFFile *fff)
{
    int fret = fff->fret;
    fff_close_files(fff);
    if ( fff->jf != NULL )
    {
        av_freep(&fff->jf->jstreams);
        av_freep(&fff->jf->jstframes);
        av_freep(&fff->jf);
    }
    for ( size_t i = 0; i < fff->nb_dctxs; i++ )
        av_freep(&fff->dctxs[i]);
    av_freep(&fff->dctxs);
    av_freep(&fff->decs);
    av_freep(&fff);
    return fret;
}

static FFFile *fff_open_files(
        const char *o_fname,
        const char *i_fname,
        const char *e_fname,
        const char *a_fname,
        const char *s_fname,
        int *_selected_features)
{
    FFFile *fff = NULL;
    int ret;

    fff = av_mallocz(sizeof(FFFile));
    fff->fret = -1;
    fff->is_exporting = (e_fname != NULL);
    fff->is_applying = (a_fname != NULL);
    if ( s_fname != NULL )
    {
        fff->is_applying_script = (o_fname != NULL);
        fff->is_exporting_script = !fff->is_applying_script;

        if ( fff->is_applying_script )
        {
            fff->s_fname = s_fname;
            fff->s_buf = read_file(s_fname, &fff->s_size);
            if ( fff->s_buf == NULL )
            {
                av_log(NULL, AV_LOG_FATAL,
                       "Could not open script file %s\n", s_fname);
                exit(1);
            }
        }
    }

    fff->fctx = avformat_alloc_context();

    /* open output file for transplication if needed */
    if ( o_fname != NULL && strcmp(o_fname, "-") == 0 )
        o_fname = "pipe:";
    if ( o_fname != NULL && ffe_output_open(&fff->ectx, fff->fctx, o_fname) < 0 )
        goto the_end;

    /* open input file */
    if ( strcmp(i_fname, "-") == 0 )
        i_fname = "pipe:";
    ret = avformat_open_input(&fff->fctx, i_fname, NULL, NULL);
    if ( ret < 0 )
    {
        av_log(NULL, AV_LOG_FATAL, "Could not open input file '%s'\n", i_fname);
        goto the_end;
    }

    ret = avformat_find_stream_info(fff->fctx, 0);
    if ( ret < 0 )
    {
        av_log(NULL, AV_LOG_FATAL,
               "Failed to retrieve input stream information\n");
        goto the_end;
    }

    av_dump_format(fff->fctx, 0, i_fname, 0);

    if ( (fff->fctx->iformat->flags & AVFMT_FFEDIT_BITSTREAM) == 0 )
    {
        av_log(NULL, AV_LOG_FATAL, "Format '%s' not supported by ffedit.\n",
               fff->fctx->iformat->long_name);
        goto the_end;
    }

    for ( size_t i = 0; i < fff->fctx->nb_streams; i++ )
    {
        GROW_ARRAY(fff->decs, fff->nb_decs);

        fff->decs[i] = avcodec_find_decoder(fff->fctx->streams[i]->codecpar->codec_id);
        if ( fff->decs[i] == NULL )
            av_log(NULL, AV_LOG_ERROR,
                   "Failed to find decoder for stream %zu. Skipping\n", i);
    }

    if ( fff->is_exporting )
    {
        fff->jf = prepare_ffedit_json_file(e_fname, i_fname, _selected_features);
        if ( fff->jf == NULL )
            goto the_end;
        for ( size_t i = 0; i < fff->fctx->nb_streams; i++ )
            add_stream_to_ffedit_json_file(fff->jf, i);
        json_dynamic_array_done(&fff->jf->jctxs[0], fff->jf->jstreams0);
    }
    else if ( fff->is_applying )
    {
        fff->jf = read_ffedit_json_file(a_fname);
    }

    fff->fret = 0;

the_end:
    if ( fff->fret != 0 )
        av_free(fff);

    return fff;
}

static int fff_processing_needed(FFFile *fff)
{
    if ( fff->ectx != NULL
      || fff->is_exporting
      || fff->is_exporting_script )
    {
        return 1;
    }
    return 0;
}

static void fff_open_decoders(FFFile *fff)
{
    fff->fret = -1;

    for ( size_t i = 0; i < fff->nb_decs; i++ )
    {
        AVCodec *decoder = fff->decs[i];
        AVDictionary *opts = NULL;
        AVCodecContext *dctx;
        int ret;

        GROW_ARRAY(fff->dctxs, fff->nb_dctxs);

        if ( decoder == NULL )
            continue;

        if ( (decoder->capabilities & AV_CODEC_CAP_FFEDIT_BITSTREAM) == 0 )
        {
            av_log(NULL, AV_LOG_ERROR,
                   "Codec '%s' not supported by ffedit. Skipping\n",
                   decoder->long_name);
            continue;
        }

        fff->dctxs[i] = avcodec_alloc_context3(decoder);
        dctx = fff->dctxs[i];
        avcodec_parameters_to_context(dctx, fff->fctx->streams[i]->codecpar);

        /* enable threads if ffedit supports it for this codec */
        if ( threads != NULL )
            av_dict_set(&opts, "threads", threads, 0);
        else if ( (decoder->capabilities & AV_CODEC_CAP_FFEDIT_SLICE_THREADS) != 0 )
            av_dict_set(&opts, "threads", "auto", 0);

        /* disable frame-based multithreading. it causes all sorts of
         * hard-to-debug issues (many seen with mpeg4). */
        av_dict_set(&opts, "thread_type", "slice", 0);

        /* select features */
        for ( size_t j = 0; j < FFEDIT_FEAT_LAST; j++ )
        {
            if ( selected_features[j] != 0
              && (selected_features_idx[j] == i
               || selected_features_idx[j] == -1) )
            {
                if ( fff->is_exporting || fff->is_exporting_script )
                    dctx->ffedit_export |= (1 << j);
                if ( fff->is_applying || fff->is_applying_script )
                {
                    dctx->ffedit_import |= (1 << j);
                    dctx->ffedit_apply |= (1 << j);
                }
            }
        }
        /* enable bitstream transplication */
        if ( fff->ectx != NULL )
            dctx->ffedit_apply |= (1 << FFEDIT_FEAT_LAST);

        ret = avcodec_open2(dctx, decoder, &opts);
        av_dict_free(&opts);
        if ( ret < 0 )
        {
            av_log(NULL, AV_LOG_ERROR,
                   "Failed to open decoder for '%s'. Skipping\n",
                   decoder->long_name);
            continue;
        }

        fff->fret = 0;
    }
}

static int ffedit_decode(
        FFFile *fff,
        AVCodecContext *dctx,
        AVFrame *iframe,
        AVPacket *ipkt,
        int stream_index)
{
    int ret;

    ret = avcodec_send_packet(dctx, ipkt);
    if ( ret < 0 )
    {
        av_log(NULL, AV_LOG_FATAL, "send_packet() failed\n");
        return ret;
    }

    while ( ret >= 0 )
    {
        ret = avcodec_receive_frame(dctx, iframe);
        if ( ret == AVERROR(EAGAIN) || ret == AVERROR_EOF )
            return 0;
        if ( ret < 0 )
        {
            av_log(NULL, AV_LOG_FATAL, "receive_frame() failed\n");
            return ret;
        }

        if ( fff->ectx != NULL && dctx->ffe_xp_packets )
        {
            for ( size_t i = 0; i < dctx->nb_ffe_xp_packets; i++ )
            {
                FFEditTransplicatePacket *opkt = &dctx->ffe_xp_packets[i];
                ffe_output_packet(fff->ectx,
                                  opkt->i_pos,
                                  opkt->i_size,
                                  opkt->data,
                                  opkt->o_size);
                av_freep(&opkt->data);
            }
            av_freep(&dctx->ffe_xp_packets);
            dctx->nb_ffe_xp_packets = 0;
        }

        if ( fff->is_exporting )
        {
            int jctx_idx = fff->jf->nb_jctxs;
            add_frame_to_ffedit_json_file(fff->jf, iframe, stream_index);
            GROW_ARRAY(fff->jf->jctxs, fff->jf->nb_jctxs);
            fff->jf->jctxs[jctx_idx] = *(json_ctx_t *) iframe->jctx;
            av_freep(&iframe->jctx);
        }
        else if ( fff->is_exporting_script )
        {
            add_frame_to_ffedit_json_queue(fff->jq_in, iframe);
        }

        fff->frame_number++;
    }

    return 0;
}

static void print_report(
        FFFile *fff,
        int64_t timer_start,
        int64_t pts,
        int is_last_report)
{
    static int64_t last_time = -1;
    float t;
    float fps = 0;
    int64_t total_size = -1;
    double speed = -1;

    int64_t cur_time = av_gettime_relative();

    AVBPrint buf;
    const char end_char = is_last_report ? '\n' : '\r';

    /* print every 500ms and at the end */
    if ( !is_last_report )
    {
        if ( last_time == -1 )
        {
            last_time = cur_time;
            return;
        }
        if ( (cur_time - last_time) < 500000 )
            return;
        last_time = cur_time;
    }

    /* init buffer */
    av_bprint_init(&buf, 0, AV_BPRINT_SIZE_AUTOMATIC);

    t = (cur_time-timer_start) / 1000000.0;

    /* frame fps */
    if ( t > 1 )
        fps = fff->frame_number / t;
    av_bprintf(&buf, "frame=%5d fps=%3.*f ", fff->frame_number, fps < 9.95, fps);
    if ( is_last_report )
        av_bprintf(&buf, "L");

    /* output size */
    if ( fff->ectx != NULL )
        total_size = fff->ectx->last_file_size + fff->ectx->file_size_delta;
    if ( total_size < 0 )
        av_bprintf(&buf, "size=N/A ");
    else
        av_bprintf(&buf, "size=%8.0fkB ", total_size / 1024.0);

    /* pts */
    if ( pts == AV_NOPTS_VALUE )
    {
        av_bprintf(&buf, "time=N/A ");
    }
    else
    {
        const char *hours_sign = (pts < 0) ? "-" : "";
        int secs = FFABS(pts) / AV_TIME_BASE;
        int us = FFABS(pts) % AV_TIME_BASE;
        int mins = secs / 60;
        int hours = mins / 60;
        secs %= 60;
        mins %= 60;
        av_bprintf(&buf, "time=%s%02d:%02d:%02d.%02d ",
                   hours_sign, hours, mins, secs, (100 * us) / AV_TIME_BASE);
    }

    /* speed */
    if ( t != 0.0 && pts != AV_NOPTS_VALUE )
        speed = (double) pts / AV_TIME_BASE / t;
    if ( speed < 0 )
        av_bprintf(&buf, " speed=N/A");
    else
        av_bprintf(&buf, " speed=%4.3gx", speed);

    /* print string */
    if ( AV_LOG_INFO > av_log_get_level() )
        fprintf(stderr, "%s    %c", buf.str, end_char);
    else
        av_log(NULL, AV_LOG_INFO, "%s    %c", buf.str, end_char);
    fflush(stderr);

    /* finalize buffer */
    av_bprint_finalize(&buf, NULL);
}

static void fff_transplicate(FFFile *fff)
{
    AVPacket *ipkt = av_packet_alloc();
    AVFrame *iframe = av_frame_alloc();
    int64_t last_pts = AV_NOPTS_VALUE;
    int64_t timer_start;
    int print_stats = !fff->is_exporting_script;

    fff->fret = -1;

    if ( fff->is_applying )
        reset_frames_idx_ffedit_json_file(fff->jf);

    if ( print_stats )
        timer_start = av_gettime_relative();

    while ( 42 )
    {
        AVCodecContext *dctx;
        int stream_index;
        int ret;

        ret = av_read_frame(fff->fctx, ipkt);
        if ( ret < 0 ) // EOF or TODO error
            break;

        stream_index = ipkt->stream_index;

        dctx = fff->dctxs[stream_index];
        if ( dctx == NULL )
            continue;

        memset(ipkt->ffedit_sd, 0x00, sizeof(ipkt->ffedit_sd));
        if ( fff->is_applying )
            get_from_ffedit_json_file(fff->jf, ipkt, stream_index);
        else if ( fff->is_applying_script )
            get_from_ffedit_json_queue(fff->jq_out, ipkt);

        ret = ffedit_decode(fff, dctx, iframe, ipkt, stream_index);
        if ( ret < 0 )
        {
            av_log(NULL, AV_LOG_FATAL, "ffedit_decode() failed\n");
            goto the_end;
        }

        if ( fff->is_applying_script )
        {
            json_ctx_free(ipkt->jctx);
            av_freep(&ipkt->jctx);
        }

        if ( print_stats )
        {
            int64_t pts = ipkt->pts;
            if ( pts == AV_NOPTS_VALUE )
                pts = ipkt->dts;
            if ( pts != AV_NOPTS_VALUE )
                pts = av_rescale_q(pts, fff->fctx->streams[stream_index]->time_base, AV_TIME_BASE_Q);
            if ( last_pts == AV_NOPTS_VALUE || last_pts < pts )
                last_pts = pts;
            print_report(fff, timer_start, last_pts, 0);
        }
    }

    for ( size_t i = 0; i < fff->fctx->nb_streams; i++ )
        if ( fff->dctxs[i] != NULL )
            ffedit_decode(fff, fff->dctxs[i], iframe, NULL, i);

    if ( print_stats )
        print_report(fff, timer_start, last_pts, 1);

    fff->fret = 0;

the_end:
    av_frame_free(&iframe);
    av_packet_free(&ipkt);
}

static void fff_print_features(FFFile *fff)
{
    for ( size_t i = 0; i < fff->nb_decs; i++ )
    {
        AVCodec *decoder = fff->decs[i];
        if ( decoder != NULL )
        {
            printf("\nFFEdit support for codec '%s':\n", decoder->long_name);
            for ( size_t j = 0; j < FFEDIT_FEAT_LAST; j++ )
                if ( (decoder->ffedit_features & (1 << j)) != 0 )
                    printf("\t[%-10s]: %s\n", ffe_feat_to_str(j), ffe_feat_desc(j));
        }
    }
    fff->fret = 0;
}

static void *fff_func(void *arg)
{
    FFFile *fff = (FFFile *) arg;
    if ( !fff_processing_needed(fff) )
    {
        /* no processing needed. just print glitching features */
        fff_print_features(fff);
    }
    else
    {
        /* open all possible decoders */
        /* TODO allow selecting streams */
        fff_open_decoders(fff);
        if ( fff->fret < 0 )
        {
            av_log(NULL, AV_LOG_FATAL, "Error opening decoders.\n");
            return NULL;
        }

        /* do the salmon dance */
        fff_transplicate(fff);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    FFFile *fff = NULL;
    FFFile *fscript = NULL;
    pthread_t script_input_thread;
    pthread_t script_thread;
    int fret = -1;

    hack_musl_pthread_stack_size();

    /* init stuff */
    av_log_set_flags(AV_LOG_SKIP_REPEATED);
    parse_loglevel(argc, argv, options);

    avformat_network_init();

    show_banner(argc, argv, options);

    parse_options(NULL, argc, argv, options, opt_filenames);

    /* check options consistency */
    if ( (export_fname != NULL) && (apply_fname != NULL) )
    {
        av_log(NULL, AV_LOG_FATAL, "Only one of -e or -a may be used!\n");
        goto the_end;
    }
    if ( (script_fname != NULL) )
    {
        if ( (export_fname != NULL) || (apply_fname != NULL) )
        {
            av_log(NULL, AV_LOG_FATAL, "Only one of -s or -e and -a may be used!\n");
            goto the_end;
        }
        if ( (output_fname == NULL) )
        {
            av_log(NULL, AV_LOG_FATAL, "Output file required when using scripts!\n");
            goto the_end;
        }
        script_is_js = av_match_ext(script_fname, "js");
        script_is_py = av_match_ext(script_fname, "py");
        if ( !script_is_js && !script_is_py )
        {
            av_log(NULL, AV_LOG_FATAL, "Only JavaScript (\".js\") or Python (\".py\") scripts supported!\n");
            goto the_end;
        }
    }

    /* there must be one input */
    if ( !input_fname )
    {
        av_log(NULL, AV_LOG_FATAL, "No input file specified!\n");
        goto the_end;
    }

    /* TODO review this */
    /* TODO cannot import/apply both mv and mv_delta at the same time */
    if ( features_selected == 0 )
    {
        /* select all features by default */
        for ( size_t i = 0; i < FFEDIT_FEAT_LAST; i++ )
        {
            if ( ffe_default_feat(i) )
            {
                selected_features[i] = 1;
                selected_features_idx[i] = -1;
            }
        }
        features_selected = 1;
    }

    /* open files for main thread */
    fff = fff_open_files(output_fname, input_fname, export_fname, apply_fname, script_fname, selected_features);
    if ( fff == NULL )
        goto the_end;

    /* start script thread if needed */
    if ( script_fname != NULL )
    {
        fscript = fff_open_files(NULL, input_fname, NULL, NULL, script_fname, selected_features);
        if ( fscript == NULL )
            goto the_end;

        /* initialize queues */
        fscript->jq_in = av_mallocz(sizeof(JSONQueue));
        pthread_mutex_init(&fscript->jq_in->mutex, NULL);
        pthread_cond_init(&fscript->jq_in->cond_empty, NULL);
        pthread_cond_init(&fscript->jq_in->cond_full, NULL);
        fff->jq_in = fscript->jq_in;
        fscript->jq_out = av_mallocz(sizeof(JSONQueue));
        pthread_mutex_init(&fscript->jq_out->mutex, NULL);
        pthread_cond_init(&fscript->jq_out->cond_empty, NULL);
        pthread_cond_init(&fscript->jq_out->cond_full, NULL);
        fff->jq_out = fscript->jq_out;

        fff->s_init = 0;
        pthread_mutex_init(&fff->s_mutex, NULL);
        pthread_cond_init(&fff->s_cond, NULL);

        if ( script_is_js )
            pthread_create(&script_thread, NULL, quickjs_func, fff);
        else /* if ( script_is_py ) */
            pthread_create(&script_thread, NULL, python_func, fff);
        pthread_create(&script_input_thread, NULL, fff_func, fscript);

        /* wait for script setup */
        pthread_mutex_lock(&fff->s_mutex);
        while ( !fff->s_init )
            pthread_cond_wait(&fff->s_cond, &fff->s_mutex);
        pthread_mutex_unlock(&fff->s_mutex);
    }

    fff_func(fff);

    if ( fscript != NULL )
    {
        /* send poison pill */
        AVFrame iframe;
        iframe.pkt_pos = -1;
        iframe.jctx = NULL;
        add_frame_to_ffedit_json_queue(fscript->jq_in, &iframe);

        /* wait for threads */
        pthread_join(script_thread, NULL);
        pthread_join(script_input_thread, NULL);

        /* destroy queues */
        pthread_mutex_destroy(&fscript->jq_in->mutex);
        pthread_cond_destroy(&fscript->jq_in->cond_empty);
        pthread_cond_destroy(&fscript->jq_in->cond_full);
        av_freep(&fscript->jq_in);
        pthread_mutex_destroy(&fscript->jq_out->mutex);
        pthread_cond_destroy(&fscript->jq_out->cond_empty);
        pthread_cond_destroy(&fscript->jq_out->cond_full);
        av_freep(&fscript->jq_out);

        pthread_mutex_destroy(&fff->s_mutex);
        pthread_cond_destroy(&fff->s_cond);
    }

the_end:
    if ( fff != NULL )
        fret = fff_terminate(fff);
    if ( fscript != NULL )
        fret = fff_terminate(fscript);

    av_freep(&input_fname);
    av_freep(&output_fname);
    av_freep(&apply_fname);
    av_freep(&export_fname);
    av_freep(&threads);

    return fret;
}
