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

#include <string.h>
#include <stdio.h>

#include "libavutil/mem.h"

#include "ffedit_json.h"

/* helper */
json_ctx_t *json_ctx_new(int large)
{
    json_ctx_t *jctx = av_mallocz(sizeof(json_ctx_t));
    json_ctx_start(jctx, large);
    return jctx;
}

/* 2d array of blocks */

//---------------------------------------------------------------------
json_t *
ffe_jblock_new(
        json_ctx_t *jctx,
        int width,
        int height,
        int pflags)
{
    // [ ] # line
    // [ ] # column
    json_t *jlines = json_array_new(jctx, height);

    for ( size_t mb_y = 0; mb_y < height; mb_y++ )
    {
        json_t *jcolumns = json_array_new(jctx, width);
        json_set_pflags(jcolumns, pflags);
        json_array_set(jlines, mb_y, jcolumns);
    }

    return jlines;
}

//---------------------------------------------------------------------
void
ffe_jblock_set(
        json_t *jso,
        int mb_y,
        int mb_x,
        json_t *jval)
{
    json_t *jline = json_array_get(jso, mb_y);
    json_array_set(jline, mb_x, jval);
}

/* macroblock array */

//---------------------------------------------------------------------
#define MAX_COMPONENTS 4
typedef struct {
    json_t *jdata;
    int h_count[MAX_COMPONENTS];
    int v_count[MAX_COMPONENTS];
} mb_array_ctx;

// Blocks per macroblock:

// yuv420
//
// Y  Y     h_count = { 2, 1, 1 };
//  UV      v_count = { 2, 1, 1 };
// Y  Y

// yuv422
//
// YUV Y    h_count = { 2, 1, 1 };
// YUV Y    v_count = { 2, 2, 2 };

//---------------------------------------------------------------------
json_t *
ffe_jmb_new(
        json_ctx_t *jctx,
        int mb_width,
        int mb_height,
        int nb_components,
        int *v_count,
        int *h_count,
        int *quant_index,
        int pflags)
{
    // {
    //  "data": [ ] # plane
    //          [ ] # line
    //          [ ] # column
    //  "v_count": [ ]
    //  "h_count": [ ]
    //  "quant_index": [ ]
    // }

    json_t *jscan = json_object_new(jctx);
    json_t *jdata = json_array_new(jctx, nb_components);
    json_t *jv_count = json_array_of_ints_new(jctx, nb_components);
    json_t *jh_count = json_array_of_ints_new(jctx, nb_components);
    json_t *jquant_index = NULL;
    if ( quant_index != NULL )
        jquant_index = json_array_of_ints_new(jctx, nb_components);

    for ( size_t component = 0; component < nb_components; component++ )
    {
        size_t height = mb_height * v_count[component];
        size_t width = mb_width * h_count[component];
        json_t *jlines = ffe_jblock_new(jctx, width, height, pflags);
        json_array_set(jdata, component, jlines);
        jv_count->array_of_ints[component] = v_count[component];
        jh_count->array_of_ints[component] = h_count[component];
        if ( jquant_index != NULL )
            jquant_index->array_of_ints[component] = quant_index[component];
    }
    json_object_add(jscan, "data", jdata);
    json_set_pflags(jv_count, JSON_PFLAGS_NO_LF);
    json_object_add(jscan, "v_count", jv_count);
    json_set_pflags(jh_count, JSON_PFLAGS_NO_LF);
    json_object_add(jscan, "h_count", jh_count);
    if ( jquant_index != NULL )
    {
        json_set_pflags(jquant_index, JSON_PFLAGS_NO_LF);
        json_object_add(jscan, "quant_index", jquant_index);
    }

    ffe_jmb_set_context(jscan, nb_components, v_count, h_count);

    return jscan;
}

//---------------------------------------------------------------------
void
ffe_jmb_set_context(
        json_t *jso,
        int nb_components,
        int *v_count,
        int *h_count)
{
    mb_array_ctx *ctx = av_mallocz(sizeof(mb_array_ctx));

    for ( size_t component = 0; component < nb_components; component++ )
    {
        ctx->v_count[component] = v_count[component];
        ctx->h_count[component] = h_count[component];
    }

    ctx->jdata = json_object_get(jso, "data");

    json_object_userdata_set(jso, ctx);
}

//---------------------------------------------------------------------
static json_t *
ffe_jmb_select(
        json_t *jso,
        int component,
        int *pmb_y,
        int *pmb_x,
        int block)
{
    mb_array_ctx *ctx = json_object_userdata_get(jso);
    int mb_y = *pmb_y;
    int mb_x = *pmb_x;
    json_t *jplane;

    mb_y *= ctx->v_count[component];
    mb_x *= ctx->h_count[component];

    mb_y += block / ctx->h_count[component];
    mb_x += block % ctx->h_count[component];

    *pmb_y = mb_y;
    *pmb_x = mb_x;

    jplane = json_array_get(ctx->jdata, component);

    return json_array_get(jplane, mb_y);
}

//---------------------------------------------------------------------
json_t *
ffe_jmb_get(
        json_t *jso,
        int component,
        int mb_y,
        int mb_x,
        int block)
{
    json_t *jline = ffe_jmb_select(jso, component, &mb_y, &mb_x, block);
    return json_array_get(jline, mb_x);
}

//---------------------------------------------------------------------
int32_t
ffe_jmb_array_of_ints_get(
        json_t *jso,
        int component,
        int mb_y,
        int mb_x,
        int block)
{
    json_t *jline = ffe_jmb_select(jso, component, &mb_y, &mb_x, block);
    return jline->array_of_ints[mb_x];
}

//---------------------------------------------------------------------
int32_t
ffe_jmb_int_get(
        json_t *jso,
        int component,
        int mb_y,
        int mb_x,
        int block)
{
    json_t *jline = ffe_jmb_select(jso, component, &mb_y, &mb_x, block);
    if ( JSON_TYPE(jline->flags) == JSON_TYPE_ARRAY_OF_INTS )
    {
        return jline->array_of_ints[mb_x];
    }
    else
    {
        json_t *jval = json_array_get(jline, mb_x);
        return json_int_val(jval);
    }
}

//---------------------------------------------------------------------
void
ffe_jmb_set(
        json_t *jso,
        int component,
        int mb_y,
        int mb_x,
        int block,
        json_t *jval)
{
    json_t *jline = ffe_jmb_select(jso, component, &mb_y, &mb_x, block);
    json_array_set(jline, mb_x, jval);
}
