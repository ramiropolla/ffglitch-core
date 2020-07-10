
#include "libavutil/json.h"

#include "config.h"

#include "libavutil/opt.h"
#include "libavutil/avstring.h"
#include "libavutil/ffversion.h"
#include "libavutil/sha.h"
#include "libavutil/thread.h"
#include "libavformat/ffedit.h"
#include "libavcodec/ffedit.h"

#include "cmdutils.h"

const char program_name[] = "ffedit";
const int program_birth_year = 2000; // FFmpeg, that is

/* command-line options */
static const char *input_fname;
static const char *output_fname;
static const char *apply_fname;
static const char *export_fname;
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
    AVFormatContext *fctx;
    AVCodec **decs;
    int    nb_decs;
    AVCodecContext **dctxs;
    int           nb_dctxs;

    int is_exporting;
    int is_applying; // implies importing

    FFEditOutputContext *ectx;

    JSONFile *jf;

    int fret;
} FFFile;

static int opt_feature(void *optctx, const char *opt, const char *arg);

/* forward declarations */
static char *read_file(const char *fname, size_t *psize);
static void sha1sum(char *shasumstr, const char *buf, size_t size);
static int sort_by_pkt_pos(const void *j1, const void *j2);

static JSONFile *read_ffedit_json_file(const char *_apply_fname)
{
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
    jf->jroot = json_parse(&jf->jctxs[0], buf);
    if ( jf->jroot == NULL )
    {
        av_log(NULL, AV_LOG_FATAL, "json_parse error: %s\n",
               json_parse_error(&jf->jctxs[0]));
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

static json_ctx_t *get_jctx_ffedit_json_file(JSONFile *jf)
{
    return &jf->jctxs[0];
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

    for ( size_t i = 0; i < FFEDIT_FEAT_LAST; i++ )
    {
        const char *key = ffe_feat_to_str(i);
        ipkt->ffedit_sd[i] = json_object_get(jframe, key);
    }
    ipkt->jctx = &jf->jctxs[0];

    jf->frames_idx[stream_index]++;
}

static void close_ffedit_json_file(JSONFile *jf, FFFile *fff)
{
    /* write output json file if needed */
    if ( jf->export_fp != NULL && jf->jstframes != NULL )
    {
        /* close json_ctx_t dynamic objects */
        for ( size_t i = 0; i < fff->fctx->nb_streams; i++ )
        {
            for ( size_t j = 0; j < jf->nb_jstreams; j++ )
                json_object_done(&jf->jctxs[0], jf->jstreams[i]);
            json_dynamic_array_done(&jf->jctxs[0], jf->jstframes[i]);
        }
        json_object_done(&jf->jctxs[0], jf->jroot);

        for ( size_t i = 0; i < fff->fctx->nb_streams; i++ )
            json_array_sort(jf->jstframes[i], sort_by_pkt_pos);

        json_fputs(jf->export_fp, jf->jroot);
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

    strncpy(str_feature, arg, sizeof(str_feature));
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
        int *_selected_features)
{
    FFFile *fff = NULL;
    int ret;

    fff = av_mallocz(sizeof(FFFile));
    fff->fret = -1;
    fff->is_exporting = (e_fname != NULL);
    fff->is_applying = (a_fname != NULL);

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
      || fff->is_exporting )
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
                if ( fff->is_exporting )
                    dctx->ffedit_export |= (1 << j);
                if ( fff->is_applying )
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

        if ( fff->ectx != NULL && dctx->ffedit_out != NULL )
        {
            ffe_output_packet(fff->ectx, dctx->ffedit_in_pos, dctx->ffedit_in_size, dctx->ffedit_out, dctx->ffedit_out_size);
            av_freep(&dctx->ffedit_out);
        }

        if ( fff->is_exporting )
        {
            int jctx_idx = fff->jf->nb_jctxs;
            add_frame_to_ffedit_json_file(fff->jf, iframe, stream_index);
            GROW_ARRAY(fff->jf->jctxs, fff->jf->nb_jctxs);
            fff->jf->jctxs[jctx_idx] = *(json_ctx_t *) iframe->jctx;
            av_freep(&iframe->jctx);
        }
    }

    return 0;
}

static void fff_transplicate(FFFile *fff)
{
    AVPacket *ipkt = av_packet_alloc();
    AVFrame *iframe = av_frame_alloc();

    fff->fret = -1;

    if ( fff->is_applying )
        reset_frames_idx_ffedit_json_file(fff->jf);

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

        ret = ffedit_decode(fff, dctx, iframe, ipkt, stream_index);
        if ( ret < 0 )
        {
            av_log(NULL, AV_LOG_FATAL, "ffedit_decode() failed\n");
            goto the_end;
        }
    }

    for ( size_t i = 0 ; i < fff->fctx->nb_streams; i++ )
        ffedit_decode(fff, fff->dctxs[i], iframe, NULL, i);

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

static void fff_func(void *arg)
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
            return;
        }

        /* do the salmon dance */
        fff_transplicate(fff);
    }
}

int main(int argc, char *argv[])
{
    FFFile *fff = NULL;
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

    /* there must be one input */
    if ( !input_fname )
    {
        av_log(NULL, AV_LOG_FATAL, "No input file specified!\n");
        goto the_end;
    }

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

    fff = fff_open_files(output_fname, input_fname, export_fname, apply_fname, selected_features);
    if ( fff == NULL )
        goto the_end;

    fff_func(fff);

the_end:
    if ( fff != NULL )
        fret = fff_terminate(fff);

    av_freep(&input_fname);
    av_freep(&output_fname);
    av_freep(&apply_fname);
    av_freep(&export_fname);
    av_freep(&threads);

    return fret;
}
