
#include "libavutil/json-c/json.h"

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
        AVFrame *f,
        int mb_y,
        int mb_x,
        int nb_directions,
        int nb_blocks)
{
    json_object *jframe = f->ffedit_sd[FFEDIT_FEAT_MV];
    ffe_mv_ctx *ctx;

    if ( jframe == NULL )
        return;

    ctx = json_object_get_userdata(jframe);
    ctx->nb_blocks = nb_blocks;

    for ( size_t i = 0; i < nb_directions; i++ )
    {
        json_object *jdata = ctx->data[i];
        json_object *jmb_y = json_object_array_get_idx(jdata, mb_y);
        json_object *jmb_x = json_object_new_array();
        json_object_array_put_idx(jmb_y, mb_x, jmb_x);
        ctx->jmb[i] = jmb_x;

        if ( nb_blocks > 1 )
        {
            for ( size_t j = 0; j < nb_blocks; j++ )
            {
                json_object *jblock = json_object_new_array();
                json_object_array_put_idx(jmb_x, j, jblock);
            }
        }
        json_object_set_userdata(jmb_x, (void *) nb_blocks, NULL);
    }
}

/*-------------------------------------------------------------------*/
void ffe_mv_import_init_mb(
        AVFrame *f,
        int mb_y,
        int mb_x,
        int nb_directions,
        int nb_blocks)
{
    json_object *jframe = f->ffedit_sd[FFEDIT_FEAT_MV];
    ffe_mv_ctx *ctx;

    if ( jframe == NULL )
        return;

    ctx = json_object_get_userdata(jframe);
    ctx->nb_blocks = nb_blocks;

    for ( size_t i = 0; i < nb_directions; i++ )
    {
        json_object *jdata = ctx->data[i];
        json_object *jmb_y;
        json_object *jmb_x;
        if ( jdata == NULL )
            continue;
        jmb_y = json_object_array_get_idx(jdata, mb_y);
        jmb_x = json_object_array_get_idx(jmb_y, mb_x);
        ctx->jmb[i] = jmb_x;
    }
}

/*-------------------------------------------------------------------*/
void ffe_mv_select(
        AVFrame *f,
        int direction,
        int blockn)
{
    json_object *jframe = f->ffedit_sd[FFEDIT_FEAT_MV];
    ffe_mv_ctx *ctx;

    if ( jframe == NULL )
        return;

    ctx = json_object_get_userdata(jframe);
    ctx->cur = ctx->jmb[direction];
    ctx->pcount = &ctx->count[direction];
    if ( ctx->nb_blocks > 1 )
        ctx->cur = json_object_array_get_idx(ctx->cur, blockn);
}

/*-------------------------------------------------------------------*/
int ffe_mv_get(
        AVFrame *f,
        int x_or_y)
{
    json_object *jframe = f->ffedit_sd[FFEDIT_FEAT_MV];
    ffe_mv_ctx *ctx = json_object_get_userdata(jframe);
    json_object *jval = json_object_array_get_idx(ctx->cur, x_or_y);
    return json_object_get_int(jval);
}

/*-------------------------------------------------------------------*/
void ffe_mv_set(
        AVFrame *f,
        int x_or_y,
        int val)
{
    json_object *jframe = f->ffedit_sd[FFEDIT_FEAT_MV];
    ffe_mv_ctx *ctx = json_object_get_userdata(jframe);
    json_object *jval = json_object_new_int(val);
    json_object_array_put_idx(ctx->cur, x_or_y, jval);
    (*ctx->pcount)++;
}

/*-------------------------------------------------------------------*/
void ffe_mv_export_init(
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

    json_object *jframe = json_object_new_object();
    ffe_mv_ctx *ctx = av_mallocz(sizeof(ffe_mv_ctx));
    json_object_set_userdata(jframe, ctx, ffe_free_userdata);

    ctx->data[0] = ffe_jblock_new(mb_width, mb_height,
                                   ffe_int2_line_to_json_string,
                                   (void *) 3);
    ctx->data[1] = ffe_jblock_new(mb_width, mb_height,
                                   ffe_int2_line_to_json_string,
                                   (void *) 3);

    ctx->count[0] = 0;
    json_object_object_add(jframe, "forward", ctx->data[0]);
    ctx->count[1] = 0;
    json_object_object_add(jframe, "backward", ctx->data[1]);

    f->ffedit_sd[FFEDIT_FEAT_MV] = jframe;
}

/*-------------------------------------------------------------------*/
void ffe_mv_export_cleanup(AVFrame *f)
{
    json_object *jframe = f->ffedit_sd[FFEDIT_FEAT_MV];
    ffe_mv_ctx *ctx = json_object_get_userdata(jframe);
    if ( ctx->count[0] == 0 )
        json_object_object_del(jframe, "forward");
    if ( ctx->count[1] == 0 )
        json_object_object_del(jframe, "backward");
}

/*-------------------------------------------------------------------*/
void ffe_mv_import_init(AVFrame *f)
{
    json_object *jframe = f->ffedit_sd[FFEDIT_FEAT_MV];
    ffe_mv_ctx *ctx = av_mallocz(sizeof(ffe_mv_ctx));
    json_object_set_userdata(jframe, ctx, ffe_free_userdata);
    json_object_object_get_ex(jframe, "forward", &ctx->data[0]);
    json_object_object_get_ex(jframe, "backward", &ctx->data[1]);
}
