
#include "libavutil/json.h"

#include "ffedit.h"
#include "ffedit_json.h"
#include "ffedit_mv.h"
#include "internal.h"

/*-------------------------------------------------------------------*/
static const AVClass ffe_mv_class = {
    .class_name = "FFEditMV",
    .item_name  = av_default_item_name,
    .version    = LIBAVUTIL_VERSION_INT,
};

/*-------------------------------------------------------------------*/
static void mv_export_init_mb(
        enum FFEditFeature feat,
        ffe_mv_mb_ctx *mbctx,
        json_ctx_t *jctx,
        AVFrame *f,
        int mb_y,
        int mb_x,
        int nb_directions,
        int nb_blocks)
{
    json_t *jframe = f->ffedit_sd[feat];
    ffe_mv_ctx *ctx;

    if ( jframe == NULL )
        return;

    ctx = json_userdata_get(jframe);
    mbctx->overflow_action = ctx->overflow_action;
    mbctx->nb_blocks = nb_blocks;

    for ( size_t i = 0; i < nb_directions; i++ )
    {
        json_t *jdata = ctx->data[i];
        json_t *jmb_y = json_array_get(jdata, mb_y);
        json_t *jmb_x;

        if ( nb_blocks > 1 )
        {
            jmb_x = json_array_new(jctx, nb_blocks);
            json_set_pflags(jmb_x, JSON_PFLAGS_NO_LF | JSON_PFLAGS_NO_SPACE);
            for ( size_t j = 0; j < nb_blocks; j++ )
            {
                json_t *jblock = json_int_new(jctx, JSON_NULL);
                json_set_pflags(jblock, JSON_PFLAGS_NO_LF | JSON_PFLAGS_NO_SPACE);
                json_array_set(jmb_x, j, jblock);
            }
        }
        else
        {
            jmb_x = json_int_new(jctx, JSON_NULL);
            json_set_pflags(jmb_x, JSON_PFLAGS_NO_LF | JSON_PFLAGS_NO_SPACE);
        }

        json_array_set(jmb_y, mb_x, jmb_x);
        mbctx->jmb[i] = jmb_x;
    }
}

void ffe_mv_export_init_mb(
        ffe_mv_mb_ctx *mbctx,
        json_ctx_t *jctx,
        AVFrame *f,
        int mb_y,
        int mb_x,
        int nb_directions,
        int nb_blocks)
{
    mv_export_init_mb(FFEDIT_FEAT_MV, mbctx, jctx, f, mb_y, mb_x, nb_directions, nb_blocks);
}

void ffe_mv_delta_export_init_mb(
        ffe_mv_mb_ctx *mbctx,
        json_ctx_t *jctx,
        AVFrame *f,
        int mb_y,
        int mb_x,
        int nb_directions,
        int nb_blocks)
{
    mv_export_init_mb(FFEDIT_FEAT_MV_DELTA, mbctx, jctx, f, mb_y, mb_x, nb_directions, nb_blocks);
}

/*-------------------------------------------------------------------*/
static void mv_import_init_mb(
        enum FFEditFeature feat,
        ffe_mv_mb_ctx *mbctx,
        json_ctx_t *jctx,
        AVFrame *f,
        int mb_y,
        int mb_x,
        int nb_directions,
        int nb_blocks)
{
    json_t *jframe = f->ffedit_sd[feat];
    ffe_mv_ctx *ctx;

    if ( jframe == NULL )
        return;

    ctx = json_userdata_get(jframe);
    mbctx->overflow_action = ctx->overflow_action;
    mbctx->nb_blocks = nb_blocks;

    for ( size_t i = 0; i < nb_directions; i++ )
    {
        json_t *jdata = ctx->data[i];
        json_t *jmb_y;
        json_t *jmb_x;
        if ( jdata == NULL )
            continue;
        jmb_y = json_array_get(jdata, mb_y);
        jmb_x = json_array_get(jmb_y, mb_x);
        mbctx->jmb[i] = jmb_x;
    }
}

void ffe_mv_import_init_mb(
        ffe_mv_mb_ctx *mbctx,
        json_ctx_t *jctx,
        AVFrame *f,
        int mb_y,
        int mb_x,
        int nb_directions,
        int nb_blocks)
{
    mv_import_init_mb(FFEDIT_FEAT_MV, mbctx, jctx, f, mb_y, mb_x, nb_directions, nb_blocks);
}

void ffe_mv_delta_import_init_mb(
        ffe_mv_mb_ctx *mbctx,
        json_ctx_t *jctx,
        AVFrame *f,
        int mb_y,
        int mb_x,
        int nb_directions,
        int nb_blocks)
{
    mv_import_init_mb(FFEDIT_FEAT_MV_DELTA, mbctx, jctx, f, mb_y, mb_x, nb_directions, nb_blocks);
}

/*-------------------------------------------------------------------*/
static void mv_select(
        enum FFEditFeature feat,
        ffe_mv_mb_ctx *mbctx,
        AVFrame *f,
        int direction,
        int blockn)
{
    json_t *jframe = f->ffedit_sd[feat];
    ffe_mv_ctx *ctx;

    if ( jframe == NULL )
        return;

    ctx = json_userdata_get(jframe);
    mbctx->cur = mbctx->jmb[direction];
    mbctx->pcount = &ctx->count[direction];
    if ( mbctx->nb_blocks > 1 )
        mbctx->cur = json_array_get(mbctx->cur, blockn);
}

void ffe_mv_select(
        ffe_mv_mb_ctx *mbctx,
        AVFrame *f,
        int direction,
        int blockn)
{
    mv_select(FFEDIT_FEAT_MV, mbctx, f, direction, blockn);
}

void ffe_mv_delta_select(
        ffe_mv_mb_ctx *mbctx,
        AVFrame *f,
        int direction,
        int blockn)
{
    mv_select(FFEDIT_FEAT_MV_DELTA, mbctx, f, direction, blockn);
}

/*-------------------------------------------------------------------*/
static int mv_get_internal(
        ffe_mv_mb_ctx *mbctx,
        AVFrame *f,
        int x_or_y)
{
    return json_array_get_int(mbctx->cur, x_or_y);
}

int ffe_mv_get(
        ffe_mv_mb_ctx *mbctx,
        AVFrame *f,
        int x_or_y)
{
    return mv_get_internal(mbctx, f, x_or_y);
}

int ffe_mv_delta_get(
        ffe_mv_mb_ctx *mbctx,
        AVFrame *f,
        int x_or_y)
{
    return mv_get_internal(mbctx, f, x_or_y);
}

/*-------------------------------------------------------------------*/
static void mv_set_internal(
        ffe_mv_mb_ctx *mbctx,
        json_ctx_t *jctx,
        int x_or_y,
        int val)
{
    if ( is_json_null(mbctx->cur) )
    {
        json_make_array_of_ints(jctx, mbctx->cur, 2);
        json_set_pflags(mbctx->cur, JSON_PFLAGS_NO_LF | JSON_PFLAGS_NO_SPACE);
    }
    json_array_set_int(jctx, mbctx->cur, x_or_y, val);
    atomic_fetch_add(mbctx->pcount, 1);
}

void ffe_mv_set(
        ffe_mv_mb_ctx *mbctx,
        json_ctx_t *jctx,
        int x_or_y,
        int val)
{
    mv_set_internal(mbctx, jctx, x_or_y, val);
}

void ffe_mv_delta_set(
        ffe_mv_mb_ctx *mbctx,
        json_ctx_t *jctx,
        int x_or_y,
        int val)
{
    mv_set_internal(mbctx, jctx, x_or_y, val);
}

/*-------------------------------------------------------------------*/
static void mv_export_fcode_internal(
        enum FFEditFeature feat,
        json_ctx_t *jctx,
        AVFrame *f,
        int f_or_b,
        int num,
        int fcode)
{
    json_t *jframe = f->ffedit_sd[feat];
    ffe_mv_ctx *ctx;

    if ( jframe == NULL )
        return;

    ctx = json_userdata_get(jframe);
    if ( f_or_b == 0 )
        json_array_set_int(jctx, ctx->fcode, num, fcode);
    else
        json_array_set_int(jctx, ctx->bcode, num, fcode);
}

void ffe_mv_export_fcode(
        json_ctx_t *jctx,
        AVFrame *f,
        int f_or_b,
        int num,
        int fcode)
{
    mv_export_fcode_internal(FFEDIT_FEAT_MV, jctx, f, f_or_b, num, fcode);
}

void ffe_mv_delta_export_fcode(
        json_ctx_t *jctx,
        AVFrame *f,
        int f_or_b,
        int num,
        int fcode)
{
    mv_export_fcode_internal(FFEDIT_FEAT_MV_DELTA, jctx, f, f_or_b, num, fcode);
}

/*-------------------------------------------------------------------*/
static int mv_overflow_internal(
        ffe_mv_mb_ctx *mbctx,
        int val,
        int fcode,
        int shift,
        int is_delta)
{
    static int warned = 0;
    const char *delta = is_delta ? " delta" : "";
    const char *_delta = is_delta ? "_delta" : "";
    const int bit_size = fcode - 1;
    const int min_val = -(1<<(shift+bit_size-1));
    const int max_val =  (1<<(shift+bit_size-1))-1;
    const int new_val = (val < min_val) ? min_val
                      : (val > max_val) ? max_val
                      :                       val;
    if ( new_val != val )
    {
        switch ( mbctx->overflow_action )
        {
            case MV_OVERFLOW_ASSERT:
                av_log(NULL, AV_LOG_ERROR, "FFedit: motion vector%s value overflow\n", delta);
                av_log(NULL, AV_LOG_ERROR, "FFedit: value of %d is outside of range [ %d, %d ] (fcode %d shift %d)\n",
                       val, min_val, max_val, fcode, shift);
                av_log(NULL, AV_LOG_ERROR, "FFedit: either reencode the input file with a higher -fcode or set the\n");
                av_log(NULL, AV_LOG_ERROR, "FFedit: \"overflow\" field in \"mv%s\" to \"truncate\", \"ignore\", or \"warn\" instead of \"assert\".\n",
                       _delta);
                exit(1);
                break;
            case MV_OVERFLOW_TRUNCATE:
                av_log(NULL, AV_LOG_VERBOSE, "FFedit: motion vector%s value truncated from %d to %d (fcode %d shift %d)\n",
                       delta, val, new_val, fcode, shift);
                val = new_val;
                break;
            case MV_OVERFLOW_IGNORE:
                /* do nothing */
                break;
            case MV_OVERFLOW_WARN:
                av_log(NULL, AV_LOG_WARNING, "FFedit: motion vector%s value %d is outside of range [ %d, %d ] (fcode %d shift %d)\n",
                       delta, val, min_val, max_val, fcode, shift);
                if ( warned == 0 )
                {
                    warned = 1;
                    av_log(NULL, AV_LOG_WARNING, "FFedit: either reencode the input file with a higher -fcode or set the\n");
                    av_log(NULL, AV_LOG_WARNING, "FFedit: \"overflow\" field in \"mv%s\" to \"assert\", \"truncate\", or \"ignore\" instead of \"warn\".\n",
                           _delta);
                }
                break;
        }
    }
    return val;
}

int ffe_mv_overflow(
        ffe_mv_mb_ctx *mbctx,
        int pred,
        int val,
        int fcode,
        int shift)
{
    /* NOTE maintaining FFmpeg behaviour that does not sign-extend when
     *      delta is zero. I don't know whether this is correct or not.
     */
    if ( pred == val )
        return 0;
    /* return value will be sign-extended while encoding */
    return mv_overflow_internal(mbctx, val, fcode, shift, 0) - pred;
}

int ffe_mv_delta_overflow(
        ffe_mv_mb_ctx *mbctx,
        int delta,
        int fcode,
        int shift)
{
    return mv_overflow_internal(mbctx, delta, fcode, shift, 1);
}

/*-------------------------------------------------------------------*/
static void mv_export_init_internal(
        enum FFEditFeature feat,
        json_ctx_t *jctx,
        AVFrame *f,
        int mb_height,
        int mb_width,
        int nb_fcodes)
{
    // {
    //  "forward":  [ ] # line
    //              [ ] # column
    //              null or [ mv_x, mv_y ]
    //                   or [ [ mv_x, mv_y ], ... ]
    //  "backward": [ ] # line
    //              [ ] # column
    //              null or [ mv_x, mv_y ]
    //                   or [ [ mv_x, mv_y ], ... ]
    //  "fcode": [ ]
    //  "bcode": [ ]
    //  "overflow": "assert", "truncate", or "ignore"
    // }

    json_t *jframe = json_object_new(jctx);
    ffe_mv_ctx *ctx = json_allocator_get0(jctx, sizeof(ffe_mv_ctx));
    json_userdata_set(jframe, ctx);

    ctx->data[0] = ffe_jblock_new(jctx, mb_width, mb_height, JSON_PFLAGS_NO_LF);
    ctx->data[1] = ffe_jblock_new(jctx, mb_width, mb_height, JSON_PFLAGS_NO_LF);
    ctx->fcode = json_array_of_ints_new(jctx, nb_fcodes);
    ctx->bcode = json_array_of_ints_new(jctx, nb_fcodes);
    ctx->overflow = json_string_new(jctx, "warn");
    ctx->overflow_action = MV_OVERFLOW_ASSERT;

    json_set_pflags(ctx->fcode, JSON_PFLAGS_NO_LF);
    json_set_pflags(ctx->bcode, JSON_PFLAGS_NO_LF);

    atomic_init(&ctx->count[0], 0);
    json_object_add(jframe, "forward", ctx->data[0]);
    atomic_init(&ctx->count[1], 0);
    json_object_add(jframe, "backward", ctx->data[1]);
    json_object_add(jframe, "fcode", ctx->fcode);
    json_object_add(jframe, "bcode", ctx->bcode);
    json_object_add(jframe, "overflow", ctx->overflow);

    f->ffedit_sd[feat] = jframe;
}

void ffe_mv_export_init(
        json_ctx_t *jctx,
        AVFrame *f,
        int mb_height,
        int mb_width,
        int nb_fcodes)
{
    mv_export_init_internal(FFEDIT_FEAT_MV, jctx, f, mb_height, mb_width, nb_fcodes);
}

void ffe_mv_delta_export_init(
        json_ctx_t *jctx,
        AVFrame *f,
        int mb_height,
        int mb_width,
        int nb_fcodes)
{
    mv_export_init_internal(FFEDIT_FEAT_MV_DELTA, jctx, f, mb_height, mb_width, nb_fcodes);
}

/*-------------------------------------------------------------------*/
static void mv_export_cleanup(
        enum FFEditFeature feat,
        json_ctx_t *jctx,
        AVFrame *f)
{
    json_t *jframe = f->ffedit_sd[feat];
    ffe_mv_ctx *ctx = json_userdata_get(jframe);
    int mvs_deleted = 0;
    if ( atomic_load(&ctx->count[0]) == 0 )
    {
        json_object_del(jframe, "forward");
        json_object_del(jframe, "fcode");
        mvs_deleted++;
    }
    if ( atomic_load(&ctx->count[1]) == 0 )
    {
        json_object_del(jframe, "backward");
        json_object_del(jframe, "bcode");
        mvs_deleted++;
    }
    if ( mvs_deleted == 2 )
        json_object_del(jframe, "overflow");
    json_object_done(jctx, jframe);
}

void ffe_mv_export_cleanup(json_ctx_t *jctx, AVFrame *f)
{
    mv_export_cleanup(FFEDIT_FEAT_MV, jctx, f);
}

void ffe_mv_delta_export_cleanup(json_ctx_t *jctx, AVFrame *f)
{
    mv_export_cleanup(FFEDIT_FEAT_MV_DELTA, jctx, f);
}

/*-------------------------------------------------------------------*/
static void mv_import_init(
        enum FFEditFeature feat,
        json_ctx_t *jctx,
        AVFrame *f)
{
    json_t *jframe = f->ffedit_sd[feat];
    ffe_mv_ctx *ctx = json_allocator_get0(jctx, sizeof(ffe_mv_ctx));
    int overflow_action = MV_OVERFLOW_ASSERT;
    json_userdata_set(jframe, ctx);
    ctx->data[0] = json_object_get(jframe, "forward");
    ctx->data[1] = json_object_get(jframe, "backward");
    ctx->fcode = json_object_get(jframe, "fcode");
    ctx->bcode = json_object_get(jframe, "bcode");
    ctx->overflow = json_object_get(jframe, "overflow");
    if ( ctx->overflow )
    {
        const char *str = json_string_get(ctx->overflow);
        if ( str != NULL )
        {
            if ( strcmp(str, "assert") == 0 )
            {
                overflow_action = MV_OVERFLOW_ASSERT;
            }
            else if ( strcmp(str, "truncate") == 0 )
            {
                overflow_action = MV_OVERFLOW_TRUNCATE;
            }
            else if ( strcmp(str, "ignore") == 0 )
            {
                overflow_action = MV_OVERFLOW_IGNORE;
            }
            else if ( strcmp(str, "warn") == 0 )
            {
                overflow_action = MV_OVERFLOW_WARN;
            }
            else
            {
                av_log(NULL, AV_LOG_ERROR, "FFedit: unexpected value \"%s\" for \"overflow\" field in \"mv\".\n", str);
                av_log(NULL, AV_LOG_ERROR, "FFedit: expected values are \"assert\", \"truncate\", \"ignore\", or \"warn\".\n");
                exit(1);
            }
        }
    }
    ctx->overflow_action = overflow_action;
}

void ffe_mv_import_init(json_ctx_t *jctx, AVFrame *f)
{
    mv_import_init(FFEDIT_FEAT_MV, jctx, f);
}

void ffe_mv_delta_import_init(json_ctx_t *jctx, AVFrame *f)
{
    mv_import_init(FFEDIT_FEAT_MV_DELTA, jctx, f);
}
