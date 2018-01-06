
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
    ctx->nb_blocks = nb_blocks;

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
        ctx->jmb[i] = jmb_x;
    }
}

/*-------------------------------------------------------------------*/
void ffe_mv_import_init_mb(
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
    ctx->nb_blocks = nb_blocks;

    for ( size_t i = 0; i < nb_directions; i++ )
    {
        json_t *jdata = ctx->data[i];
        json_t *jmb_y;
        json_t *jmb_x;
        if ( jdata == NULL )
            continue;
        jmb_y = json_array_get(jdata, mb_y);
        jmb_x = json_array_get(jmb_y, mb_x);
        ctx->jmb[i] = jmb_x;
    }
}

/*-------------------------------------------------------------------*/
void ffe_mv_select(
        AVFrame *f,
        int direction,
        int blockn)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_MV];
    ffe_mv_ctx *ctx;

    if ( jframe == NULL )
        return;

    ctx = json_userdata_get(jframe);
    ctx->cur = ctx->jmb[direction];
    ctx->pcount = &ctx->count[direction];
    if ( ctx->nb_blocks > 1 )
        ctx->cur = json_array_get(ctx->cur, blockn);
}

/*-------------------------------------------------------------------*/
int ffe_mv_get(
        AVFrame *f,
        int x_or_y)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_MV];
    ffe_mv_ctx *ctx = json_userdata_get(jframe);
    return json_array_get_int(ctx->cur, x_or_y);
}

/*-------------------------------------------------------------------*/
void ffe_mv_set(
        AVFrame *f,
        int x_or_y,
        int val)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_MV];
    ffe_mv_ctx *ctx = json_userdata_get(jframe);
    if ( is_json_null(ctx->cur) )
    {
        json_make_array_of_ints(f->jctx, ctx->cur, 2);
        json_set_pflags(ctx->cur, JSON_PFLAGS_NO_LF | JSON_PFLAGS_NO_SPACE);
    }
    json_array_set_int(f->jctx, ctx->cur, x_or_y, val);
    (*ctx->pcount)++;
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

    ctx->count[0] = 0;
    json_object_add(jframe, "forward", ctx->data[0]);
    ctx->count[1] = 0;
    json_object_add(jframe, "backward", ctx->data[1]);

    f->ffedit_sd[FFEDIT_FEAT_MV] = jframe;
}

/*-------------------------------------------------------------------*/
void ffe_mv_export_cleanup(json_ctx_t *jctx, AVFrame *f)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_MV];
    ffe_mv_ctx *ctx = json_userdata_get(jframe);
    if ( ctx->count[0] == 0 )
        json_object_del(jframe, "forward");
    if ( ctx->count[1] == 0 )
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
