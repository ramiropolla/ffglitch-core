
#include <string.h>
#include <stdio.h>

#include "libavutil/mem.h"

#include "cas9_json.h"

/* 2d array of blocks */

//---------------------------------------------------------------------
json_object *
cas9_jblock_new(
        int width,
        int height,
        json_object_to_json_string_fn line_func,
        void *ud)
{
    // [ ] # line
    // [ ] # column
    json_object *jlines = json_object_new_array();

    json_object_array_put_idx(jlines, height-1, NULL);
    for ( size_t mb_y = 0; mb_y < height; mb_y++ )
    {
        json_object *jcolumns = json_object_new_array();
        json_object_set_serializer(jcolumns, line_func, ud, NULL);
        json_object_array_put_idx(jcolumns, width-1, NULL);
        json_object_array_put_idx(jlines, mb_y, jcolumns);
    }

    return jlines;
}

/* macroblock array */

//---------------------------------------------------------------------
#define MAX_COMPONENTS 4
typedef struct {
    json_object *jdata;
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
json_object *
cas9_jmb_new(
        int mb_width,
        int mb_height,
        int nb_components,
        int *v_count,
        int *h_count,
        json_object_to_json_string_fn line_func,
        void *ud)
{
    // {
    //  "data": [ ] # plane
    //          [ ] # line
    //          [ ] # column
    // }

    json_object *jscan = json_object_new_object();
    json_object *jdata = json_object_new_array();

    json_object_array_put_idx(jdata, nb_components-1, NULL);
    for ( size_t component = 0; component < nb_components; component++ )
    {
        size_t height = mb_height * v_count[component];
        size_t width = mb_width * h_count[component];
        json_object *jlines = cas9_jblock_new(width, height, line_func, ud);
        json_object_array_put_idx(jdata, component, jlines);
    }
    json_object_object_add(jscan, "data", jdata);

    cas9_jmb_set_context(jscan, nb_components, v_count, h_count);

    return jscan;
}

//---------------------------------------------------------------------
void
cas9_jmb_set_context(
        json_object *jso,
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

    json_object_object_get_ex(jso, "data", &ctx->jdata);

    json_object_set_userdata(jso, ctx, cas9_free_userdata);
}

//---------------------------------------------------------------------
static json_object *
cas9_jmb_select(
        json_object *jso,
        int component,
        int *pmb_y,
        int *pmb_x,
        int block)
{
    mb_array_ctx *ctx = json_object_get_userdata(jso);
    int mb_y = *pmb_y;
    int mb_x = *pmb_x;
    json_object *jplane;

    mb_y *= ctx->v_count[component];
    mb_x *= ctx->h_count[component];

    mb_y += block / ctx->h_count[component];
    mb_x += block % ctx->h_count[component];

    *pmb_y = mb_y;
    *pmb_x = mb_x;

    jplane = json_object_array_get_idx(ctx->jdata, component);

    return json_object_array_get_idx(jplane, mb_y);
}

//---------------------------------------------------------------------
json_object *
cas9_jmb_get(
        json_object *s,
        int component,
        int mb_y,
        int mb_x,
        int block)
{
    json_object *jline = cas9_jmb_select(s, component, &mb_y, &mb_x, block);
    return json_object_array_get_idx(jline, mb_x);
}

//---------------------------------------------------------------------
void
cas9_jmb_set(
        json_object *s,
        int component,
        int mb_y,
        int mb_x,
        int block,
        json_object *jval)
{
    json_object *jline = cas9_jmb_select(s, component, &mb_y, &mb_x, block);
    json_object_array_put_idx(jline, mb_x, jval);
}

//---------------------------------------------------------------------
void
cas9_json_array_zero_fill(
        json_object *jso,
        size_t len)
{
    for ( int i = 0; i < len; i++ )
        if ( json_object_array_get_idx(jso, i) == NULL )
            json_object_array_put_idx(jso, i, json_object_new_int(0));
}


/* helper printing functions */

//---------------------------------------------------------------------
static av_always_inline int
intx_line_to_json_string(
        json_object *jso,
        struct printbuf *pb,
        int level,
        int flags,
        int arr_len)
{
    size_t length = json_object_array_length(jso);
    int num = (int) json_object_get_userdata(jso);
    char fmt[5];
    int null_len = num;

    fmt[0] = '%';
    if ( num > 0 )
    {
        fmt[1] = ' ';
        fmt[2] = '0' + num;
        fmt[3] = 'd';
        fmt[4] = '\0';
        null_len = 1 + arr_len * (num + 1);
    }
    else
    {
        fmt[1] = 'd';
        fmt[2] = '\0';
    }

    printbuf_strappend(pb, "[ ");

    for ( size_t i = 0; i < length; i++ )
    {
        json_object *jval = json_object_array_get_idx(jso, i);
        char sbuf[21];

        if ( i != 0 )
            printbuf_strappend(pb, ", ");

        if ( jval == NULL )
        {
            snprintf(sbuf, sizeof(sbuf), "%-*s", null_len, "null");
            printbuf_memappend(pb, sbuf, strlen(sbuf));
        }
        else if ( json_object_get_userdata(jval) )
        {
            const char *s;

            s = json_object_to_json_string_ext(jval, JSON_C_TO_STRING_PRETTY);
            printbuf_memappend(pb, s, strlen(s));
        }
        else
        {
            if ( arr_len > 1 )
                printbuf_strappend(pb, "[");
            for ( size_t j = 0; j < arr_len; j++ )
            {
                json_object *jval2 = jval;
                if ( j != 0 )
                    printbuf_strappend(pb, ",");
                if ( arr_len > 1 )
                    jval2 = json_object_array_get_idx(jval2, j);
                snprintf(sbuf, sizeof(sbuf), fmt, json_object_get_int(jval2));
                printbuf_memappend(pb, sbuf, strlen(sbuf));
            }
            if ( arr_len > 1 )
                printbuf_strappend(pb, "]");
        }
    }

    return printbuf_strappend(pb, " ]");
}

//---------------------------------------------------------------------
int
cas9_int2_line_to_json_string(
        json_object *jso,
        struct printbuf *pb,
        int level,
        int flags)
{
    return intx_line_to_json_string(jso, pb, level, flags, 2);
}

//---------------------------------------------------------------------
int
cas9_int_line_to_json_string(
        json_object *jso,
        struct printbuf *pb,
        int level,
        int flags)
{
    return intx_line_to_json_string(jso, pb, level, flags, 1);
}

//---------------------------------------------------------------------
int
cas9_array_line_to_json_string(
        json_object *jso,
        struct printbuf *pb,
        int level,
        int flags)
{
    size_t length = json_object_array_length(jso);

    printbuf_strappend(pb, "[ ");

    for ( size_t i = 0; i < length; i++ )
    {
        json_object *jval = json_object_array_get_idx(jso, i);
        const char *s;

        s = json_object_to_json_string_ext(jval, JSON_C_TO_STRING_PRETTY);

        if ( i != 0 )
            printbuf_strappend(pb, ", ");

        printbuf_memappend(pb, s, strlen(s));
    }

    return printbuf_strappend(pb, " ]");
}

//---------------------------------------------------------------------
void cas9_free_userdata(struct json_object *jso, void *userdata)
{
    av_free(userdata);
}
