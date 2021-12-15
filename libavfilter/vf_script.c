/*
 * Copyright (C) 2021-2022 Ramiro Polla
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

#include "libavutil/avassert.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/quickjs/quickjs-libc.h"
#include "libavutil/script.h"
#include "libavutil/time.h"
#include "libavformat/avformat.h"
#include "formats.h"
#include "internal.h"
#include "pixelsort.h"
#include "transpose.h"
#include "video.h"

/*********************************************************************/
typedef struct ScriptContext {
    const AVClass *class;
    char *script_fname;
    char *script_params;

    FFScriptContext *script;
    FFScriptObject *filter_func;

    const AVPixFmtDescriptor *desc;
    int format;
    const char *pix_fmt;
    int hsub, vsub;             ///< chroma subsampling
    int planes;                 ///< number of planes

    AVFilterContext *avfictx;
} ScriptContext;

/*********************************************************************/
#define OFFSET(x) offsetof(ScriptContext, x)
#define FLAGS AV_OPT_FLAG_VIDEO_PARAM|AV_OPT_FLAG_FILTERING_PARAM

static const AVOption script_options[] = {
    { "file", "JavaScript or Python3 file", OFFSET(script_fname), AV_OPT_TYPE_STRING, { .str=NULL }, 0, 0, FLAGS },
    { "params", "setup() parameters", OFFSET(script_params), AV_OPT_TYPE_STRING, { .str=NULL }, 0, 0, FLAGS },
    { NULL },
};

AVFILTER_DEFINE_CLASS(script);

/*********************************************************************/
static void get_ptr_and_linesize(
        JSContext *ctx,
        JSValue data,
        size_t idx,
        uint8_t **pptr,
        int *plinesize)
{
    JSValue plane = JS_GetPropertyUint32(ctx, data, idx);
    JSValue jptr = JS_GetPropertyStr(ctx, plane, "ptr");
    JSValue jlinesize = JS_GetPropertyStr(ctx, plane, "linesize");
    *pptr = (uint8_t *) JS_VALUE_GET_PTR(jptr);
    *plinesize = JS_VALUE_GET_INT(jlinesize);
    JS_FreeValue(ctx, jlinesize);
    JS_FreeValue(ctx, jptr);
    JS_FreeValue(ctx, plane);
}

static void scale_thresholds(PixelSortThreadData *td)
{
    if ( td->sort_colorspace_is_yuv || td->sort_colorspace_is_rgb )
    {
        td->lower_threshold = roundf(td->lower_threshold * 255.);
        td->upper_threshold = roundf(td->upper_threshold * 255.);
    }
    if ( td->sort_colorspace_is_hsv )
    {
        if ( td->trigger_by_n == 2 )
        {
            td->lower_threshold = roundf(td->lower_threshold * 255.);
            td->upper_threshold = roundf(td->upper_threshold * 255.);
        }
    }
    else if ( td->sort_colorspace_is_hsl )
    {
        if ( td->trigger_by_n == 2 )
        {
            td->lower_threshold = roundf(td->lower_threshold * 510.);
            td->upper_threshold = roundf(td->upper_threshold * 510.);
        }
    }
}

static JSValue js_ffgac_pixelsort(
        JSContext *ctx,
        JSValueConst this_val,
        int argc,
        JSValueConst *argv)
{
    // ffgac.pixelsort(data, [ 0, height ], [ 0, width ], options);

    ScriptContext *sctx = (ScriptContext *) JS_GetContextOpaque(ctx);
    const int nb_threads = ff_filter_get_nb_threads(sctx->avfictx);
    // const int nb_threads = 1;
    PixelSortThreadData td;

    JSValue data = argv[0];
    JSValue jrange_y = argv[1];
    JSValue jrange_x = argv[2];
    JSValue joptions = argv[3];
    JSValue jmode = JS_NULL;
    JSValue jreverse_sort = JS_NULL;
    JSValue jcolorspace = JS_NULL;
    JSValue jorder = JS_NULL;
    JSValue jdata_pix_fmt = JS_NULL;
    JSValue jdata_format = JS_NULL;
    JSValue jtrigger_by = JS_NULL;
    JSValue jsort_by = JS_NULL;
    const char *mode = NULL;
    const char *colorspace = NULL;
    const char *order = NULL;
    const char *data_pix_fmt = NULL;
    int data_format;
    const char *trigger_by = NULL;
    const char *sort_by = NULL;
    int pix_fmt_is_yuv444p;
    int pix_fmt_is_gbrp;
    int order_is_horizontal;
    int order_is_vertical;

    JSValue *prange_y;
    JSValue *prange_x;
    uint32_t range_y_length;
    uint32_t range_x_length;
    int length_y;
    int length_x;

    if ( !JS_GetFastArray(jrange_y, &prange_y, &range_y_length)
      || !JS_GetFastArray(jrange_x, &prange_x, &range_x_length)
      || range_y_length != 2
      || range_x_length != 2 )
    {
        JS_ThrowTypeError(ctx, "pixelsort(data, [ range y ], [ range x ], options)");
        return JS_EXCEPTION;
    }
    td.start_y = JS_VALUE_GET_INT(prange_y[0]);
    td.end_y = JS_VALUE_GET_INT(prange_y[1]);
    length_y = td.end_y - td.start_y;

    td.start_x = JS_VALUE_GET_INT(prange_x[0]);
    td.end_x = JS_VALUE_GET_INT(prange_x[1]);
    length_x = td.end_x - td.start_x;

    if ( length_y <= 0 || length_x <= 0 )
    {
        JS_ThrowTypeError(ctx, "end must be greater than start");
        return JS_EXCEPTION;
    }

    jmode = JS_GetPropertyStr(ctx, joptions, "mode");
    if ( !JS_IsString(jmode) )
    {
mode_error:
        JS_ThrowTypeError(ctx, "options.mode must be one of \"threshold\" or \"random\"");
        return JS_EXCEPTION;
    }
    mode = JS_ToCString(ctx, jmode);
    td.mode_is_threshold = (strcmp(mode, "threshold") == 0);
    td.mode_is_random = (strcmp(mode, "random") == 0);
    if ( !td.mode_is_threshold && !td.mode_is_random )
        goto mode_error;

    jreverse_sort = JS_GetPropertyStr(ctx, joptions, "reverse_sort");
    if ( !JS_IsBool(jreverse_sort) )
    {
        JS_ThrowTypeError(ctx, "options.reverse_sort must be one of \"true\" or \"false\"");
        return JS_EXCEPTION;
    }
    td.reverse_sort = (JS_VALUE_GET_INT(jreverse_sort) != 0);

    jcolorspace = JS_GetPropertyStr(ctx, joptions, "colorspace");
    if ( !JS_IsString(jcolorspace) )
    {
colorspace_error:
        JS_ThrowTypeError(ctx, "colorspace must be one of \"yuv\", \"rgb\", \"hsv\", or \"hsl\"");
        return JS_EXCEPTION;
    }
    colorspace = JS_ToCString(ctx, jcolorspace);
    td.sort_colorspace_is_yuv = (strcmp(colorspace, "yuv") == 0);
    td.sort_colorspace_is_rgb = (strcmp(colorspace, "rgb") == 0);
    td.sort_colorspace_is_hsv = (strcmp(colorspace, "hsv") == 0);
    td.sort_colorspace_is_hsl = (strcmp(colorspace, "hsl") == 0);
    if ( !td.sort_colorspace_is_yuv
      && !td.sort_colorspace_is_rgb
      && !td.sort_colorspace_is_hsv
      && !td.sort_colorspace_is_hsl )
    {
        goto colorspace_error;
    }

    jorder = JS_GetPropertyStr(ctx, joptions, "order");
    if ( !JS_IsString(jorder) )
    {
order_error:
        JS_ThrowTypeError(ctx, "options.order must be one of \"horizontal\" or \"vertical\"");
        return JS_EXCEPTION;
    }
    order = JS_ToCString(ctx, jorder);
    order_is_horizontal = (strcmp(order, "horizontal") == 0);
    order_is_vertical   = (strcmp(order, "vertical")   == 0);
    if ( !order_is_horizontal
      && !order_is_vertical )
    {
        goto order_error;
    }

    /* check data's pix_fmt */
    jdata_pix_fmt = JS_GetPropertyStr(ctx, data, "pix_fmt");
    if ( !JS_IsString(jdata_pix_fmt) )
    {
        JS_ThrowTypeError(ctx, "data.pix_fmt is not a string (?)");
        return JS_EXCEPTION;
    }
    data_pix_fmt = JS_ToCString(ctx, jdata_pix_fmt);
    pix_fmt_is_yuv444p = (strcmp(data_pix_fmt, "yuv444p") == 0);
    pix_fmt_is_gbrp = (strcmp(data_pix_fmt, "gbrp") == 0);
    if ( pix_fmt_is_yuv444p && pix_fmt_is_gbrp )
    {
        JS_ThrowTypeError(ctx, "only yuv444p and gbrp pix_fmt supported for now");
        return JS_EXCEPTION;
    }
    if ( td.sort_colorspace_is_yuv && !pix_fmt_is_yuv444p )
    {
        JS_ThrowTypeError(ctx, "sort_by \"yuv\" requested but data pix_fmt is not yuv444p");
        return JS_EXCEPTION;
    }
    if ( td.sort_colorspace_is_rgb && !pix_fmt_is_gbrp )
    {
        JS_ThrowTypeError(ctx, "sort_by \"rgb\" requested but data pix_fmt is not gbrp");
        return JS_EXCEPTION;
    }
    if ( td.sort_colorspace_is_hsv && !pix_fmt_is_gbrp )
    {
        JS_ThrowTypeError(ctx, "sort_by \"hsv\" requested but data pix_fmt is not gbrp");
        return JS_EXCEPTION;
    }
    if ( td.sort_colorspace_is_hsl && !pix_fmt_is_gbrp )
    {
        JS_ThrowTypeError(ctx, "sort_by \"hsl\" requested but data pix_fmt is not gbrp");
        return JS_EXCEPTION;
    }

    jtrigger_by = JS_GetPropertyStr(ctx, joptions, "trigger_by");
    if ( !JS_IsString(jtrigger_by) )
    {
        JS_ThrowTypeError(ctx, "options.trigger_by must be a string");
        return JS_EXCEPTION;
    }
    trigger_by = JS_ToCString(ctx, jtrigger_by);
    if ( td.sort_colorspace_is_yuv )
    {
        switch ( trigger_by[0] )
        {
            case 'y': td.trigger_by_n = 0; break;
            case 'u': td.trigger_by_n = 1; break;
            case 'v': td.trigger_by_n = 2; break;
            default:
                JS_ThrowTypeError(ctx, "options.trigger_by with colorspace \"yuv\" must be \"y\", \"u\", or \"v\"");
                return JS_EXCEPTION;
        }
    }
    else if ( td.sort_colorspace_is_rgb )
    {
        switch ( trigger_by[0] )
        {
            case 'r': td.trigger_by_n = 2; break;
            case 'g': td.trigger_by_n = 0; break;
            case 'b': td.trigger_by_n = 1; break;
            default:
                JS_ThrowTypeError(ctx, "options.trigger_by with colorspace \"rgb\" must be \"r\", \"g\", or \"b\"");
                return JS_EXCEPTION;
        }
    }
    else if ( td.sort_colorspace_is_hsv )
    {
        switch ( trigger_by[0] )
        {
            case 'h': td.trigger_by_n = 0; break;
            case 's': td.trigger_by_n = 1; break;
            case 'v': td.trigger_by_n = 2; break;
            default:
                JS_ThrowTypeError(ctx, "options.trigger_by with colorspace \"hsv\" must be \"h\", \"s\", or \"v\"");
                return JS_EXCEPTION;
        }
    }
    else /* if ( td.sort_colorspace_is_hsl ) */
    {
        switch ( trigger_by[0] )
        {
            case 'h': td.trigger_by_n = 0; break;
            case 's': td.trigger_by_n = 1; break;
            case 'l': td.trigger_by_n = 2; break;
            default:
                JS_ThrowTypeError(ctx, "options.trigger_by with colorspace \"hsl\" must be \"h\", \"s\", or \"v\"");
                return JS_EXCEPTION;
        }
    }
    JS_FreeValue(ctx, jtrigger_by);

    jsort_by = JS_GetPropertyStr(ctx, joptions, "sort_by");
    if ( !JS_IsString(jsort_by) )
    {
        JS_ThrowTypeError(ctx, "options.sort_by must be a string");
        return JS_EXCEPTION;
    }
    sort_by = JS_ToCString(ctx, jsort_by);
    if ( td.sort_colorspace_is_yuv )
    {
        switch ( sort_by[0] )
        {
            case 'y': td.sort_by_n = 0; break;
            case 'u': td.sort_by_n = 1; break;
            case 'v': td.sort_by_n = 2; break;
            default:
                JS_ThrowTypeError(ctx, "options.sort_by with colorspace \"yuv\" must be \"y\", \"u\", or \"v\"");
                return JS_EXCEPTION;
        }
    }
    else if ( td.sort_colorspace_is_rgb )
    {
        switch ( sort_by[0] )
        {
            case 'r': td.sort_by_n = 2; break;
            case 'g': td.sort_by_n = 0; break;
            case 'b': td.sort_by_n = 1; break;
            default:
                JS_ThrowTypeError(ctx, "options.sort_by with colorspace \"rgb\" must be \"r\", \"g\", or \"b\"");
                return JS_EXCEPTION;
        }
    }
    else if ( td.sort_colorspace_is_hsv )
    {
        switch ( sort_by[0] )
        {
            case 'h': td.sort_by_n = 0; break;
            case 's': td.sort_by_n = 1; break;
            case 'v': td.sort_by_n = 2; break;
            default:
                JS_ThrowTypeError(ctx, "options.sort_by with colorspace \"hsv\" must be \"h\", \"s\", or \"v\"");
                return JS_EXCEPTION;
        }
    }
    else /* if ( td.sort_colorspace_is_hsl ) */
    {
        switch ( sort_by[0] )
        {
            case 'h': td.sort_by_n = 0; break;
            case 's': td.sort_by_n = 1; break;
            case 'l': td.sort_by_n = 2; break;
            default:
                JS_ThrowTypeError(ctx, "options.sort_by with colorspace \"hsl\" must be \"h\", \"s\", or \"l\"");
                return JS_EXCEPTION;
        }
    }
    JS_FreeValue(ctx, jsort_by);

    /* check data's format */
    jdata_format = JS_GetPropertyStr(ctx, data, "format");
    if ( !JS_IsInt32(jdata_format) )
    {
        JS_ThrowTypeError(ctx, "data.format is not an int (?)");
        return JS_EXCEPTION;
    }
    data_format = JS_VALUE_GET_INT(jdata_format);

    if ( td.mode_is_threshold )
    {
        JSValue jthreshold = JS_GetPropertyStr(ctx, joptions, "threshold");
        JSValue *pthreshold;
        uint32_t threshold_length;
        double dummy;
        if ( JS_IsArray(ctx, jthreshold) <= 0
          || !JS_GetFastArray(jthreshold, &pthreshold, &threshold_length)
          || threshold_length != 2
          || !JS_IsNumber(pthreshold[0])
          || !JS_IsNumber(pthreshold[1]) )
        {
            JS_ThrowTypeError(ctx, "options.threshold must be an array with two numbers (lower and upper)");
            return JS_EXCEPTION;
        }
        JS_ToFloat64(ctx, &dummy, pthreshold[0]);
        td.lower_threshold = dummy;
        JS_ToFloat64(ctx, &dummy, pthreshold[1]);
        td.upper_threshold = dummy;
        scale_thresholds(&td);
        td.reverse_range = (td.lower_threshold > td.upper_threshold);
        if ( td.reverse_range )
            FFSWAP(float, td.lower_threshold, td.upper_threshold);
        JS_FreeValue(ctx, jthreshold);
    }
    else if ( td.mode_is_random )
    {
        JSValue jclength = JS_GetPropertyStr(ctx, joptions, "clength");
        if ( !JS_IsInt32(jclength) )
        {
            JS_ThrowTypeError(ctx, "options.clength must be an integer");
            return JS_EXCEPTION;
        }
        td.clength = JS_VALUE_GET_INT(jclength);
        if ( td.clength == 0 )
            td.clength = 1;
        JS_FreeValue(ctx, jclength);
    }

    /* get ptr and linesize for each plane */
    get_ptr_and_linesize(ctx, data, 0, &td.src_0, &td.linesizes[0]);
    get_ptr_and_linesize(ctx, data, 1, &td.src_1, &td.linesizes[1]);
    get_ptr_and_linesize(ctx, data, 2, &td.src_2, &td.linesizes[2]);

    if ( order_is_horizontal )
    {
        td.cur_y = td.start_y;
        ff_pixelsort_mutex_init(&td);
        ff_filter_execute(sctx->avfictx, ff_pixelsort_slice, &td, NULL, nb_threads);
        ff_pixelsort_mutex_destroy(&td);
    }
    else /* if ( order_is_vertical ) */
    {
        AVFrame tmp_frame = { .width = length_y, .height = length_x, .format = data_format };
        AVFrame src_frame = { .width = length_x, .height = length_y, .format = data_format };
        TransContext cclock = { sctx->class, 0, 0, sctx->planes, { 0, 0, 0, 0 }, TRANSPOSE_PT_TYPE_NONE, TRANSPOSE_CCLOCK };
        TransContext clock = { sctx->class, 0, 0, sctx->planes, { 0, 0, 0, 0 }, TRANSPOSE_PT_TYPE_NONE, TRANSPOSE_CLOCK };
        PixelSortThreadData td2 = td;
        VFTransposeThreadData cclock_td = { &src_frame, &tmp_frame, &cclock };
        VFTransposeThreadData clock_td = { &tmp_frame, &src_frame, &clock };

        /* init tmp frame */
        av_frame_get_buffer(&tmp_frame, 0);

        /* init src frame */
        src_frame.data[0] = &td.src_0[td.start_y * td.linesizes[0] + td.start_x];
        src_frame.data[1] = &td.src_1[td.start_y * td.linesizes[1] + td.start_x];
        src_frame.data[2] = &td.src_2[td.start_y * td.linesizes[2] + td.start_x];
        src_frame.data[3] = NULL;
        src_frame.linesize[0] = td.linesizes[0];
        src_frame.linesize[1] = td.linesizes[1];
        src_frame.linesize[2] = td.linesizes[2];
        src_frame.linesize[3] = 0;

        /* transpose counter clockwise */
        ff_vf_transpose_init(&cclock, sctx->desc, sctx->desc, sctx->format);
        ff_filter_execute(sctx->avfictx, ff_vf_transpose_filter_slice, &cclock_td, NULL, nb_threads);

        /* pixelsort by rows */
        td2.start_y = 0;
        td2.end_y = length_x;
        td2.start_x = 0;
        td2.end_x = length_y;
        td2.src_0 = tmp_frame.data[0];
        td2.src_1 = tmp_frame.data[1];
        td2.src_2 = tmp_frame.data[2];
        td2.linesizes[0] = tmp_frame.linesize[0];
        td2.linesizes[1] = tmp_frame.linesize[1];
        td2.linesizes[2] = tmp_frame.linesize[2];
        td2.cur_y = td2.start_y;
        ff_pixelsort_mutex_init(&td2);
        ff_filter_execute(sctx->avfictx, ff_pixelsort_slice, &td2, NULL, nb_threads);
        ff_pixelsort_mutex_destroy(&td2);

        /* transpose clockwise */
        ff_vf_transpose_init(&clock, sctx->desc, sctx->desc, sctx->format);
        ff_filter_execute(sctx->avfictx, ff_vf_transpose_filter_slice, &clock_td, NULL, nb_threads);

        /* free frame */
        av_frame_unref(&tmp_frame);
    }

    JS_FreeCString(ctx, mode);
    JS_FreeCString(ctx, colorspace);
    JS_FreeCString(ctx, order);
    JS_FreeCString(ctx, data_pix_fmt);
    JS_FreeCString(ctx, trigger_by);
    JS_FreeCString(ctx, sort_by);

    JS_FreeValue(ctx, jdata_format);
    JS_FreeValue(ctx, jdata_pix_fmt);
    JS_FreeValue(ctx, jmode);
    JS_FreeValue(ctx, jreverse_sort);
    JS_FreeValue(ctx, jcolorspace);
    JS_FreeValue(ctx, jorder);

    return JS_TRUE;
}

static const JSCFunctionListEntry js_ffgac_funcs[] = {
    JS_CFUNC_DEF("pixelsort", 4, js_ffgac_pixelsort),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "ffgac", JS_PROP_CONFIGURABLE),
};

static const JSCFunctionListEntry js_ffgac_obj[] = {
    JS_OBJECT_DEF("ffgac", js_ffgac_funcs, FF_ARRAY_ELEMS(js_ffgac_funcs), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE),
};

/*********************************************************************/
static int filter_frame(ScriptContext *sctx, AVFilterLink *inlink, AVFrame *out, int64_t frame_num, double pts)
{
#define TIME_SCRIPT 0
#if TIME_SCRIPT
    int64_t convert1 = 0;
    int64_t call = 0;
    int64_t convert2 = 0;
    int64_t t0;
    int64_t t1;
#endif
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(inlink->format);
    FFScriptContext *script = sctx->script;
    FFScriptObject *func_args = NULL;
    int ret;

#if TIME_SCRIPT
    t0 = av_gettime_relative();
#endif

    /* convert to python/quickjs */
    if ( script->script_is_py )
    {
        FFPythonContext *py_ctx = (FFPythonContext *) script;
        PythonFunctions *pyfuncs = &py_ctx->pyfuncs;
        PyObject *py_args;
        PyObject *data;
        data = pyfuncs->PyList_New(sctx->planes);
        /* TODO add pix_fmt and format */
        for ( int p = 0; p < sctx->planes; p++ )
        {
            const int h = (p == 1 || p == 2) ? AV_CEIL_RSHIFT(inlink->h, sctx->vsub) : inlink->h;
            const int w = (p == 1 || p == 2) ? AV_CEIL_RSHIFT(inlink->w, sctx->hsub) : inlink->w;
            const int linesize = out->linesize[p];
            uint8_t *ptr = out->data[p];
            PyObject *plane = pyfuncs->PyList_New(h);
            for ( int i = 0; i < h; i++ )
            {
                PyObject *row = py_ctx->new_Uint8FFPtr(py_ctx, w, ptr);
                pyfuncs->PyList_SetItem(plane, i, row);
                ptr += linesize;
            }
            pyfuncs->PyList_SetItem(data, p, plane);
        }
        py_args = pyfuncs->PyDict_New();
        pyfuncs->PyDict_SetItemString(py_args, "frame_num", pyfuncs->PyLong_FromLong(frame_num));
        pyfuncs->PyDict_SetItemString(py_args, "pts", pyfuncs->PyFloat_FromDouble(pts));
        pyfuncs->PyDict_SetItemString(py_args, "data", data);
        func_args = (FFScriptObject *) py_args;
    }
    else
    {
        FFQuickJSContext *js_ctx = (FFQuickJSContext *) script;
        JSContext *qjs_ctx = js_ctx->ctx;
        JSValue jval;
        JSValue data;
        JSValue *pdata;

        data = JS_NewFastArray(qjs_ctx, &pdata, sctx->planes, 1);
        JS_SetPropertyStr(qjs_ctx, data, "pix_fmt", JS_NewString(qjs_ctx, desc->name));
        JS_SetPropertyStr(qjs_ctx, data, "format", JS_NewInt32(qjs_ctx, inlink->format));
        for ( int p = 0; p < sctx->planes; p++ )
        {
            const int h = (p == 1 || p == 2) ? AV_CEIL_RSHIFT(inlink->h, sctx->vsub) : inlink->h;
            const int w = (p == 1 || p == 2) ? AV_CEIL_RSHIFT(inlink->w, sctx->hsub) : inlink->w;
            const int linesize = out->linesize[p];
            uint8_t *ptr = out->data[p];
            JSValue plane;
            JSValue *pplane;

            plane = JS_NewFastArray(qjs_ctx, &pplane, h, 1);
            JS_SetPropertyStr(qjs_ctx, plane, "ptr", JS_MKPTR(JS_TAG_INT, ptr));
            JS_SetPropertyStr(qjs_ctx, plane, "linesize", JS_NewInt32(qjs_ctx, linesize));
            JS_SetPropertyStr(qjs_ctx, plane, "width", JS_NewInt32(qjs_ctx, w));
            JS_SetPropertyStr(qjs_ctx, plane, "height", JS_NewInt32(qjs_ctx, h));
            for ( int i = 0; i < h; i++ )
            {
                *pplane++ = JS_NewUint8FFPtr(qjs_ctx, ptr, w);
                ptr += linesize;
            }
            *pdata++ = plane;
        }
        jval = JS_NewObject(qjs_ctx);
        JS_SetPropertyStr(qjs_ctx, jval, "frame_num", JS_NewInt64(qjs_ctx, frame_num));
        JS_SetPropertyStr(qjs_ctx, jval, "pts", JS_NewFloat64(qjs_ctx, pts));
        JS_SetPropertyStr(qjs_ctx, jval, "data", data);
        func_args = (FFScriptObject *) av_malloc(sizeof(FFQuickJSObject));
        ((FFQuickJSObject *)func_args)->jval = jval;
    }

#if TIME_SCRIPT
    t1 = av_gettime_relative();
    convert1 += (t1 - t0);
    t0 = t1;
#endif

    /* call filter() with data */
    ret = ff_script_call_func(script, NULL, sctx->filter_func, func_args, NULL);

#if TIME_SCRIPT
    t1 = av_gettime_relative();
    call += (t1 - t0);
    t0 = t1;
#endif

    ff_script_free_obj(script, func_args);

    if ( ret < 0 )
    {
        av_log(sctx, AV_LOG_FATAL, "Error calling filter() function in %s\n", sctx->script_fname);
        return AVERROR(EINVAL);
    }

    /* convert back from python/quickjs */
    /* (nothing to do) */

#if TIME_SCRIPT
    t1 = av_gettime_relative();
    convert2 += (t1 - t0);
    t0 = t1;
#endif

#if TIME_SCRIPT
    printf("time taken convert1 %" PRId64 " call %" PRId64 " convert2 %" PRId64 "\n", convert1, call, convert2);
#endif

    return 0;
}

/*********************************************************************/
static av_cold int script_init(AVFilterContext *ctx)
{
    ScriptContext *sctx = (ScriptContext *) ctx->priv;
    FFScriptContext *script;
    FFScriptObject *setup_func;
    int ret = 0;

    script = ff_script_init(sctx->script_fname, 0);
    if ( script == NULL )
    {
        ret = AVERROR(EINVAL);
        goto end;
    }

    /* get functions */
    setup_func = ff_script_get_func(script, "setup", 0);
    sctx->filter_func = ff_script_get_func(script, "filter", 1);

    /* filter() is mandatory */
    if ( sctx->filter_func == NULL )
    {
        ret = AVERROR(EINVAL);
        goto end;
    }

    /* init extra quickjs features */
    if ( script->script_is_js )
    {
        FFQuickJSContext *js_ctx = (FFQuickJSContext *) script;
        JSContext *qjs_ctx = js_ctx->ctx;
        /* ffgac */
        JS_SetPropertyFunctionList(qjs_ctx, js_ctx->global_object, js_ffgac_obj, FF_ARRAY_ELEMS(js_ffgac_obj));
        /* pixelsort */
        JS_SetContextOpaque(qjs_ctx, sctx);
    }

    /* run setup */
    if ( setup_func != NULL )
    {
        const char *script_params = sctx->script_params;
        json_ctx_t jctx;
        json_t *args = NULL;
        json_t *jpix_fmt;
        FFScriptObject *func_args = NULL;

        /* prepare args */
        json_ctx_start(&jctx, 0);
        args = json_object_new(&jctx);
        if ( script_params != NULL )
        {
            json_t *params = json_parse(&jctx, script_params);
            if ( params == NULL )
            {
                const char *fname = "<params>";
                json_error_ctx_t jectx;
                json_error_parse(&jectx, script_params);
                av_log(sctx, AV_LOG_FATAL, "%s:%d:%d: %s\n",
                       fname, (int) jectx.line, (int) jectx.offset, jectx.str);
                av_log(sctx, AV_LOG_FATAL, "%s:%d:%s\n", fname, (int) jectx.line, jectx.buf);
                av_log(sctx, AV_LOG_FATAL, "%s:%d:%s\n", fname, (int) jectx.line, jectx.column);
                json_error_free(&jectx);
                exit(1);
            }
            json_object_add(args, "params", params);
        }
        json_object_done(&jctx, args);

        /* convert to python/quickjs */
        func_args = ff_script_from_json(script, args);

        /* call setup() */
        ret = ff_script_call_func(script, NULL, setup_func, func_args, NULL);
        if ( ret < 0 )
        {
            av_log(sctx, AV_LOG_FATAL, "Error calling setup() function in %s\n", sctx->script_fname);
            ret = AVERROR(EINVAL);
            goto setup_end;
        }

        /* convert back from python/quickjs */
        args = ff_script_to_json(&jctx, script, func_args);

        /* check returned pix_fmt */
        jpix_fmt = json_object_get(args, "pix_fmt");
        if ( jpix_fmt != NULL )
        {
            if ( JSON_TYPE(jpix_fmt->flags) != JSON_TYPE_STRING )
            {
                av_log(sctx, AV_LOG_FATAL, "args[\"pix_fmt\"] returned from setup() must be a string!\n");
                ret = AVERROR(EINVAL);
                goto setup_end;
            }
            sctx->pix_fmt = av_strdup(json_string_get(jpix_fmt));
        }

setup_end:
        /* free stuff */
        ff_script_free_obj(script, func_args);
        ff_script_free_obj(script, setup_func);
        json_ctx_free(&jctx);
        if ( ret < 0 )
            goto end;
    }

    ret = 0;

end:
    if ( ret != 0 )
        ff_script_uninit(&script);
    sctx->script = script;

    return ret;
}

/*********************************************************************/
static int script_query_formats(AVFilterContext *ctx)
{
    ScriptContext *sctx = (ScriptContext *) ctx->priv;
    static const int yuv_pix_fmts[] = {
        AV_PIX_FMT_YUV444P,  AV_PIX_FMT_YUV422P,  AV_PIX_FMT_YUV420P,
        AV_PIX_FMT_YUV411P,  AV_PIX_FMT_YUV410P,  AV_PIX_FMT_YUV440P,
        AV_PIX_FMT_YUVA444P, AV_PIX_FMT_YUVA422P, AV_PIX_FMT_YUVA420P,
        AV_PIX_FMT_GRAY8,
        AV_PIX_FMT_GBRP, AV_PIX_FMT_GBRAP,
        AV_PIX_FMT_NONE
    };
    AVFilterFormats *fmts_list;

    if ( sctx->pix_fmt != NULL )
    {
        int tmp_pix_fmts[2] = { AV_PIX_FMT_NONE, AV_PIX_FMT_NONE };
        tmp_pix_fmts[0] = av_get_pix_fmt(sctx->pix_fmt);
        if ( tmp_pix_fmts[0] == AV_PIX_FMT_NONE )
        {
            av_log(sctx, AV_LOG_ERROR, "Unknown pix_fmt \"%s\"\n", sctx->pix_fmt);
            return AVERROR(EINVAL);
        }
        fmts_list = ff_make_format_list(tmp_pix_fmts);
    }
    else
    {
        fmts_list = ff_make_format_list(yuv_pix_fmts);
    }
    if ( !fmts_list )
        return AVERROR(ENOMEM);

    return ff_set_common_formats(ctx, fmts_list);
}

/*********************************************************************/
static int script_config_props(AVFilterLink *inlink)
{
    ScriptContext *sctx = (ScriptContext *) inlink->dst->priv;
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(inlink->format);

    av_assert0(desc);

    sctx->desc = desc;
    sctx->format = inlink->format;
    sctx->hsub = desc->log2_chroma_w;
    sctx->vsub = desc->log2_chroma_h;
    sctx->planes = desc->nb_components;

    return 0;
}

/*********************************************************************/
static int script_filter_frame(AVFilterLink *inlink, AVFrame *in)
{
    AVFilterContext *ctx = inlink->dst;
    AVFilterLink *outlink = inlink->dst->outputs[0];
    AVFrame *out = ff_get_video_buffer(outlink, in->width, in->height);
    int64_t frame_num = inlink->frame_count_out;
    double pts = (in->pts == AV_NOPTS_VALUE) ? NAN : (in->pts * av_q2d(inlink->time_base));
    ScriptContext *sctx = (ScriptContext *) ctx->priv;
    int ret;

    if ( !out )
    {
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    ret = av_frame_copy_props(out, in);
    if ( ret < 0 )
        goto fail;
    ret = av_frame_copy(out, in);
    if ( ret < 0 )
        goto fail;

    sctx->avfictx = ctx;
    ret = filter_frame(sctx, inlink, out, frame_num, pts);
    if ( ret < 0 )
        goto fail;

    av_frame_free(&in);
    return ff_filter_frame(outlink, out);
fail:
    av_frame_free(&in);
    av_frame_free(&out);
    return ret;
}

/*********************************************************************/
static av_cold void script_uninit(AVFilterContext *ctx)
{
    ScriptContext *sctx = (ScriptContext *) ctx->priv;
    FFScriptContext *script = sctx->script;
    if ( script != NULL )
    {
        ff_script_free_obj(script, sctx->filter_func);
        ff_script_uninit(&script);
    }
}

/*********************************************************************/
static const AVFilterPad script_inputs[] = {
    {
        .name         = "default",
        .type         = AVMEDIA_TYPE_VIDEO,
        .filter_frame = script_filter_frame,
        .config_props = script_config_props,
    },
};

/*********************************************************************/
static const AVFilterPad script_outputs[] = {
    {
        .name = "default",
        .type = AVMEDIA_TYPE_VIDEO,
    },
};

/*********************************************************************/
const AVFilter ff_vf_script = {
    .name          = "script",
    .description   = NULL_IF_CONFIG_SMALL("Run external script on data"),
    .inputs        = script_inputs,
    .outputs       = script_outputs,
    .priv_class    = &script_class,
    .flags         = AVFILTER_FLAG_SUPPORT_TIMELINE_GENERIC | AVFILTER_FLAG_SLICE_THREADS,
    .nb_inputs     = 1,
    .nb_outputs    = 1,
    .formats_state = FF_FILTER_FORMATS_QUERY_FUNC,
    .init          = script_init,
    .uninit        = script_uninit,
    .formats       = { .query_func = script_query_formats },
    .priv_size     = sizeof(ScriptContext),
};
