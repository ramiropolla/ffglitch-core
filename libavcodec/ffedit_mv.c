
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
void ffe_mv_export_init_mb(
        ffe_mv_mb_ctx *mbctx,
        json_ctx_t *jctx,
        AVFrame *f,
        int mb_y,
        int mb_x,
        int nb_directions,
        int nb_blocks)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_MV];
    ffe_mv_ctx *ctx;

    if ( jframe == NULL )
        return;

    ctx = json_userdata_get(jframe);
    mbctx->nb_blocks = nb_blocks;

    for ( size_t i = 0; i < nb_directions; i++ )
    {
        json_t *jdata = ctx->data[i];
        json_t *jmb_y = json_array_get(jdata, mb_y);
        json_t *jmb_x;

        if ( nb_blocks > 1 )
        {
            jmb_x = json_array_new(jctx);
            json_set_pflags(jmb_x, JSON_PFLAGS_NO_LF | JSON_PFLAGS_NO_SPACE);
            json_array_alloc(jctx, jmb_x, nb_blocks);
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

/*-------------------------------------------------------------------*/
void ffe_mv_import_init_mb(
        ffe_mv_mb_ctx *mbctx,
        json_ctx_t *jctx,
        AVFrame *f,
        int mb_y,
        int mb_x,
        int nb_directions,
        int nb_blocks)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_MV];
    ffe_mv_ctx *ctx;

    if ( jframe == NULL )
        return;

    ctx = json_userdata_get(jframe);
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

/*-------------------------------------------------------------------*/
void ffe_mv_select(
        ffe_mv_mb_ctx *mbctx,
        AVFrame *f,
        int direction,
        int blockn)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_MV];
    ffe_mv_ctx *ctx;

    if ( jframe == NULL )
        return;

    ctx = json_userdata_get(jframe);
    mbctx->cur = mbctx->jmb[direction];
    mbctx->pcount = &ctx->count[direction];
    if ( mbctx->nb_blocks > 1 )
        mbctx->cur = json_array_get(mbctx->cur, blockn);
}

/*-------------------------------------------------------------------*/
int ffe_mv_get(
        ffe_mv_mb_ctx *mbctx,
        AVFrame *f,
        int x_or_y)
{
    return json_array_get_int(mbctx->cur, x_or_y);
}

/*-------------------------------------------------------------------*/
void ffe_mv_set(
        ffe_mv_mb_ctx *mbctx,
        AVFrame *f,
        int x_or_y,
        int val)
{
    if ( is_json_null(mbctx->cur) )
    {
        json_make_array_of_ints(f->jctx, mbctx->cur, 2);
        json_set_pflags(mbctx->cur, JSON_PFLAGS_NO_LF | JSON_PFLAGS_NO_SPACE);
    }
    json_array_set_int(f->jctx, mbctx->cur, x_or_y, val);
    atomic_fetch_add(mbctx->pcount, 1);
}

/*-------------------------------------------------------------------*/
void ffe_mv_export_init(
        json_ctx_t *jctx,
        AVFrame *f,
        int mb_height,
        int mb_width)
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
    // }

    json_t *jframe = json_object_new(jctx);
    ffe_mv_ctx *ctx = json_allocator_get0(f->jctx, sizeof(ffe_mv_ctx));
    json_userdata_set(jframe, ctx);

    ctx->data[0] = ffe_jblock_new(jctx, mb_width, mb_height, JSON_PFLAGS_NO_LF);
    ctx->data[1] = ffe_jblock_new(jctx, mb_width, mb_height, JSON_PFLAGS_NO_LF);

    atomic_init(&ctx->count[0], 0);
    json_object_add(jframe, "forward", ctx->data[0]);
    atomic_init(&ctx->count[1], 0);
    json_object_add(jframe, "backward", ctx->data[1]);

    f->ffedit_sd[FFEDIT_FEAT_MV] = jframe;
}

/*-------------------------------------------------------------------*/
void ffe_mv_export_cleanup(json_ctx_t *jctx, AVFrame *f)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_MV];
    ffe_mv_ctx *ctx = json_userdata_get(jframe);
    if ( atomic_load(&ctx->count[0]) == 0 )
        json_object_del(jframe, "forward");
    if ( atomic_load(&ctx->count[1]) == 0 )
        json_object_del(jframe, "backward");
    json_object_done(jctx, jframe);
}

/*-------------------------------------------------------------------*/
void ffe_mv_import_init(json_ctx_t *jctx, AVFrame *f)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_MV];
    ffe_mv_ctx *ctx = json_allocator_get0(f->jctx, sizeof(ffe_mv_ctx));
    json_userdata_set(jframe, ctx);
    ctx->data[0] = json_object_get(jframe, "forward");
    ctx->data[1] = json_object_get(jframe, "backward");
}
