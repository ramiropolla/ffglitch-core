
#include "libavutil/json.h"

#include "config.h"

#include "libavutil/opt.h"
#include "libavutil/avstring.h"
#include "libavutil/ffversion.h"
#include "libavutil/sha.h"
#include "libavformat/ffedit.h"
#include "libavcodec/ffedit.h"

#include "cmdutils.h"

const char program_name[] = "ffedit";
const int program_birth_year = 2000; // FFmpeg, that is

/* only one input and one optional output file */
static const char *input_filename;
static const char *output_filename;

/* don't you just love globals? */
static AVFormatContext *fctx;
static AVCodec **decs;
static int    nb_decs;
static AVCodecContext **dctxs;
static int           nb_dctxs;

static int is_exporting;
static int is_applying; // implies importing

static const char *export_fname;
static FILE *export_fp;
static const char *apply_fname;
static int file_overwrite;
static int test_mode;

static json_ctx_t jctx;
static json_t *jroot;
static json_t *jstreams0;
static json_t **jstreams;
static int   nb_jstreams;
static json_t **jstframes;
static int   nb_jstframes;

/* TODO use avformat_match_stream_specifier */
static int selected_features[FFEDIT_FEAT_LAST];
static int selected_features_idx[FFEDIT_FEAT_LAST];
static int features_selected;
static int opt_feature(void *optctx, const char *opt, const char *arg);

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

    buf = malloc(size+1);
    buf[size] = '\0';

    fread(buf, size, 1, fp);
    fclose(fp);

    *psize = size;

    return buf;
}

static const OptionDef options[] = {
    CMDUTILS_COMMON_OPTIONS
    { "a", HAS_ARG | OPT_STRING, { &apply_fname }, "apply data", "json file" },
    { "e", HAS_ARG | OPT_STRING, { &export_fname }, "export data", "json file" },
    { "f", HAS_ARG, { .func_arg = opt_feature }, "select feature (optionally specify stream with feat:#)", "feature" },
    { "t", OPT_BOOL, { &test_mode }, "test mode (bitexact output)" },
    { "y", OPT_BOOL, { &file_overwrite }, "overwrite output files" },
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
    if ( input_filename == NULL )
    {
        input_filename = filename;
    }
    else if ( output_filename == NULL )
    {
        output_filename = filename;
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

static void close_files(FFEditOutputContext *ectx)
{
    /* write output json file if needed */
    if ( export_fp != NULL && jstframes != NULL )
    {
        for ( size_t i = 0; i < fctx->nb_streams; i++ )
            json_array_sort(jstframes[i], sort_by_pkt_pos);

        json_fputs(export_fp, jroot);
        fclose(export_fp);
    }

    /* free json-c objects */
    if ( jroot != NULL )
        json_ctx_free(&jctx);

    /* write glitched file if needed */
    if ( ectx != NULL )
    {
        ffe_output_flush(ectx);
        ffe_output_freep(&ectx);
    }

    /* close input file */
    for ( size_t i = 0; i < nb_dctxs; i++ )
        avcodec_free_context(&dctxs[i]);
    if ( fctx != NULL )
        avformat_close_input(&fctx);
}

static int open_files(
        FFEditOutputContext **pectx,
        const char *o_fname,
        const char *i_fname)
{
    int fret = -1;
    int ret;

    fctx = avformat_alloc_context();

    /* open output file for transplication if needed */
    if ( o_fname != NULL && strcmp(o_fname, "-") == 0 )
        o_fname = "pipe:";
    if ( o_fname != NULL && ffe_output_open(pectx, fctx, o_fname) < 0 )
        goto the_end;

    /* open input file */
    if ( strcmp(i_fname, "-") == 0 )
        i_fname = "pipe:";
    ret = avformat_open_input(&fctx, i_fname, NULL, NULL);
    if ( ret < 0 )
    {
        av_log(NULL, AV_LOG_FATAL, "Could not open input file '%s'\n", i_fname);
        goto the_end;
    }

    ret = avformat_find_stream_info(fctx, 0);
    if ( ret < 0 )
    {
        av_log(NULL, AV_LOG_FATAL,
               "Failed to retrieve input stream information\n");
        goto the_end;
    }

    av_dump_format(fctx, 0, i_fname, 0);

    if ( (fctx->iformat->flags & AVFMT_FFEDIT_BITSTREAM) == 0 )
    {
        av_log(NULL, AV_LOG_FATAL, "Format '%s' not supported by ffedit.\n",
               fctx->iformat->long_name);
        goto the_end;
    }

    for ( size_t i = 0; i < fctx->nb_streams; i++ )
    {
        GROW_ARRAY(decs, nb_decs);

        decs[i] = avcodec_find_decoder(fctx->streams[i]->codecpar->codec_id);
        if ( decs[i] == NULL )
            av_log(NULL, AV_LOG_ERROR,
                   "Failed to find decoder for stream %zu. Skipping\n", i);

        if ( jstreams0 != NULL && is_exporting )
        {
            json_t *jstream = json_object_new(&jctx);
            json_t *jframes = json_array_new(&jctx);

            GROW_ARRAY(jstreams, nb_jstreams);
            GROW_ARRAY(jstframes, nb_jstframes);

            jstreams[i] = jstream;
            jstframes[i] = jframes;

            json_object_add(jstream, "frames", jframes);
            json_array_add(jstreams0, jstream);
        }
    }

    fret = 0;

the_end:
    return fret;
}

static int processing_needed(void)
{
    if ( output_filename != NULL
      || is_exporting )
    {
        return 1;
    }
    return 0;
}

static int open_decoders(FFEditOutputContext *ectx)
{
    int fret = -1;

    for ( size_t i = 0; i < nb_decs; i++ )
    {
        AVCodec *decoder = decs[i];
        AVDictionary *opts = NULL;
        AVCodecContext *dctx;
        int ret;

        GROW_ARRAY(dctxs, nb_dctxs);

        if ( decoder == NULL )
            continue;

        if ( (decoder->capabilities & AV_CODEC_CAP_FFEDIT_BITSTREAM) == 0 )
        {
            av_log(NULL, AV_LOG_ERROR,
                   "Codec '%s' not supported by ffedit. Skipping\n",
                   decoder->long_name);
            continue;
        }

        dctxs[i] = avcodec_alloc_context3(decoder);
        dctx = dctxs[i];
        avcodec_parameters_to_context(dctx, fctx->streams[i]->codecpar);

        av_dict_set(&opts, "thread_type", "frame", 0);
        /* select features */
        for ( size_t j = 0; j < FFEDIT_FEAT_LAST; j++ )
        {
            if ( selected_features[j] != 0
              && (selected_features_idx[j] == i
               || selected_features_idx[j] == -1) )
            {
                if ( is_exporting )
                    dctx->ffedit_export |= (1 << j);
                if ( is_applying )
                {
                    dctx->ffedit_import |= (1 << j);
                    dctx->ffedit_apply |= (1 << j);
                }
            }
        }
        /* enable bitstream transplication */
        if ( ectx != NULL )
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

        fret = 0;
    }

    return fret;
}

static int ffedit_decode(
        FFEditOutputContext *ectx,
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

    if ( ectx != NULL && dctx->ffedit_out != NULL )
    {
        ffe_output_packet(ectx, ipkt->pos, ipkt->size, dctx->ffedit_out, dctx->ffedit_out_size);
        av_freep(&dctx->ffedit_out);
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

        if ( is_exporting )
        {
            json_t *jframes = jstframes[stream_index];
            json_t *jframe = json_object_new(&jctx);
            json_t *jpkt_pos = json_int_new(&jctx, iframe->pkt_pos);
            json_t *jpts = json_int_new(&jctx, iframe->pts);
            json_t *jdts = json_int_new(&jctx, iframe->pkt_dts);

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

            json_array_add(jframes, jframe);
        }
    }

    return 0;
}

static int transplicate(FFEditOutputContext *ectx)
{
    int fret = -1;

    AVPacket *ipkt = av_packet_alloc();
    AVFrame *iframe = av_frame_alloc();

    size_t *frames_idx = NULL;

    frames_idx = av_calloc(nb_jstframes, sizeof(size_t));

    while ( 42 )
    {
        AVCodecContext *dctx;
        int stream_index;
        int ret;

        ret = av_read_frame(fctx, ipkt);
        if ( ret < 0 ) // EOF or TODO error
            break;

        stream_index = ipkt->stream_index;

        dctx = dctxs[stream_index];
        if ( dctx == NULL )
            continue;

        memset(ipkt->ffedit_sd, 0x00, sizeof(ipkt->ffedit_sd));
        ipkt->jctx = &jctx;
        if ( is_applying )
        {
            size_t idx = frames_idx[stream_index];
            json_t *jframes = jstframes[stream_index];
            json_t *jframe = json_array_get(jframes, idx);

            for ( size_t i = 0; i < FFEDIT_FEAT_LAST; i++ )
            {
                const char *key = ffe_feat_to_str(i);
                ipkt->ffedit_sd[i] = json_object_get(jframe, key);
            }

            frames_idx[stream_index]++;
        }

        ret = ffedit_decode(ectx, dctx, iframe, ipkt, stream_index);
        if ( ret < 0 )
        {
            av_log(NULL, AV_LOG_FATAL, "ffedit_decode() failed\n");
            goto the_end;
        }
    }

    for ( size_t i = 0 ; i < fctx->nb_streams; i++ )
        ffedit_decode(ectx, dctxs[i], iframe, NULL, i);

    fret = 0;

the_end:
    av_frame_free(&iframe);
    av_packet_free(&ipkt);
    if ( frames_idx != NULL )
        av_freep(&frames_idx);

    return fret;
}

static void print_features(void)
{
    for ( size_t i = 0; i < nb_decs; i++ )
    {
        AVCodec *decoder = decs[i];
        if ( decoder != NULL )
        {
            printf("\nFFEdit support for codec '%s':\n", decoder->long_name);
            for ( size_t j = 0; j < FFEDIT_FEAT_LAST; j++ )
                if ( (decoder->ffedit_features & (1 << j)) != 0 )
                    printf("\t[%-10s]: %s\n", ffe_feat_to_str(j), ffe_feat_desc(j));
        }
    }
}

int main(int argc, char *argv[])
{
    FFEditOutputContext *ectx = NULL;
    int fret = -1;
    int ret;

    hack_musl_pthread_stack_size();

    /* init stuff */
    av_log_set_flags(AV_LOG_SKIP_REPEATED);
    parse_loglevel(argc, argv, options);

    avformat_network_init();

    show_banner(argc, argv, options);

    parse_options(NULL, argc, argv, options, opt_filenames);

    /* check options consistency */
    is_exporting = (export_fname != NULL);
    is_applying = (apply_fname != NULL);
    if ( is_exporting && is_applying )
    {
        av_log(NULL, AV_LOG_FATAL, "Only one of -e or -a may be used!\n");
        goto the_end;
    }

    /* there must be one input */
    if ( !input_filename )
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

    if ( is_exporting )
    {
        json_t *jfname = NULL;
        json_t *jfeatures = NULL;
        size_t size;
        char *buf;

        // TODO check if file exists before overwritting
        export_fp = fopen(export_fname, "w");
        if ( export_fp == NULL )
        {
            av_log(NULL, AV_LOG_FATAL,
                   "Could not open json file %s\n", export_fname);
            goto the_end;
        }

        json_ctx_start(&jctx);
        jroot = json_object_new(&jctx);

        if ( test_mode == 0 )
        {
            json_t *jversion = json_string_new(&jctx, FFMPEG_VERSION);
            json_object_add(jroot, "ffedit_version", jversion);
        }

        jfname = json_string_new(&jctx, av_basename(input_filename));
        json_object_add(jroot, "filename", jfname);

        buf = read_file(input_filename, &size);
        if ( buf != NULL )
        {
            json_t *jshasum = NULL;
            struct AVSHA *sha;
            char shasumstr[41];
            uint8_t shasum[20];

            sha = av_sha_alloc();
            av_sha_init(sha, 160);
            av_sha_update(sha, buf, size);
            av_sha_final(sha, shasum);
            av_free(sha);

            free(buf);

            for ( size_t i = 0; i < 20; i++ )
            {
                shasumstr[i*2+0] = nibble2char(shasum[i] >> 4);
                shasumstr[i*2+1] = nibble2char(shasum[i] & 0xF);
            }
            shasumstr[40] = '\0';

            jshasum = json_string_new(&jctx, shasumstr);
            json_object_add(jroot, "sha1sum", jshasum);
        }

        jfeatures = json_array_new(&jctx);
        for ( size_t i = 0; i < FFEDIT_FEAT_LAST; i++ )
        {
            if ( selected_features[i] )
            {
                const char *feat_str = ffe_feat_to_str(i);
                json_t *jfeature = json_string_new(&jctx, feat_str);
                json_array_add(jfeatures, jfeature);
            }
        }
        json_array_done(&jctx, jfeatures);
        json_object_add(jroot, "features", jfeatures);

        jstreams0 = json_array_new(&jctx);
        json_object_add(jroot, "streams", jstreams0);
    }
    else if ( is_applying )
    {
        size_t size;
        char *buf;

        buf = read_file(apply_fname, &size);
        if ( buf == NULL )
        {
            av_log(NULL, AV_LOG_FATAL,
                   "Could not open json file %s\n", apply_fname);
            exit(1);
        }

        json_ctx_start(&jctx);
        jroot = json_parse(&jctx, buf);
        if ( jroot == NULL )
        {
            av_log(NULL, AV_LOG_FATAL, "json_parse error: %s\n",
                   json_parse_error(&jctx));
            exit(1);
        }
        jstreams0 = json_object_get(jroot, "streams");

        size = json_array_length(jstreams0);
        for ( size_t i = 0; i < size; i++ )
        {
            GROW_ARRAY(jstreams, nb_jstreams);
            GROW_ARRAY(jstframes, nb_jstframes);
            jstreams[i] = json_array_get(jstreams0, i);
            jstframes[i] = json_object_get(jstreams[i], "frames");
        }

        free(buf);
    }

    ret = open_files(&ectx, output_filename, input_filename);
    if ( ret < 0 )
        goto the_end;

    if ( !processing_needed() )
    {
        /* no processing needed. just print glitching features */
        print_features();
        fret = 0;
    }
    else
    {
        /* open all possible decoders */
        /* TODO allow selecting streams */
        ret = open_decoders(ectx);
        if ( ret < 0 )
        {
            av_log(NULL, AV_LOG_FATAL, "Error opening decoders.\n");
            goto the_end;
        }

        /* do the salmon dance */
        fret = transplicate(ectx);
    }

the_end:
    close_files(ectx);

    return fret;
}
