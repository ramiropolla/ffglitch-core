
#include <string.h>
#include <stdio.h>

#include "libavutil/mem.h"

#include "ffedit_json.h"

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
    json_t *jlines = json_array_new(jctx);

    json_array_alloc(jctx, jlines, height);
    for ( size_t mb_y = 0; mb_y < height; mb_y++ )
    {
        json_t *jcolumns = json_array_new(jctx);
        json_set_pflags(jcolumns, pflags);
        json_array_alloc(jctx, jcolumns, width);
        json_array_set(jlines, mb_y, jcolumns);
    }

    return jlines;
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
        int pflags)
{
    // {
    //  "data": [ ] # plane
    //          [ ] # line
    //          [ ] # column
    // }

    json_t *jscan = json_object_new(jctx);
    json_t *jdata = json_array_new(jctx);

    json_array_alloc(jctx, jdata, nb_components);
    for ( size_t component = 0; component < nb_components; component++ )
    {
        size_t height = mb_height * v_count[component];
        size_t width = mb_width * h_count[component];
        json_t *jlines = ffe_jblock_new(jctx, width, height, pflags);
        json_array_set(jdata, component, jlines);
    }
    json_object_add(jscan, "data", jdata);

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

    json_userdata_set(jso, ctx);
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
    mb_array_ctx *ctx = json_userdata_get(jso);
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
        json_t *s,
        int component,
        int mb_y,
        int mb_x,
        int block)
{
    json_t *jline = ffe_jmb_select(s, component, &mb_y, &mb_x, block);
    return json_array_get(jline, mb_x);
}

//---------------------------------------------------------------------
void
ffe_jmb_set(
        json_t *s,
        int component,
        int mb_y,
        int mb_x,
        int block,
        json_t *jval)
{
    json_t *jline = ffe_jmb_select(s, component, &mb_y, &mb_x, block);
    json_array_set(jline, mb_x, jval);
}

//---------------------------------------------------------------------
void
ffe_json_array_zero_fill(
        json_ctx_t *jctx,
        json_t *jso,
        size_t len)
{
    for ( int i = 0; i < len; i++ )
        if ( json_array_get_int(jso, i) == JSON_NULL )
            json_array_set_int(jctx, jso, i, 0);
}
