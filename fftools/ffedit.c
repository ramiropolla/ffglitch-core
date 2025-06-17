/*
 * Copyright (c) 2017-2023 Ramiro Polla
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "libavutil/json.h"

#include "config.h"

#include "libavutil/opt.h"
#include "libavutil/avassert.h"
#include "libavutil/avstring.h"
#include "libavutil/bprint.h"
#include "libavutil/ffversion.h"
#include "libavutil/file_open.h"
#include "libavutil/sha.h"
#include "libavutil/time.h"
#include "libavutil/thread.h"
#include "libavformat/ffedit.h"
#include "libavcodec/ffedit.h"

#include "cmdutils.h"
#include "opt_common.h"

/* must come after cmdutils.h because of GROW_ARRAY() */
#include "libavutil/script.h"

const char program_name[] = "ffedit";
const int program_birth_year = 2000; // FFmpeg, that is

extern const void *ffe_class[1];

#define THREAD_TYPE            pthread_t
#define THREAD_RET_TYPE        void *
#define THREAD_CREATE(thread, func, args) do { pthread_create(&thread, NULL, func, args); } while ( 0 )
#define THREAD_JOIN(thread)    do { pthread_join(thread, NULL);       } while ( 0 )
#define THREAD_RET_OK          NULL
#define MUTEX_TYPE             pthread_mutex_t
#define MUTEX_INIT(mutex)      do { pthread_mutex_init(&mutex, NULL); } while ( 0 )
#define MUTEX_LOCK(mutex)      do { pthread_mutex_lock(&mutex);       } while ( 0 )
#define MUTEX_UNLOCK(mutex)    do { pthread_mutex_unlock(&mutex);     } while ( 0 )
#define MUTEX_DESTROY(mutex)   do { pthread_mutex_destroy(&mutex);    } while ( 0 )
#define COND_TYPE              pthread_cond_t
#define COND_INIT(cond)        do { pthread_cond_init(&cond, NULL);   } while ( 0 )
#define COND_SIGNAL(cond)      do { pthread_cond_signal(&cond);       } while ( 0 )
#define COND_WAIT(cond, mutex) do { pthread_cond_wait(&cond, &mutex); } while ( 0 )
#define COND_DESTROY(cond)     do { pthread_cond_destroy(&cond);      } while ( 0 )

#include "libavutil/quickjs/quickjs-libc.h"
#include "ffedit_common.c"

/*********************************************************************/
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
} FFEditJSONFile;

/*********************************************************************/
typedef struct FFEditPacketQueueElement {
    struct FFEditPacketQueueElement *next;
    AVPacket pkt;
} FFEditPacketQueueElement;

typedef struct {
    FFEditPacketQueueElement *first;
    FFEditPacketQueueElement *last;
    size_t nb_elements;
    COND_TYPE cond_empty;
    COND_TYPE cond_full;
    MUTEX_TYPE mutex;
} FFEditPacketQueue;

/*********************************************************************/
typedef struct {
    AVFormatContext *fctx;

    const AVCodec **decs;
    int          nb_decs;

    AVRational *time_bases;
    int      nb_time_bases;

    const AVCodecParameters **pars;
    int                    nb_pars;

    FFEditPacketQueue *pq_out;
    FFEditPacketQueue *pq_out_2;
} FFEditInputContext;

/*********************************************************************/
typedef struct {
    FFEditOutputContext *ectx;

    AVCodecContext **dctxs;
    int           nb_dctxs;

    AVRational *time_bases;
    int      nb_time_bases;

    int is_exporting;
    int is_importing;
    int is_applying;
    int is_exporting_script;
    int is_importing_script;
    int is_applying_script;

    FFEditJSONFile *jf;
    FFEditJSONQueue *jq_in;
    FFEditJSONQueue *jq_out;
    FFEditPacketQueue *pq_in;

    int fret;

    int frame_number;
} FFEditActionContext;

typedef enum {                 // o e a s
    ACTION_PRINT_FEATURES = 0, //
    ACTION_REPLICATE,          // x
    ACTION_EXPORT,             //   x
    ACTION_TRANSPLICATE,       // x   x
    ACTION_SCRIPT,             // ?     x
} FFEditAction;

/*********************************************************************/
/* command-line options */
static const char *apply_fname;
static const char *export_fname;
static int test_mode;
static int file_overwrite;
static const char *threads;

/* forward declarations */
static void sha1sum(char *shasumstr, const char *buf, size_t size);
static int opt_feature(void *optctx, const char *opt, const char *arg);

/*********************************************************************/
static int64_t benchmark_json_parse;
static int64_t benchmark_import;
static int64_t benchmark_json_fputs;
static int64_t benchmark_export;
static int64_t benchmark_transplicate;
static int64_t benchmark_output;

/*********************************************************************/
static FFEditJSONFile *read_ffedit_json_file(const char *_apply_fname)
{
    int64_t t0;
    int64_t t1;

    FFEditJSONFile *jf = NULL;
    size_t size;
    char *buf;

    jf = av_mallocz(sizeof(FFEditJSONFile));

    buf = ff_script_read_file(_apply_fname, &size);
    if ( buf == NULL )
    {
        av_log(ffe_class, AV_LOG_FATAL, "Could not open json file %s\n", _apply_fname);
        exit(1);
    }

    GROW_ARRAY(jf->jctxs, jf->nb_jctxs);
    json_ctx_start(&jf->jctxs[0], 1);

    if ( do_benchmark )
        t0 = av_gettime_relative();

    jf->jroot = json_parse(&jf->jctxs[0], buf);

    if ( do_benchmark )
    {
        t1 = av_gettime_relative();
        benchmark_json_parse = (t1 - t0);
        t0 = t1;
    }

    if ( jf->jroot == NULL )
    {
        json_error_ctx_t jectx;
        json_error_parse(&jectx, buf);
        const char *fname = (strcmp(_apply_fname, "-") == 0) ? "<stdin>" : _apply_fname;
        av_log(ffe_class, AV_LOG_FATAL, "%s:%d:%d: %s\n",
               fname, (int) jectx.line, (int) jectx.offset, jectx.str);
        av_log(ffe_class, AV_LOG_FATAL, "%s:%d:%s\n", fname, (int) jectx.line, jectx.buf);
        av_log(ffe_class, AV_LOG_FATAL, "%s:%d:%s\n", fname, (int) jectx.line, jectx.column);
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

static void reset_frames_idx_ffedit_json_file(FFEditJSONFile *jf)
{
    memset(jf->frames_idx, 0x00, jf->nb_jstframes * sizeof(size_t));
}

static void add_stream_to_ffedit_json_file(
        FFEditJSONFile *jf,
        size_t i,
        const AVCodec *dec)
{
    json_t *jstream = json_object_new(&jf->jctxs[0]);
    json_t *jframes = json_dynamic_array_new(&jf->jctxs[0]);
    json_t *jcodec = json_string_new(&jf->jctxs[0], dec->name);

    GROW_ARRAY(jf->jstreams, jf->nb_jstreams);
    GROW_ARRAY(jf->jstframes, jf->nb_jstframes);

    jf->jstreams[i] = jstream;
    jf->jstframes[i] = jframes;

    json_object_add(jstream, "codec", jcodec);
    json_object_add(jstream, "frames", jframes);
    json_dynamic_array_add(jf->jstreams0, jstream);
}

static FFEditJSONFile *prepare_ffedit_json_file(
        const char *_export_fname,
        const char *_input_fname,
        FFEditInputContext *ffi,
        int *_selected_features)
{
    size_t nb_streams = ffi->nb_decs;
    const AVCodec **decs = ffi->decs;
    FFEditJSONFile *jf = NULL;
    json_t *jfname = NULL;
    json_t *jfeatures = NULL;
    size_t size;
    char *buf;

    jf = av_mallocz(sizeof(FFEditJSONFile));

    if ( strcmp(_export_fname, "-") == 0 )
        jf->export_fp = stdout;
    if ( jf->export_fp == NULL )
    {
        // TODO check if file exists before overwritting
        jf->export_fp = avpriv_fopen_utf8(_export_fname, "w");
    }

    if ( jf->export_fp == NULL )
    {
        av_log(ffe_class, AV_LOG_FATAL, "Could not open json file %s\n", _export_fname);
        exit(1);
    }

    GROW_ARRAY(jf->jctxs, jf->nb_jctxs);
    json_ctx_start(&jf->jctxs[0], 1);
    jf->jroot = json_object_new(&jf->jctxs[0]);

    if ( test_mode == 0 )
    {
        json_t *jversion = json_string_new(&jf->jctxs[0], FFMPEG_VERSION);
        json_object_add(jf->jroot, "ffedit_version", jversion);
    }

    jfname = json_string_new(&jf->jctxs[0], av_basename(_input_fname));
    json_object_add(jf->jroot, "filename", jfname);

    buf = ff_script_read_file(_input_fname, &size);
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

    for ( size_t i = 0; i < nb_streams; i++ )
        add_stream_to_ffedit_json_file(jf, i, decs[i]);

    json_dynamic_array_done(&jf->jctxs[0], jf->jstreams0);

    return jf;
}

/* Adds a packet to FFEditJSONFile.
 * This packet is expected to be filled at some point after it is
 * decoded. In case it is *not* decoded, this means it was skipped by
 * the codec itself. We still need to export it to the JSON file, so
 * that it can be accounted for while applying. */
static void add_packet_to_ffedit_json_file(
        FFEditJSONFile *jf,
        int64_t pkt_pos,
        int stream_index)
{
    json_t *jframes = jf->jstframes[stream_index];
    json_t *jframe = json_object_new(&jf->jctxs[0]);
    json_t *jpkt_pos = json_int_new(&jf->jctxs[0], pkt_pos);

    json_object_add(jframe, "pkt_pos", jpkt_pos);
    /* NOTE: jframe *must* be closed with json_object_done() at some
     *       point. */

    json_dynamic_array_add(jframes, jframe);
}

static json_t *match_packet_and_frame_in_ffedit_json_file(
        FFEditJSONFile *jf,
        int64_t pkt_pos,
        int stream_index)
{
    json_t *jframes = jf->jstframes[stream_index];
    size_t len = json_array_length(jframes);

    /* search backwards through jframes, since the frame we're looking
     * for is very likely the last one or near the end. */
    for ( size_t i = 0; i < len; i++ )
    {
        size_t idx = len - i - 1;
        json_t *jframe = json_array_get(jframes, idx);
        /* NOTE: the first object in jframe is "pkt_pos", created in
         *       add_packet_to_ffedit_json_file() */
        json_t *jpkt_pos = jframe->obj->kvps[0].value;
        if ( pkt_pos == json_int_val(jpkt_pos) )
            return jframe;
    }

    av_log(ffe_class, AV_LOG_FATAL, "unexpected pkt_pos in decoded frame\n");
    av_log(ffe_class, AV_LOG_FATAL, "This should not have happened.\n");
    av_log(ffe_class, AV_LOG_FATAL, "Please report this file to the FFglitch developer.\n");
    exit(1);
}

static void add_frame_to_ffedit_json_file(
        FFEditJSONFile *jf,
        AVFrame *iframe,
        int stream_index)
{
    int jctx_idx = jf->nb_jctxs;

    json_t *jframe = match_packet_and_frame_in_ffedit_json_file(jf, iframe->pkt_pos, stream_index);
    json_t *jpts = json_int_new(&jf->jctxs[0], iframe->pts);
    json_t *jdts = json_int_new(&jf->jctxs[0], iframe->pkt_dts);

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

    GROW_ARRAY(jf->jctxs, jf->nb_jctxs);
    jf->jctxs[jctx_idx] = *(json_ctx_t *) iframe->jctx;
    av_freep(&iframe->jctx);
}

static void get_from_ffedit_json_file(
        FFEditJSONFile *jf,
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

static void close_ffedit_json_file(FFEditJSONFile **pjf)
{
    FFEditJSONFile *jf = *pjf;

    /* write output json file if needed */
    if ( jf->export_fp != NULL && jf->jstframes != NULL )
    {
        int64_t t0;
        int64_t t1;

        /* check all undecoded packets */
        for ( size_t i = 0; i < jf->nb_jstreams; i++ )
        {
            json_t *jframes = jf->jstframes[i];
            size_t jlen = json_array_length(jframes);
            for ( size_t j = 0; j < jlen; j++ )
            {
                json_t *jframe = json_array_get(jframes, j);
                /* NOTE: If the frame only has one object, this means
                 *       that only "pkt_pos" was created in
                 *       add_packet_to_ffedit_json_file(), and that the
                 *       frame was *not* updated in
                 *       add_frame_to_ffedit_json_file(), which means
                 *       that the packet was skipped by the codec
                 *       itself. We need to close the object here. */
                if ( json_object_length(jframe) == 1 )
                    json_object_done(&jf->jctxs[0], jframe);
            }
        }

        /* close json_ctx_t dynamic objects */
        for ( size_t i = 0; i < jf->nb_jstreams; i++ )
            json_object_done(&jf->jctxs[0], jf->jstreams[i]);
        json_object_done(&jf->jctxs[0], jf->jroot);

        for ( size_t i = 0; i < jf->nb_jstframes; i++ )
        {
            json_dynamic_array_done(&jf->jctxs[0], jf->jstframes[i]);
            json_array_sort(jf->jstframes[i], sort_by_pkt_pos);
        }

        if ( do_benchmark )
            t0 = av_gettime_relative();

        json_fputs(jf->export_fp, jf->jroot);

        if ( do_benchmark )
        {
            t1 = av_gettime_relative();
            benchmark_json_fputs = (t1 - t0);
            t0 = t1;
        }

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

    av_freep(&jf->jstreams);
    av_freep(&jf->jstframes);
    av_freep(pjf);
}

/*********************************************************************/
static void ffedit_packet_queue_init(
        FFEditPacketQueue *pq,
        FFEditPacketQueue **ppq_in,
        FFEditPacketQueue **ppq_out)
{
    MUTEX_INIT(pq->mutex);
    COND_INIT(pq->cond_empty);
    COND_INIT(pq->cond_full);
    *ppq_in = pq;
    *ppq_out = pq;
}

static void ffedit_packet_queue_destroy(FFEditPacketQueue *pq)
{
    MUTEX_DESTROY(pq->mutex);
    COND_DESTROY(pq->cond_empty);
    COND_DESTROY(pq->cond_full);
}

static void add_to_ffedit_packet_queue(FFEditPacketQueue *pq, AVPacket *pkt)
{
    MUTEX_LOCK(pq->mutex);

    while ( 42 )
    {
        if ( pq->nb_elements == MAX_QUEUE )
        {
            COND_WAIT(pq->cond_full, pq->mutex);
        }
        else
        {
            FFEditPacketQueueElement *element = av_mallocz(sizeof(FFEditPacketQueueElement));
            if ( pkt->data == NULL )
                element->pkt = *pkt;
            else
                av_packet_ref(&element->pkt, pkt);
            if ( pq->first == NULL )
                pq->first = element;
            else
                pq->last->next = element;
            pq->last = element;
            pq->nb_elements++;
            COND_SIGNAL(pq->cond_empty);
            break;
        }
    }

    MUTEX_UNLOCK(pq->mutex);
}

static void get_from_ffedit_packet_queue(FFEditPacketQueue *pq, AVPacket *pkt)
{
    MUTEX_LOCK(pq->mutex);

    while ( 42 )
    {
        if ( pq->nb_elements == 0 )
        {
            COND_WAIT(pq->cond_empty, pq->mutex);
        }
        else
        {
            FFEditPacketQueueElement *element = pq->first;
            *pkt = element->pkt;
            pq->first = element->next;
            av_freep(&element);
            pq->nb_elements--;
            COND_SIGNAL(pq->cond_full);
            break;
        }
    }

    MUTEX_UNLOCK(pq->mutex);
}

/*********************************************************************/
static char nibble2char(uint8_t c)
{
    return c + (c > 9 ? ('a'-10) : '0');
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

/*********************************************************************/
static const OptionDef options[] = {
    CMDUTILS_COMMON_OPTIONS
    { "i",                     OPT_TYPE_STRING,            0, { &input_fname }, "input file", "file" },
    { "o",                     OPT_TYPE_STRING,            0, { &output_fname }, "output file", "file" },
    { "a",                     OPT_TYPE_STRING,            0, { &apply_fname }, "apply data", "JSON file" },
    { "e",                     OPT_TYPE_STRING,            0, { &export_fname }, "export data", "JSON file" },
    { "s",                     OPT_TYPE_STRING,            0, { &script_fname }, "run script", "javascript or python3 file" },
    { "sp",                    OPT_TYPE_STRING,            0, { &script_params }, "script setup() parameters", "JSON string" },
    { "f",                     OPT_TYPE_FUNC,   OPT_FUNC_ARG, { .func_arg = opt_feature }, "select feature (optionally specify stream with feat:#)", "feature" },
    { "y",                     OPT_TYPE_BOOL,              0, { &file_overwrite }, "overwrite output files" },
    { "threads",               OPT_TYPE_STRING,            0, { &threads }, "set the number of threads" },
    { "t",                     OPT_TYPE_BOOL,              0, { &test_mode }, "test mode (bitexact output)" },
    { "benchmark",             OPT_TYPE_BOOL,     OPT_EXPERT, { &do_benchmark }, "add timings for benchmarking" },
    { NULL, },
};

static void show_usage(void)
{
    av_log(ffe_class, AV_LOG_INFO, "Simple media glitcher\n");
    av_log(ffe_class, AV_LOG_INFO, "usage: %s [options] input_file [output_file]\n", program_name);
    av_log(ffe_class, AV_LOG_INFO, "\n");
}

static void print_example(const char *desc, const char *str)
{
    av_log(ffe_class, AV_LOG_INFO, "\t%s:\n", desc);
    av_log(ffe_class, AV_LOG_INFO, "\t\tffedit %s\n", str);
}

void show_help_default(const char *opt, const char *arg)
{
    av_log_set_callback(log_callback_help);
    show_usage();
    show_help_options(options, "Main options:", 0, OPT_EXPERT);
    av_log(ffe_class, AV_LOG_INFO, "Examples:\n");
    print_example("check for supported glitches",
                  "-i input.avi");
    print_example("replicate file (for testing)",
                  "-i input.mpg -o output.mpg");
    print_example("export all supported data to a json file",
                  "-i input.avi -e data.json");
    print_example("export quantized DC coeffs from an mjpeg file to a json file",
                  "-i input.mjpeg -f q_dc -e data.json");
    print_example("apply all glitched data from json file",
                  "-i input.avi -a data.json -o output.avi");
    print_example("apply only quantized DC coeffs from json file",
                  "-i input.jpg -f q_dc -a data.json -o output.jpg");
    print_example("export motion vectors from stream 0 to a json file",
                  "-i input.mpg -f mv:0 -e data.json");
}

static int opt_feature(void *optctx, const char *opt, const char *arg)
{
    return opt_feature_internal(arg);
}

static int opt_unknown(void *optctx, const char *opt)
{
    av_log(ffe_class, AV_LOG_FATAL, "Unknown option \"%s\"\n", opt);
    av_log(ffe_class, AV_LOG_FATAL, "Starting with FFglitch 0.10, \"input\" and \"output\" filenames must be\n");
    av_log(ffe_class, AV_LOG_FATAL, "explicitly specified with the \"-i\" and \"-o\" options, respectively.\n");
    av_log(ffe_class, AV_LOG_FATAL, "For example, what used to be:\n");
    av_log(ffe_class, AV_LOG_FATAL, " ffedit <input.mpg> <output.mpg>\n");
    av_log(ffe_class, AV_LOG_FATAL, "must now to be:\n");
    av_log(ffe_class, AV_LOG_FATAL, " ffedit -i <input.mpg> -o <output.mpg>\n");
    exit(1);
    return AVERROR(EINVAL);
}

/*********************************************************************/
static void ffedit_input_open(
        FFEditInputContext *ffi,
        FFEditOutputContext *ectx,
        const char *i_fname,
        int print_format_support)
{
    AVFormatContext *fctx;
    int ret;

    /* allocate contexts */
    fctx = avformat_alloc_context();
    /* ectx must be set before avformat_open_input(), since it calls
     * read_header() internally. */
    fctx->ectx = ectx;
    ffi->fctx = fctx;

    /* open input file */
    if ( strcmp(i_fname, "-") == 0 )
        i_fname = "pipe:";
    ret = avformat_open_input(&fctx, i_fname, NULL, NULL);
    if ( ret < 0 )
    {
        av_log(ffe_class, AV_LOG_FATAL, "Could not open input file '%s'\n", i_fname);
        exit(1);
    }

    ret = avformat_find_stream_info(fctx, 0);
    if ( ret < 0 )
    {
        av_log(ffe_class, AV_LOG_FATAL, "Failed to retrieve input stream information\n");
        exit(1);
    }

    av_dump_format(fctx, 0, i_fname, 0);

    if ( (fctx->iformat->flags & AVFMT_FFEDIT_BITSTREAM) == 0 )
    {
        if ( print_format_support )
            printf("\nFFEdit does not support format '%s'.\n", fctx->iformat->long_name);
        else
            av_log(ffe_class, AV_LOG_FATAL, "Format '%s' not supported by ffedit.\n", fctx->iformat->long_name);
        exit(1);
    }

    /* find codec parameters and decoders */
    for ( size_t i = 0; i < fctx->nb_streams; i++ )
    {
        const AVCodecParameters *par = fctx->streams[i]->codecpar;
        const AVCodec *dec = avcodec_find_decoder(par->codec_id);

        GROW_ARRAY(ffi->pars, ffi->nb_pars);
        ffi->pars[i] = par;
        GROW_ARRAY(ffi->decs, ffi->nb_decs);
        ffi->decs[i] = dec;
        GROW_ARRAY(ffi->time_bases, ffi->nb_time_bases);
        ffi->time_bases[i] = fctx->streams[i]->time_base;

        if ( dec == NULL )
            av_log(ffe_class, AV_LOG_ERROR, "Failed to find decoder for stream %zu. Skipping\n", i);
    }
}

static void ffedit_input_close(FFEditInputContext *ffi)
{
    if ( ffi->fctx != NULL )
        avformat_close_input(&ffi->fctx);
    av_freep(&ffi->decs);
    av_freep(&ffi->pars);
    av_freep(&ffi->time_bases);
}

/*********************************************************************/
static void add_to_ffi_packet_queues(
        FFEditInputContext *ffi,
        AVPacket *pkt)
{
    add_to_ffedit_packet_queue(ffi->pq_out, pkt);
    if ( ffi->pq_out_2 != NULL )
        add_to_ffedit_packet_queue(ffi->pq_out_2, pkt);
}

/*********************************************************************/
static THREAD_RET_TYPE read_func(void *arg)
{
    FFEditInputContext *ffi = (FFEditInputContext *) arg;
    AVFormatContext *fctx = ffi->fctx;
    AVPacket pkt;
    while ( 42 )
    {
        int ret;
        ret = av_read_frame(fctx, &pkt);
        if ( ret < 0 ) // EOF or TODO error
            break;
        add_to_ffi_packet_queues(ffi, &pkt);
        av_packet_unref(&pkt);
    }
    /* send null packets */
    pkt.data = NULL;
    pkt.size = 0;
    for ( size_t i = 0; i < fctx->nb_streams; i++ )
    {
        pkt.stream_index = i;
        add_to_ffi_packet_queues(ffi, &pkt);
    }
    return THREAD_RET_OK;
}

/*********************************************************************/
static void fact_open_decoders(
        FFEditActionContext *fact,
        const FFEditInputContext *ffi)
{
    /* to prevent messages from being printed twice (once for exporting
     * and once for importing) while scripting, we only print them when
     * exporting. */
    int print_messages = fact->is_exporting_script;
    int fret = -1;

    for ( size_t i = 0; i < ffi->nb_decs; i++ )
    {
        const AVCodec *decoder = ffi->decs[i];
        AVDictionary *opts = NULL;
        AVCodecContext *dctx;
        int ret;

        GROW_ARRAY(fact->dctxs, fact->nb_dctxs);
        GROW_ARRAY(fact->time_bases, fact->nb_time_bases);

        if ( decoder == NULL )
            continue;

        if ( (decoder->capabilities & AV_CODEC_CAP_FFEDIT_BITSTREAM) == 0 )
        {
            if ( print_messages )
                av_log(ffe_class, AV_LOG_ERROR, "Codec '%s' not supported by FFedit. Skipping\n", decoder->long_name);
            continue;
        }

        fact->time_bases[i] = ffi->time_bases[i];
        fact->dctxs[i] = avcodec_alloc_context3(decoder);
        dctx = fact->dctxs[i];
        avcodec_parameters_to_context(dctx, ffi->pars[i]);

        /* enable threads if ffedit supports it for this codec */
        if ( threads != NULL )
            av_dict_set(&opts, "threads", threads, 0);
        else if ( (decoder->capabilities & AV_CODEC_CAP_FFEDIT_SLICE_THREADS) != 0 )
            av_dict_set(&opts, "threads", "auto", 0);

        /* disable frame-based multithreading. it causes all sorts of
         * hard-to-debug issues (many seen with mpeg4). */
        av_dict_set(&opts, "thread_type", "slice", 0);

        /* only perform parsing of bitstream (not decoding) */
        dctx->ffedit_flags |= FFEDIT_FLAGS_PARSE_ONLY;

        /* select features */
        for ( size_t j = 0; j < FFEDIT_FEAT_LAST; j++ )
        {
            if ( selected_features[j] != 0
              && (selected_features_idx[j] == i
               || selected_features_idx[j] == -1) )
            {
                if ( fact->is_exporting || fact->is_exporting_script )
                    dctx->ffedit_export |= (1 << j);
                if ( fact->is_importing || fact->is_importing_script )
                    dctx->ffedit_import |= (1 << j);
                if ( fact->is_applying || fact->is_applying_script )
                    dctx->ffedit_apply |= (1 << j);
            }
        }
        /* enable bitstream transplication */
        if ( fact->ectx != NULL )
            dctx->ffedit_apply |= (1 << FFEDIT_FEAT_LAST);

        ret = avcodec_open2(dctx, decoder, &opts);
        av_dict_free(&opts);
        if ( ret < 0 )
        {
            if ( print_messages )
                av_log(ffe_class, AV_LOG_ERROR, "Failed to open decoder for '%s'. Skipping\n", decoder->long_name);
            continue;
        }

        fret = 0;
    }

    if ( fret == -1 )
    {
        av_log(ffe_class, AV_LOG_FATAL, "Could not open any decoder!\n");
        exit(1);
    }
}

static int ffedit_decode(
        FFEditActionContext *fact,
        AVCodecContext *dctx,
        AVFrame *iframe,
        AVPacket *ipkt,
        int stream_index)
{
    int ret;

    ret = avcodec_send_packet(dctx, ipkt);
    /* NOTE: AVERROR(EAGAIN) is considered an error, since we always
     *       read all output with avcodec_receive_frame().
     */
    if ( ret < 0 && ret != AVERROR_EOF )
    {
        char errbuf[0x100] = "unknown error";
        av_strerror(ret, errbuf, sizeof(errbuf));
        av_log(ffe_class, AV_LOG_FATAL, "send_packet() failed: %s\n", errbuf);
        return ret;
    }

    while ( 42 )
    {
        ret = avcodec_receive_frame(dctx, iframe);
        if ( ret < 0 )
        {
            if ( ret != AVERROR(EAGAIN) && ret != AVERROR_EOF )
            {
                char errbuf[0x100] = "unknown error";
                av_strerror(ret, errbuf, sizeof(errbuf));
                av_log(ffe_class, AV_LOG_FATAL, "receive_frame() failed: %s\n", errbuf);
            }
            return ret;
        }

        if ( fact->ectx != NULL && dctx->ffe_xp_packets )
        {
            for ( size_t i = 0; i < dctx->nb_ffe_xp_packets; i++ )
            {
                FFEditTransplicatePacket *opkt = &dctx->ffe_xp_packets[i];
                ffe_output_packet(fact->ectx,
                                  opkt->i_pos,
                                  opkt->i_size,
                                  opkt->data,
                                  opkt->o_size);
                av_freep(&opkt->data);
            }
            av_freep(&dctx->ffe_xp_packets);
            dctx->nb_ffe_xp_packets = 0;
        }

        if ( fact->is_exporting )
            add_frame_to_ffedit_json_file(fact->jf, iframe, stream_index);
        else if ( fact->is_exporting_script )
            add_frame_to_ffedit_json_queue(fact->jq_in, iframe, stream_index);

        fact->frame_number++;
    }

    /* never reached */
    av_assert0(0);

    return 0;
}

static void print_report(
        FFEditActionContext *fact,
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
        fps = fact->frame_number / t;
    av_bprintf(&buf, "frame=%5d fps=%3.*f ", fact->frame_number, fps < 9.95, fps);
    if ( is_last_report )
        av_bprintf(&buf, "L");

    /* output size */
    if ( fact->ectx != NULL )
        total_size = fact->ectx->last_file_size + fact->ectx->file_size_delta;
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
        av_log(ffe_class, AV_LOG_INFO, "%s    %c", buf.str, end_char);
    fflush(stderr);

    /* finalize buffer */
    av_bprint_finalize(&buf, NULL);
}

static THREAD_RET_TYPE fact_transplicate(void *arg)
{
    FFEditActionContext *fact = (FFEditActionContext *) arg;
    AVPacket *ipkt = av_packet_alloc();
    AVFrame *iframe = av_frame_alloc();
    int64_t last_pts = AV_NOPTS_VALUE;
    int64_t timer_start;
    /* to prevent messages from being printed twice (once for exporting
     * and once for importing) while scripting, we only print them when
     * exporting. */
    int print_stats = fact->is_exporting_script;
    json_ctx_t **cur_jctx = av_mallocz(fact->nb_dctxs * sizeof(json_ctx_t *));

    fact->fret = -1;

    if ( fact->is_importing )
        reset_frames_idx_ffedit_json_file(fact->jf);

    if ( print_stats )
        timer_start = av_gettime_relative();

    while ( 42 )
    {
        AVCodecContext *dctx;
        int stream_index;
        int ret;

        /* NOTE: There is always at least one packet in the queue (the
         *       null packet from read_func()) which must trigger
         *       an error below, so this call will always succeed.
         */
        get_from_ffedit_packet_queue(fact->pq_in, ipkt);

        stream_index = ipkt->stream_index;

        dctx = fact->dctxs[stream_index];
        if ( dctx == NULL )
        {
            av_packet_unref(ipkt);
            continue;
        }

        if ( ipkt->data != NULL )
        {
            memset(ipkt->ffedit_sd, 0x00, sizeof(ipkt->ffedit_sd));
            if ( fact->is_importing )
                get_from_ffedit_json_file(fact->jf, ipkt, stream_index);
            else if ( fact->is_exporting )
                add_packet_to_ffedit_json_file(fact->jf, ipkt->pos, stream_index);
            else if ( fact->is_importing_script )
                get_from_ffedit_json_queue(fact->jq_out, ipkt);
        }

        if ( fact->is_importing_script )
        {
            json_ctx_free(cur_jctx[stream_index]);
            av_free(cur_jctx[stream_index]);
            cur_jctx[stream_index] = ipkt->jctx;
        }

        ret = ffedit_decode(fact, dctx, iframe, ipkt, stream_index);

        if ( print_stats )
        {
            int64_t pts = ipkt->pts;
            if ( pts == AV_NOPTS_VALUE )
                pts = ipkt->dts;
            if ( pts != AV_NOPTS_VALUE )
                pts = av_rescale_q(pts, fact->time_bases[stream_index], AV_TIME_BASE_Q);
            if ( last_pts == AV_NOPTS_VALUE || last_pts < pts )
                last_pts = pts;
            print_report(fact, timer_start, last_pts, 0);
        }

        av_packet_unref(ipkt);

        if ( ret < 0 )
        {
            if ( ret == AVERROR_EOF )
                break;
            if ( ret != AVERROR(EAGAIN) )
            {
                av_log(ffe_class, AV_LOG_FATAL, "ffedit_decode() failed\n");
                goto the_end;
            }
        }
    }

    if ( print_stats )
        print_report(fact, timer_start, last_pts, 1);

    fact->fret = 0;

the_end:
    av_frame_free(&iframe);
    av_freep(&ipkt);
    av_freep(&cur_jctx);

    return THREAD_RET_OK;
}

static void fact_print_features(FFEditInputContext *ffi)
{
    for ( size_t i = 0; i < ffi->nb_decs; i++ )
    {
        const AVCodec *decoder = ffi->decs[i];
        if ( decoder != NULL )
        {
            if ( decoder->ffedit_features == 0 )
            {
                printf("\nFFEdit does not support codec '%s'.\n", decoder->long_name);
            }
            else
            {
                printf("\nFFEdit support for codec '%s':\n", decoder->long_name);
                for ( size_t j = 0; j < FFEDIT_FEAT_LAST; j++ )
                    if ( (decoder->ffedit_features & (1 << j)) != 0 )
                        printf("\t[%-12s]: %s\n", ffe_feat_to_str(j), ffe_feat_desc(j));
            }
        }
    }
}

static void fact_terminate(FFEditActionContext *fact)
{
    /* free codec contexts */
    for ( size_t i = 0; i < fact->nb_dctxs; i++ )
    {
        avcodec_free_context(&fact->dctxs[i]);
        av_freep(&fact->dctxs[i]);
    }
    av_freep(&fact->dctxs);
    av_freep(&fact->time_bases);
}

/*********************************************************************/
/* exit(1)s on error */
static int check_actions_options(void)
{
    int actions_count = 0;
    if ( (export_fname != NULL) )
        actions_count++;
    if ( (apply_fname != NULL) )
        actions_count++;
    if ( (script_fname != NULL) )
        actions_count++;
    if ( actions_count > 1 )
    {
        av_log(ffe_class, AV_LOG_FATAL, "Only one of -s, -e, or -a may be used!\n");
        exit(1);
    }
    if ( (export_fname != NULL) )
    {
        if ( (output_fname != NULL) )
        {
            av_log(ffe_class, AV_LOG_FATAL, "Exporting data (-e) does not support output file (-o)!\n");
            exit(1);
        }
        return ACTION_EXPORT;
    }
    else if ( (apply_fname != NULL) )
    {
        if ( (output_fname == NULL) )
        {
            av_log(ffe_class, AV_LOG_FATAL, "Output file (-o) required when transplicating (-a)!\n");
            exit(1);
        }
        return ACTION_TRANSPLICATE;
    }
    else if ( (script_fname != NULL) )
    {
        return ACTION_SCRIPT;
    }
    else if ( (output_fname != NULL) )
    {
        return ACTION_REPLICATE;
    }
    return ACTION_PRINT_FEATURES;
}

/* exit(1)s on error */
static void check_selected_features(FFEditAction action)
{
    if ( features_selected == 0 )
    {
        av_log(ffe_class, AV_LOG_FATAL, "At least one feature (-f) must be selected!\n");
        av_log(ffe_class, AV_LOG_FATAL, "Supported features can be listed by specifying only the input file (-i) and no action.\n");
        if ( action == ACTION_SCRIPT )
            av_log(ffe_class, AV_LOG_FATAL, "You can also select features by adding them to args[\"features\"] in your script's setup().\n");
        exit(1);
    }
    /* check for mutually exclusive features if applying requested */
    if ( action == ACTION_TRANSPLICATE || action == ACTION_SCRIPT )
    {
        for ( size_t i = 0; i < FFEDIT_FEAT_LAST; i++ )
        {
            if ( selected_features[i] )
            {
                for ( size_t j = 0; j < FFEDIT_FEAT_LAST; j++ )
                {
                    if ( (i != j) && selected_features[j] )
                    {
                        if ( ffe_feat_excludes(i, j) )
                        {
                            av_log(ffe_class, AV_LOG_FATAL, "Mutually exclusive features \"%s\" and \"%s\" specified while transplicating!\n",
                                   ffe_feat_to_str(i), ffe_feat_to_str(j));
                            exit(1);
                        }
                    }
                }
            }
        }
    }
}

/*********************************************************************/
int main(int argc, char *argv[])
{
    int64_t t0;
    int64_t t1;
    FFEditAction action;
    int ret;

    hack_musl_pthread_stack_size();

    /* init stuff */
    av_log_set_flags(AV_LOG_SKIP_REPEATED);
    parse_loglevel(argc, argv, options);

    avformat_network_init();

    show_banner(argc, argv, options);

    ret = parse_options(NULL, argc, argv, options, opt_unknown);
    if ( ret < 0 )
        exit(ret == AVERROR_EXIT ? 0 : 1);

    /* there must be one input */
    if ( !input_fname )
    {
        av_log(ffe_class, AV_LOG_FATAL, "No input file specified!\n");
        exit(1);
    }

    /* check action options consistency */
    action = check_actions_options();

    /* initialize ffedit actions */
    if ( action == ACTION_PRINT_FEATURES )
    {
        FFEditInputContext ffi = { 0 };

        /* open input/output files */
        ffedit_input_open(&ffi, NULL, input_fname, 1);

        /* no processing needed. just print glitching features */
        fact_print_features(&ffi);

        /* terminate */
        ffedit_input_close(&ffi);
    }
    else if ( action == ACTION_REPLICATE )
    {
        /* output context to keep packets modified by decoder */
        FFEditOutputContext *ffo_codec = NULL;
        /* output context to keep track of fixups from file format */
        FFEditOutputContext *ffo_fmt = NULL;
        FFEditInputContext ffi = { 0 };
        FFEditActionContext fact0 = { 0 };
        FFEditPacketQueue pq = { 0 };
        THREAD_TYPE read_thread;

        /* open input/output files */
        ffe_output_open(&ffo_fmt, NULL);
        ffedit_input_open(&ffi, ffo_fmt, input_fname, 0);
        ffedit_output_open(&ffo_codec, output_fname);
        if ( check_seekability(ffi.fctx, &ffo_fmt, ffo_codec) < 0 )
            exit(1);
        fact0.ectx = ffo_codec;
        fact_open_decoders(&fact0, &ffi);

        if ( do_benchmark )
            t0 = av_gettime_relative();

        /* do the salmon dance */
        ffedit_packet_queue_init(&pq, &fact0.pq_in, &ffi.pq_out);
        THREAD_CREATE(read_thread, read_func, &ffi);
        fact_transplicate(&fact0);

        /* wait for threads */
        THREAD_JOIN(read_thread);

        if ( do_benchmark )
        {
            t1 = av_gettime_relative();
            benchmark_transplicate = (t1 - t0);
            t0 = t1;
        }

        /* terminate */
        ffedit_packet_queue_destroy(&pq);
        ffedit_output_close(&ffo_codec, &ffo_fmt, ffi.fctx);
        fact_terminate(&fact0);
        ffedit_input_close(&ffi);

        if ( do_benchmark )
        {
            t1 = av_gettime_relative();
            benchmark_output = (t1 - t0);
            t0 = t1;
        }
    }
    else if ( action == ACTION_EXPORT )
    {
        FFEditInputContext ffi = { 0 };
        FFEditActionContext fact0 = { 0 };
        FFEditPacketQueue pq = { 0 };
        THREAD_TYPE read_thread;

        /* check that at least one feature was selected */
        check_selected_features(action);

        /* open input/output files */
        ffedit_input_open(&ffi, NULL, input_fname, 0);
        fact0.is_exporting = 1;
        fact_open_decoders(&fact0, &ffi);

        /* prepare output json file */
        fact0.jf = prepare_ffedit_json_file(export_fname, input_fname, &ffi, selected_features);

        if ( do_benchmark )
            t0 = av_gettime_relative();

        /* do the salmon dance */
        ffedit_packet_queue_init(&pq, &fact0.pq_in, &ffi.pq_out);
        THREAD_CREATE(read_thread, read_func, &ffi);
        fact_transplicate(&fact0);

        /* wait for threads */
        THREAD_JOIN(read_thread);

        if ( do_benchmark )
        {
            t1 = av_gettime_relative();
            benchmark_transplicate = (t1 - t0);
            t0 = t1;
        }

        /* terminate */
        ffedit_packet_queue_destroy(&pq);
        close_ffedit_json_file(&fact0.jf);
        fact_terminate(&fact0);
        ffedit_input_close(&ffi);

        if ( do_benchmark )
        {
            t1 = av_gettime_relative();
            benchmark_export = (t1 - t0);
            t0 = t1;
        }
    }
    else if ( action == ACTION_TRANSPLICATE )
    {
        /* output context to keep packets modified by decoder */
        FFEditOutputContext *ffo_codec = NULL;
        /* output context to keep track of fixups from file format */
        FFEditOutputContext *ffo_fmt = NULL;
        FFEditInputContext ffi = { 0 };
        FFEditActionContext fact0 = { 0 };
        FFEditPacketQueue pq = { 0 };
        THREAD_TYPE read_thread;

        /* check that at least one feature was selected */
        check_selected_features(action);

        /* open input/output files */
        ffe_output_open(&ffo_fmt, NULL);
        ffedit_input_open(&ffi, ffo_fmt, input_fname, 0);
        ffedit_output_open(&ffo_codec, output_fname);
        if ( check_seekability(ffi.fctx, &ffo_fmt, ffo_codec) < 0 )
            exit(1);
        fact0.ectx = ffo_codec;
        fact0.is_importing = 1;
        fact0.is_applying = 1;
        fact_open_decoders(&fact0, &ffi);

        if ( do_benchmark )
            t0 = av_gettime_relative();

        /* read input json file */
        fact0.jf = read_ffedit_json_file(apply_fname);

        if ( do_benchmark )
        {
            t1 = av_gettime_relative();
            benchmark_import = (t1 - t0);
            t0 = t1;
        }

        /* do the salmon dance */
        ffedit_packet_queue_init(&pq, &fact0.pq_in, &ffi.pq_out);
        THREAD_CREATE(read_thread, read_func, &ffi);
        fact_transplicate(&fact0);

        /* wait for threads */
        THREAD_JOIN(read_thread);

        if ( do_benchmark )
        {
            t1 = av_gettime_relative();
            benchmark_transplicate = (t1 - t0);
            t0 = t1;
        }

        /* terminate */
        ffedit_packet_queue_destroy(&pq);
        ffedit_output_close(&ffo_codec, &ffo_fmt, ffi.fctx);
        close_ffedit_json_file(&fact0.jf);
        fact_terminate(&fact0);
        ffedit_input_close(&ffi);

        if ( do_benchmark )
        {
            t1 = av_gettime_relative();
            benchmark_output = (t1 - t0);
            t0 = t1;
        }
    }
    else if ( action == ACTION_SCRIPT )
    {
        int do_output;
        /* output context to keep packets modified by decoder */
        FFEditOutputContext *ffo_codec = NULL;
        /* output context to keep track of fixups from file format */
        FFEditOutputContext *ffo_fmt = NULL;
        FFEditInputContext ffi = { 0 };
        FFEditScriptFuncContext sfc = { 0 };
        FFEditActionContext fact0 = { 0 };
        FFEditActionContext fact1 = { 0 };
        FFEditPacketQueue pq = { 0 };
        FFEditPacketQueue pq_2 = { 0 };
        THREAD_TYPE read_thread;
        AVFrame poison_frame;

        THREAD_TYPE script_exporting_thread;

        /* init script func context */
        sfc_init(&sfc, script_fname);

        /* start is_importing_script thread */
        sfc_setup(&sfc, script_func);

        /* check that at least one feature was selected */
        check_selected_features(action);

        /* check that we have an output filename */
        do_output = (output_fname != NULL);

        /* open input/output files */
        if ( do_output )
        {
            ffe_output_open(&ffo_fmt, NULL);
            ffedit_output_open(&ffo_codec, output_fname);
        }
        ffedit_input_open(&ffi, ffo_fmt, input_fname, 0);
        if ( do_output && check_seekability(ffi.fctx, &ffo_fmt, ffo_codec) < 0 )
            exit(1);
        fact0.ectx = ffo_codec;
        fact0.jq_out = &sfc.jq_out;
        fact0.is_importing_script = 1;
        fact0.is_applying_script = do_output;
        fact_open_decoders(&fact0, &ffi);
        fact1.jq_in = &sfc.jq_in;
        fact1.is_exporting_script = 1;
        fact_open_decoders(&fact1, &ffi);

        /* prepare the arguments for each stream */
        for ( size_t i = 0; i < ffi.nb_decs; i++ )
        {
            GROW_ARRAY(sfc.decs, sfc.nb_decs);
            sfc.decs[i] = ffi.decs[i];
        }

        if ( do_benchmark )
            t0 = av_gettime_relative();

        /* resume is_importing_script thread */
        sfc_resume(&sfc);

        /* do the salmon dance */
        ffedit_packet_queue_init(&pq, &fact0.pq_in, &ffi.pq_out);
        ffedit_packet_queue_init(&pq_2, &fact1.pq_in, &ffi.pq_out_2);
        THREAD_CREATE(read_thread, read_func, &ffi);
        THREAD_CREATE(script_exporting_thread, fact_transplicate, &fact1);
        fact_transplicate(&fact0);

        /* send poison pill to exit script_func_thread */
        poison_frame.pkt_pos = -2;
        poison_frame.jctx = NULL;
        add_frame_to_ffedit_json_queue(fact1.jq_in, &poison_frame, -1);

        /* wait for threads */
        THREAD_JOIN(read_thread);
        THREAD_JOIN(script_exporting_thread);

        if ( do_benchmark )
        {
            t1 = av_gettime_relative();
            benchmark_transplicate = (t1 - t0);
            t0 = t1;
        }

        /* destroy script func context */
        sfc_destroy(&sfc);

        /* terminate */
        ffedit_packet_queue_destroy(&pq);
        ffedit_packet_queue_destroy(&pq_2);
        if ( do_output )
            ffedit_output_close(&ffo_codec, &ffo_fmt, ffi.fctx);
        fact_terminate(&fact0);
        fact_terminate(&fact1);
        ffedit_input_close(&ffi);

        if ( do_benchmark )
        {
            t1 = av_gettime_relative();
            benchmark_output = (t1 - t0);
            t0 = t1;
        }
    }

    if ( do_benchmark )
    {
        if ( benchmark_json_parse != 0 )
            av_log(ffe_class, AV_LOG_INFO, "time taken in json_parse() %" PRId64 "\n", benchmark_json_parse);
        if ( benchmark_import != 0 )
            av_log(ffe_class, AV_LOG_INFO, "time taken in import %" PRId64 "\n", benchmark_import);
        if ( benchmark_transplicate != 0 )
            av_log(ffe_class, AV_LOG_INFO, "time taken in transplicate %" PRId64 "\n", benchmark_transplicate);
        if ( benchmark_json_fputs != 0 )
            av_log(ffe_class, AV_LOG_INFO, "time taken in json_fputs() %" PRId64 "\n", benchmark_json_fputs);
        if ( benchmark_export != 0 )
            av_log(ffe_class, AV_LOG_INFO, "time taken in export %" PRId64 "\n", benchmark_export);
        if ( benchmark_convert_to_quickjs != 0 )
            av_log(ffe_class, AV_LOG_INFO, "time taken in convert_to_quickjs %" PRId64 "\n", benchmark_convert_to_quickjs);
        if ( benchmark_script_call != 0 )
            av_log(ffe_class, AV_LOG_INFO, "time taken in script_call %" PRId64 "\n", benchmark_script_call);
        if ( benchmark_convert_from_quickjs != 0 )
            av_log(ffe_class, AV_LOG_INFO, "time taken in convert_from_quickjs %" PRId64 "\n", benchmark_convert_from_quickjs);
        if ( benchmark_output != 0 )
            av_log(ffe_class, AV_LOG_INFO, "time taken in output %" PRId64 "\n", benchmark_output);
    }

    av_freep(&input_fname);
    av_freep(&output_fname);
    av_freep(&apply_fname);
    av_freep(&export_fname);
    av_freep(&script_fname);
    av_freep(&threads);

    return 0;
}

static const AVClass _ffe_class = {
    .class_name = "FFEdit",
    .item_name  = av_default_item_name,
    .version    = LIBAVUTIL_VERSION_INT,
};
const void *ffe_class[1] = { &_ffe_class };
