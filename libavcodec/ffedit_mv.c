/*
 * Copyright (c) 2017-2022 Ramiro Polla
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

#include "ffedit.h"
#include "ffedit_json.h"
#include "ffedit_mv.h"
#include "internal.h"

/*-------------------------------------------------------------------*/
static av_always_inline
void mv_export_init_mb(
        enum FFEditFeature feat,
        ffe_mv_mb_ctx *mbctx,
        AVFrame *f,
        int mb_y,
        int mb_x,
        int nb_directions,
        int nb_blocks)
{
    json_t *jframe = f->ffedit_sd[feat];

    if ( jframe == NULL )
        return;

    mbctx->mb_y = mb_y;
    mbctx->mb_x = mb_x;
}

void ffe_mv_export_init_mb(
        ffe_mv_mb_ctx *mbctx,
        AVFrame *f,
        int mb_y,
        int mb_x,
        int nb_directions,
        int nb_blocks)
{
    mv_export_init_mb(FFEDIT_FEAT_MV, mbctx, f, mb_y, mb_x, nb_directions, nb_blocks);
}

void ffe_mv_delta_export_init_mb(
        ffe_mv_mb_ctx *mbctx,
        AVFrame *f,
        int mb_y,
        int mb_x,
        int nb_directions,
        int nb_blocks)
{
    mv_export_init_mb(FFEDIT_FEAT_MV_DELTA, mbctx, f, mb_y, mb_x, nb_directions, nb_blocks);
}

/*-------------------------------------------------------------------*/
static av_always_inline
void mv_import_init_mb(
        enum FFEditFeature feat,
        ffe_mv_mb_ctx *mbctx,
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

    ctx = json_object_userdata_get(jframe);
    mbctx->overflow_action = ctx->overflow_action;
    mbctx->mb_y = mb_y;
    mbctx->mb_x = mb_x;
    mbctx->nb_blocks = nb_blocks;

    for ( size_t i = 0; i < nb_directions; i++ )
    {
        json_t *jdata = ctx->data[i];
        if ( jdata != NULL && !ctx->import_is_mv2darray[i] )
        {
            json_t *jmb_y = json_array_get(jdata, mb_y);
            json_t *jmb_x = json_array_get(jmb_y, mb_x);
            mbctx->jmb[i] = jmb_x;
        }
    }
}

void ffe_mv_import_init_mb(
        ffe_mv_mb_ctx *mbctx,
        AVFrame *f,
        int mb_y,
        int mb_x,
        int nb_directions,
        int nb_blocks)
{
    mv_import_init_mb(FFEDIT_FEAT_MV, mbctx, f, mb_y, mb_x, nb_directions, nb_blocks);
}

void ffe_mv_delta_import_init_mb(
        ffe_mv_mb_ctx *mbctx,
        AVFrame *f,
        int mb_y,
        int mb_x,
        int nb_directions,
        int nb_blocks)
{
    mv_import_init_mb(FFEDIT_FEAT_MV_DELTA, mbctx, f, mb_y, mb_x, nb_directions, nb_blocks);
}

/*-------------------------------------------------------------------*/
static av_always_inline
void mv_export_select(
        enum FFEditFeature feat,
        ffe_mv_mb_ctx *mbctx,
        AVFrame *f,
        int direction,
        int blockn)
{
    json_t *jframe = f->ffedit_sd[feat];
    ffe_mv_ctx *ctx;
    json_t *jdata;
    json_mv2darray_t *jmv2d;
    size_t idx;

    if ( jframe == NULL )
        return;

    ctx = json_object_userdata_get(jframe);

    jdata = ctx->data[direction];
    jmv2d = jdata->mv2darray;

    idx = mbctx->mb_y * jmv2d->width + mbctx->mb_x;
    mbctx->cur_export = &jmv2d->mvs[blockn][idx << 1];
    jmv2d->nb_blocks_array[idx] = blockn + 1;
    mbctx->pused = &ctx->used[direction];
}

void ffe_mv_export_select(
        ffe_mv_mb_ctx *mbctx,
        AVFrame *f,
        int direction,
        int blockn)
{
    mv_export_select(FFEDIT_FEAT_MV, mbctx, f, direction, blockn);
}

void ffe_mv_delta_export_select(
        ffe_mv_mb_ctx *mbctx,
        AVFrame *f,
        int direction,
        int blockn)
{
    mv_export_select(FFEDIT_FEAT_MV_DELTA, mbctx, f, direction, blockn);
}

/*-------------------------------------------------------------------*/
static av_always_inline
void mv_import_select(
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

    ctx = json_object_userdata_get(jframe);

    mbctx->cur_export = NULL;
    mbctx->cur_import = NULL;

    if ( ctx->import_is_mv2darray[direction] )
    {
        json_t *jdata = ctx->data[direction];
        if ( jdata != NULL )
        {
            json_mv2darray_t *jmv2d = jdata->mv2darray;
            size_t idx = mbctx->mb_y * jmv2d->width + mbctx->mb_x;
            mbctx->cur_export = &jmv2d->mvs[blockn][idx << 1];
        }
    }
    else
    {
        mbctx->cur_import = mbctx->jmb[direction];
        if ( mbctx->nb_blocks > 1 )
            mbctx->cur_import = json_array_get(mbctx->cur_import, blockn);
    }
}

void ffe_mv_import_select(
        ffe_mv_mb_ctx *mbctx,
        AVFrame *f,
        int direction,
        int blockn)
{
    mv_import_select(FFEDIT_FEAT_MV, mbctx, f, direction, blockn);
}

void ffe_mv_delta_import_select(
        ffe_mv_mb_ctx *mbctx,
        AVFrame *f,
        int direction,
        int blockn)
{
    mv_import_select(FFEDIT_FEAT_MV_DELTA, mbctx, f, direction, blockn);
}

/*-------------------------------------------------------------------*/
static av_always_inline
int32_t mv_get_internal(
        ffe_mv_mb_ctx *mbctx,
        int x_or_y)
{
    if ( mbctx->cur_export != NULL )
        return mbctx->cur_export[x_or_y];
    else
        return mbctx->cur_import->array_of_ints[x_or_y];
}

int32_t ffe_mv_get(
        ffe_mv_mb_ctx *mbctx,
        int x_or_y)
{
    return mv_get_internal(mbctx, x_or_y);
}

int32_t ffe_mv_delta_get(
        ffe_mv_mb_ctx *mbctx,
        int x_or_y)
{
    return mv_get_internal(mbctx, x_or_y);
}

/*-------------------------------------------------------------------*/
static av_always_inline
void mv_set_internal(
        ffe_mv_mb_ctx *mbctx,
        int x_or_y,
        int32_t val)
{
    mbctx->cur_export[x_or_y] = val;
    *mbctx->pused = 1;
}

void ffe_mv_set(
        ffe_mv_mb_ctx *mbctx,
        int x_or_y,
        int32_t val)
{
    mv_set_internal(mbctx, x_or_y, val);
}

void ffe_mv_delta_set(
        ffe_mv_mb_ctx *mbctx,
        int x_or_y,
        int32_t val)
{
    mv_set_internal(mbctx, x_or_y, val);
}

/*-------------------------------------------------------------------*/
static av_always_inline
void mv_export_fcode_internal(
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

    ctx = json_object_userdata_get(jframe);
    if ( f_or_b == 0 )
        ctx->fcode->array_of_ints[num] = fcode;
    else
        ctx->bcode->array_of_ints[num] = fcode;
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
static av_always_inline
int mv_overflow_internal(
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
                av_log(NULL, AV_LOG_ERROR, "motion vector%s value overflow\n", delta);
                av_log(NULL, AV_LOG_ERROR, "value of %d is outside of range [ %d, %d ] (fcode %d shift %d)\n",
                       val, min_val, max_val, fcode, shift);
                av_log(NULL, AV_LOG_ERROR, "either reencode the input file with a higher -fcode or set the\n");
                av_log(NULL, AV_LOG_ERROR, "\"overflow\" field in \"mv%s\" to \"truncate\", \"ignore\", or \"warn\" instead of \"assert\".\n",
                       _delta);
                exit(1);
                break;
            case MV_OVERFLOW_TRUNCATE:
                av_log(NULL, AV_LOG_VERBOSE, "motion vector%s value truncated from %d to %d (fcode %d shift %d)\n",
                       delta, val, new_val, fcode, shift);
                val = new_val;
                break;
            case MV_OVERFLOW_IGNORE:
                /* do nothing */
                break;
            case MV_OVERFLOW_WARN:
                av_log(NULL, AV_LOG_WARNING, "motion vector%s value %d is outside of range [ %d, %d ] (fcode %d shift %d)\n",
                       delta, val, min_val, max_val, fcode, shift);
                if ( warned == 0 )
                {
                    warned = 1;
                    av_log(NULL, AV_LOG_WARNING, "either reencode the input file with a higher -fcode or set the\n");
                    av_log(NULL, AV_LOG_WARNING, "\"overflow\" field in \"mv%s\" to \"assert\", \"truncate\", or \"ignore\" instead of \"warn\".\n",
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
static av_always_inline
void mv_export_init_internal(
        enum FFEditFeature feat,
        json_ctx_t *jctx,
        AVFrame *f,
        int mb_height,
        int mb_width,
        int nb_fcodes,
        int max_nb_blocks)
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
    //  "direct": [ ] # line
    //            [ ] # column
    //            null or [ mv_x, mv_y ]
    //                 or [ [ mv_x, mv_y ], ... ]
    //  "fcode": [ ]
    //  "bcode": [ ]
    //  "overflow": "assert", "truncate", or "ignore"
    // }

    json_t *jframe = json_object_new(jctx);
    ffe_mv_ctx *ctx = json_allocator_get0(jctx, sizeof(ffe_mv_ctx));
    json_object_userdata_set(jframe, ctx);

    ctx->data[0] = json_mv2darray_new(jctx, mb_width, mb_height, max_nb_blocks, -1);
    ctx->data[1] = json_mv2darray_new(jctx, mb_width, mb_height, max_nb_blocks, -1);
    ctx->data[2] = json_mv2darray_new(jctx, mb_width, mb_height, max_nb_blocks, -1);
    ctx->fcode = json_array_of_ints_new(jctx, nb_fcodes);
    ctx->bcode = json_array_of_ints_new(jctx, nb_fcodes);
    ctx->overflow = json_string_new(jctx, "warn");
    ctx->overflow_action = MV_OVERFLOW_ASSERT;

    json_set_pflags(ctx->fcode, JSON_PFLAGS_NO_LF);
    json_set_pflags(ctx->bcode, JSON_PFLAGS_NO_LF);

    ctx->used[0] = 0;
    json_object_add(jframe, "forward", ctx->data[0]);
    ctx->used[1] = 0;
    json_object_add(jframe, "backward", ctx->data[1]);
    ctx->used[2] = 0;
    json_object_add(jframe, "direct", ctx->data[2]);
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
        int nb_fcodes,
        int max_nb_blocks)
{
    mv_export_init_internal(FFEDIT_FEAT_MV, jctx, f, mb_height, mb_width, nb_fcodes, max_nb_blocks);
}

void ffe_mv_delta_export_init(
        json_ctx_t *jctx,
        AVFrame *f,
        int mb_height,
        int mb_width,
        int nb_fcodes,
        int max_nb_blocks)
{
    mv_export_init_internal(FFEDIT_FEAT_MV_DELTA, jctx, f, mb_height, mb_width, nb_fcodes, max_nb_blocks);
}

/*-------------------------------------------------------------------*/
static av_always_inline
void mv_export_cleanup(
        enum FFEditFeature feat,
        json_ctx_t *jctx,
        AVFrame *f)
{
    json_t *jframe = f->ffedit_sd[feat];
    ffe_mv_ctx *ctx = json_object_userdata_get(jframe);
    int mvs_deleted = 0;
    if ( ctx->used[0] == 0 )
    {
        json_object_del(jframe, "forward");
        json_object_del(jframe, "fcode");
        mvs_deleted++;
    }
    else
    {
        json_mv2darray_done(jctx, ctx->data[0]);
    }
    if ( ctx->used[1] == 0 )
    {
        json_object_del(jframe, "backward");
        json_object_del(jframe, "bcode");
        mvs_deleted++;
    }
    else
    {
        json_mv2darray_done(jctx, ctx->data[1]);
    }
    if ( ctx->used[2] == 0 )
    {
        json_object_del(jframe, "direct");
        mvs_deleted++;
    }
    else
    {
        json_mv2darray_done(jctx, ctx->data[2]);
    }
    if ( mvs_deleted == 3 )
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
static av_always_inline
void mv_import_init(
        enum FFEditFeature feat,
        json_ctx_t *jctx,
        AVFrame *f)
{
    json_t *jframe = f->ffedit_sd[feat];
    ffe_mv_ctx *ctx = json_allocator_get0(jctx, sizeof(ffe_mv_ctx));
    int overflow_action = MV_OVERFLOW_ASSERT;
    json_object_userdata_set(jframe, ctx);
    ctx->data[0] = json_object_get(jframe, "forward");
    ctx->data[1] = json_object_get(jframe, "backward");
    ctx->data[2] = json_object_get(jframe, "direct");
    ctx->import_is_mv2darray[0] = (ctx->data[0] != NULL) && (JSON_TYPE(ctx->data[0]->flags) == JSON_TYPE_MV_2DARRAY);
    ctx->import_is_mv2darray[1] = (ctx->data[1] != NULL) && (JSON_TYPE(ctx->data[1]->flags) == JSON_TYPE_MV_2DARRAY);
    ctx->import_is_mv2darray[2] = (ctx->data[2] != NULL) && (JSON_TYPE(ctx->data[2]->flags) == JSON_TYPE_MV_2DARRAY);
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
                av_log(NULL, AV_LOG_ERROR, "unexpected value \"%s\" for \"overflow\" field in \"mv\".\n", str);
                av_log(NULL, AV_LOG_ERROR, "expected values are \"assert\", \"truncate\", \"ignore\", or \"warn\".\n");
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
