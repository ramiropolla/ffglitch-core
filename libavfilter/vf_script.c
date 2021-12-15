/*
 * Copyright (C) 2021 Ramiro Polla
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
#include "libavutil/time.h"
#include "internal.h"

#include "compat/load_python.c"

/*********************************************************************/
typedef struct QuickJSContext {
    JSRuntime *rt;
    JSContext *ctx;
    JSValue global_object;
    JSValue filter_func;
    JSValue ctor_UintC8Array;
} QuickJSContext;

#define QUICKJS_ZERO_COPY 1
#define PYTHON_ZERO_COPY  1

/*********************************************************************/
typedef struct PythonContext {
    void *libpython_so;

    PythonFunctions pyfuncs;

    PyObject *PyExc_IndexError;
    PyObject *UintC8Array;

    PyObject *globals;
    PyObject *module;
    PyObject *locals;
    PyObject *builtins;
    PyObject *filter_func;
} PythonContext;

/*********************************************************************/
typedef struct ScriptContext {
    const AVClass *class;
    char *js_fname;
    char *py_fname;

    QuickJSContext qjs;
    PythonContext pctx;

    int hsub, vsub;             ///< chroma subsampling
    int planes;                 ///< number of planes
} ScriptContext;

/*********************************************************************/
#define OFFSET(x) offsetof(ScriptContext, x)
#define FLAGS AV_OPT_FLAG_VIDEO_PARAM|AV_OPT_FLAG_FILTERING_PARAM

static const AVOption script_options[] = {
    { "js", "JavaScript file", OFFSET(js_fname), AV_OPT_TYPE_STRING, { .str=NULL }, 0, 0, FLAGS },
    { "py", "Python file",     OFFSET(py_fname), AV_OPT_TYPE_STRING, { .str=NULL }, 0, 0, FLAGS },
    { NULL },
};

AVFILTER_DEFINE_CLASS(script);

/*********************************************************************/
static int quickjs_init(ScriptContext *sctx)
{
    QuickJSContext *qjs = &sctx->qjs;
    JSRuntime *qjs_rt;
    JSContext *qjs_ctx;
    JSValue val;
    uint8_t *buf;
    size_t len;
    int ret = 0;

    /* TODO check for errors */
    qjs_rt = JS_NewRuntime();
    js_std_init_handlers(qjs_rt);
    qjs_ctx = JS_NewContext(qjs_rt);

    /* loader for ES6 modules */
    JS_SetModuleLoaderFunc(qjs_rt, NULL, js_module_loader, NULL);
    js_std_add_helpers(qjs_ctx, 0, NULL);

    qjs->global_object = JS_GetGlobalObject(qjs_ctx);
#if QUICKJS_ZERO_COPY
    qjs->ctor_UintC8Array = JS_GetPropertyStr(qjs_ctx, qjs->global_object, "UintC8Array");
#else
    qjs->ctor_UintC8Array = JS_GetPropertyStr(qjs_ctx, qjs->global_object, "Uint8Array");
#endif

    /* system modules */
    js_init_module_std(qjs_ctx, "std");
    js_init_module_os(qjs_ctx, "os");

    /* load file */
    buf = js_load_file(qjs_ctx, &len, sctx->js_fname);
    if ( buf == NULL )
    {
        av_log(sctx, AV_LOG_FATAL, "Could not open script file %s\n", sctx->js_fname);
        ret = AVERROR(EINVAL);
        goto end;
    }

    val = JS_Eval(qjs_ctx, buf, len, sctx->js_fname, JS_EVAL_TYPE_GLOBAL);
    if ( JS_IsException(val) )
    {
        js_std_dump_error(qjs_ctx);
        ret = AVERROR(EINVAL);
        goto end;
    }
    JS_FreeValue(qjs_ctx, val);

    qjs->filter_func = JS_GetPropertyStr(qjs_ctx, qjs->global_object, "filter");
    if ( JS_IsUndefined(qjs->filter_func) )
    {
        av_log(sctx, AV_LOG_FATAL, "Could not find function filter() in %s\n", sctx->js_fname);
        ret = AVERROR(EINVAL);
        goto end;
    }

    qjs->rt = qjs_rt;
    qjs->ctx = qjs_ctx;

end:
    return ret;
}

/*********************************************************************/
static int quickjs_filter(ScriptContext *sctx, AVFilterLink *inlink, AVFrame *out, int64_t frame_num, double pts)
{
#define TIME_QUICKJS 0
#if TIME_QUICKJS
    int64_t convert1 = 0;
    int64_t call = 0;
    int64_t convert2 = 0;
    int64_t t0;
    int64_t t1;
#endif
    QuickJSContext *qjs = &sctx->qjs;
    JSContext *qjs_ctx = qjs->ctx;
    JSValue args;
    JSValue data;
    JSValue val;

#if TIME_QUICKJS
    t0 = av_gettime_relative();
#endif

    /* convert to quickjs */
    data = JS_NewArray(qjs_ctx);
    for ( int p = 0; p < sctx->planes; p++ )
    {
        const int h = (p == 1 || p == 2) ? AV_CEIL_RSHIFT(inlink->h, sctx->vsub) : inlink->h;
        const int w = (p == 1 || p == 2) ? AV_CEIL_RSHIFT(inlink->w, sctx->hsub) : inlink->w;
        const int linesize = out->linesize[p];
        uint8_t *ptr = out->data[p];
        JSValue plane = JS_NewArray(qjs_ctx);
        for ( int i = 0; i < h; i++ )
        {
#if QUICKJS_ZERO_COPY
            JSValueConst argv[2] = { JS_NewInt32(qjs_ctx, w), JS_MKPTR(JS_TAG_OBJECT, ptr) };
            JSValue row = JS_CallConstructor(qjs_ctx, qjs->ctor_UintC8Array, 2, argv);
#else
            JSValueConst argv[1] = { JS_NewInt32(qjs_ctx, w) };
            JSValue row = JS_CallConstructor(qjs_ctx, qjs->ctor_UintC8Array, 1, argv);
            for ( int j = 0; j < w; j++ )
            {
                JSValue pixval = JS_NewInt32(qjs_ctx, ptr[j]);
                JS_DefinePropertyValueUint32(qjs_ctx, row, j, pixval, JS_PROP_C_W_E);
            }
#endif
            JS_DefinePropertyValueUint32(qjs_ctx, plane, i, row, JS_PROP_C_W_E);
            ptr += linesize;
        }
        JS_DefinePropertyValueUint32(qjs_ctx, data, p, plane, JS_PROP_C_W_E);
    }
    args = JS_NewObject(qjs_ctx);
    JS_SetPropertyStr(qjs_ctx, args, "frame_num", JS_NewInt64(qjs_ctx, frame_num));
    JS_SetPropertyStr(qjs_ctx, args, "pts", JS_NewFloat64(qjs_ctx, pts));
    JS_SetPropertyStr(qjs_ctx, args, "data", data);

#if TIME_QUICKJS
    t1 = av_gettime_relative();
    convert1 += (t1 - t0);
    t0 = t1;
#endif

    /* call filter_func() with data */
    val = JS_Call(qjs_ctx, qjs->filter_func, JS_UNDEFINED, 1, &args);
    if ( JS_IsException(val) )
    {
        js_std_dump_error(qjs_ctx);
        return AVERROR(EINVAL);
    }
    JS_FreeValue(qjs_ctx, val);

#if TIME_QUICKJS
    t1 = av_gettime_relative();
    call += (t1 - t0);
    t0 = t1;
#endif

    /* convert back from quickjs */
#if !QUICKJS_ZERO_COPY
    for ( int p = 0; p < sctx->planes; p++ )
    {
        const int h = (p == 1 || p == 2) ? AV_CEIL_RSHIFT(inlink->h, sctx->vsub) : inlink->h;
        const int w = (p == 1 || p == 2) ? AV_CEIL_RSHIFT(inlink->w, sctx->hsub) : inlink->w;
        const int linesize = out->linesize[p];
        uint8_t *ptr = out->data[p];
        JSValue plane = JS_GetPropertyUint32(qjs_ctx, data, p);
        for ( int i = 0; i < h; i++ )
        {
            JSValue row = JS_GetPropertyUint32(qjs_ctx, plane, i);
            for ( int j = 0; j < w; j++ )
            {
                JSValue val_i = JS_GetPropertyUint32(qjs_ctx, row, j);
                int64_t i64;
                JS_ToInt64(qjs_ctx, &i64, val_i);
                ptr[j] = i64;
                JS_FreeValue(qjs_ctx, val_i);
            }
            JS_FreeValue(qjs_ctx, row);
            ptr += linesize;
        }
        JS_FreeValue(qjs_ctx, plane);
    }
#endif
    JS_FreeValue(qjs_ctx, args);

#if TIME_QUICKJS
    t1 = av_gettime_relative();
    convert2 += (t1 - t0);
    t0 = t1;
#endif

#if TIME_QUICKJS
    printf("time taken convert1 %" PRId64 " call %" PRId64 " convert2 %" PRId64 "\n", convert1, call, convert2);
#endif

    return 0;
}

/*********************************************************************/
static void quickjs_uninit(ScriptContext *sctx)
{
    QuickJSContext *qjs = &sctx->qjs;

    JS_FreeValue(qjs->ctx, qjs->filter_func);
    JS_FreeValue(qjs->ctx, qjs->global_object);
    JS_FreeValue(qjs->ctx, qjs->ctor_UintC8Array);

    /* free quickjs */
    js_std_free_handlers(qjs->rt);
    JS_FreeContext(qjs->ctx);
    JS_FreeRuntime(qjs->rt);
}

/*********************************************************************/
typedef struct {
    PyObject ob_base;
    PythonContext *pctx;
    uint8_t *ptr;
    size_t len;
} py_UintC8Array;

static size_t uint8carray_len(PyObject *self)
{
    py_UintC8Array *_self = (py_UintC8Array *) self;
    return _self->len;
}

static PyObject *uint8carray_subscript(PyObject *self, PyObject *key)
{
    py_UintC8Array *_self = (py_UintC8Array *) self;
    PythonContext *pctx = _self->pctx;
    PythonFunctions *pyfuncs = &pctx->pyfuncs;
    size_t i = pyfuncs->PyLong_AsSize_t(key);
    if ( i == (size_t) -1 )
        return NULL;
    if ( i > _self->len )
    {
        pyfuncs->PyErr_SetString(pctx->PyExc_IndexError, "index out of range");
        return NULL;
    }
    return pyfuncs->PyLong_FromLong(_self->ptr[i]);
}

static int uint8carray_ass_subscript(PyObject *self, PyObject *key, PyObject *value)
{
    py_UintC8Array *_self = (py_UintC8Array *) self;
    PythonContext *pctx = _self->pctx;
    PythonFunctions *pyfuncs = &pctx->pyfuncs;
    size_t i;
    size_t val;
    i = pyfuncs->PyLong_AsSize_t(key);
    if ( i == (size_t) -1 )
        return -1;
    if ( i > _self->len )
    {
        pyfuncs->PyErr_SetString(pctx->PyExc_IndexError, "index out of range");
        return -1;
    }
    val = pyfuncs->PyLong_AsSize_t(value);
    if ( val == (size_t) -1 )
        return -1;
    _self->ptr[i] = val;
    return 0;
}

#define Py_mp_ass_subscript 3
#define Py_mp_length 4
#define Py_mp_subscript 5
#define Py_tp_init 60
static PyType_Slot py_UintC8Array_slots[] = {
    { Py_mp_ass_subscript, uint8carray_ass_subscript },
    { Py_mp_length, uint8carray_len },
    { Py_mp_subscript, uint8carray_subscript },
    { 0, 0 },
};

static PyObject *new_UintC8Array(PythonContext *pctx, size_t len, void *ptr)
{
    PythonFunctions *pyfuncs = &pctx->pyfuncs;
    void *obj = pyfuncs->PyObject_Malloc(sizeof(py_UintC8Array));
    PyObject *py_obj = pyfuncs->PyObject_Init(obj, pctx->UintC8Array);
    py_UintC8Array *py_arr = (py_UintC8Array *) py_obj;
    py_arr->pctx = pctx;
    py_arr->len = len;
    py_arr->ptr = ptr;
    return py_obj;
}

#define Py_TPFLAGS_BASETYPE         (1UL << 10)
#define Py_TPFLAGS_HAVE_VERSION_TAG (1UL << 18)
#define Py_TPFLAGS_DEFAULT          Py_TPFLAGS_HAVE_VERSION_TAG

static PyType_Spec HeapCType_spec = {
    "ffedit.UintC8Array",
    sizeof(py_UintC8Array),
    0,
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    py_UintC8Array_slots
};

/*********************************************************************/
static int python_init(ScriptContext *sctx)
{
    PythonContext *pctx = &sctx->pctx;
    PythonFunctions *pyfuncs = &pctx->pyfuncs;
    PyObject *pysrc;
    PyObject *pyret;
    uint8_t *buf;
    size_t len;
    int ret = 0;

    pctx->libpython_so = dlopen_python();
    if ( pctx->libpython_so == NULL )
    {
        av_log(sctx, AV_LOG_FATAL, "Could not find libpython in the usual places.\n");
        av_log(sctx, AV_LOG_FATAL, "Try specifying the path to libpython using the FFGLITCH_LIBPYTHON_PATH environment variable.\n");
        ret = AVERROR(EINVAL);
        goto end;
    }

    if ( !dlsyms_python(&pctx->pyfuncs, pctx->libpython_so) )
    {
        av_log(sctx, AV_LOG_FATAL, "Could not load all necessary functions from libpython.\n");
        ret = AVERROR(EINVAL);
        goto end;
    }

    pyfuncs->Py_Initialize();
    pctx->module = pyfuncs->PyImport_AddModule("__main__");
    pctx->locals = pyfuncs->PyModule_GetDict(pctx->module);
    pctx->globals = pyfuncs->PyDict_New();
    pctx->builtins = pyfuncs->PyEval_GetBuiltins();
    pyfuncs->PyDict_SetItemString(pctx->globals, "__builtins__", pctx->builtins);
    pctx->PyExc_IndexError = pyfuncs->PyDict_GetItemString(pctx->builtins, "IndexError");
    pctx->UintC8Array = pyfuncs->PyType_FromSpec(&HeapCType_spec);

    /* load file */
    // TODO don't use js_load_file()
    buf = js_load_file(NULL, &len, sctx->py_fname);
    if ( buf == NULL )
    {
        av_log(sctx, AV_LOG_FATAL, "Could not open script file %s\n", sctx->py_fname);
        ret = AVERROR(EINVAL);
        goto end;
    }

    pysrc = pyfuncs->Py_CompileString(buf, sctx->py_fname, Py_file_input);
    if ( pysrc == NULL )
    {
        av_log(sctx, AV_LOG_FATAL, "Could not compile script file %s\n", sctx->py_fname);
        pyfuncs->PyErr_Print();
        ret = AVERROR(EINVAL);
        goto end;
    }

    pyret = pyfuncs->PyEval_EvalCode(pysrc, pctx->globals, pctx->locals);
    if ( pyret == NULL )
    {
        av_log(sctx, AV_LOG_FATAL, "Could not eval script file %s\n", sctx->py_fname);
        pyfuncs->PyErr_Print();
        ret = AVERROR(EINVAL);
        goto end;
    }
    pyfuncs->Py_DecRef(pyret);
    pyfuncs->Py_DecRef(pysrc);

    pctx->filter_func = pyfuncs->PyObject_GetAttrString(pctx->module, "filter");
    if ( pctx->filter_func == NULL
      || !pyfuncs->PyCallable_Check(pctx->filter_func) )
    {
        av_log(sctx, AV_LOG_FATAL, "Could not find function filter() in %s\n", sctx->py_fname);
        pyfuncs->PyErr_Print();
        ret = AVERROR(EINVAL);
        goto end;
    }

end:
    return ret;
}

/*********************************************************************/
static int python_filter(ScriptContext *sctx, AVFilterLink *inlink, AVFrame *out, int64_t frame_num, double pts)
{
#define TIME_PYTHON 0
#if TIME_PYTHON
    int64_t convert1 = 0;
    int64_t call = 0;
    int64_t convert2 = 0;
    int64_t t0;
    int64_t t1;
#endif
    PythonContext *pctx = &sctx->pctx;
    PythonFunctions *pyfuncs = &pctx->pyfuncs;
    PyObject *pyargs;
    PyObject *pyret;
    PyObject *data;
    int ret = 0;

#if TIME_PYTHON
    t0 = av_gettime_relative();
#endif

    /* convert to python */
    data = pyfuncs->PyList_New(sctx->planes);
    for ( int p = 0; p < sctx->planes; p++ )
    {
        const int h = (p == 1 || p == 2) ? AV_CEIL_RSHIFT(inlink->h, sctx->vsub) : inlink->h;
        const int w = (p == 1 || p == 2) ? AV_CEIL_RSHIFT(inlink->w, sctx->hsub) : inlink->w;
        const int linesize = out->linesize[p];
        uint8_t *ptr = out->data[p];
        PyObject *plane = pyfuncs->PyList_New(h);
        for ( int i = 0; i < h; i++ )
        {
#if PYTHON_ZERO_COPY
            PyObject *row = new_UintC8Array(pctx, w, ptr);
#else
            PyObject *row = pyfuncs->PyList_New(w);
            for ( int j = 0; j < w; j++ )
            {
                PyObject *pixval = pyfuncs->PyLong_FromLong(ptr[j]);
                pyfuncs->PyList_SetItem(row, j, pixval);
            }
#endif
            pyfuncs->PyList_SetItem(plane, i, row);
            ptr += linesize;
        }
        pyfuncs->PyList_SetItem(data, p, plane);
    }
    pyargs = pyfuncs->PyDict_New();
    pyfuncs->PyDict_SetItemString(pyargs, "frame_num", pyfuncs->PyLong_FromLong(frame_num));
    pyfuncs->PyDict_SetItemString(pyargs, "pts", pyfuncs->PyFloat_FromDouble(pts));
    pyfuncs->PyDict_SetItemString(pyargs, "data", data);

#if TIME_PYTHON
    t1 = av_gettime_relative();
    convert1 += (t1 - t0);
    t0 = t1;
#endif

    /* call filter_func() with data */
    pyret = pyfuncs->PyObject_CallFunction(pctx->filter_func, "O", pyargs);
    if ( pyret == NULL )
    {
        av_log(sctx, AV_LOG_FATAL, "Error calling filter() function in %s\n", sctx->py_fname);
        pyfuncs->PyErr_Print();
        ret = AVERROR(EINVAL);
        goto end;
    }
    pyfuncs->Py_DecRef(pyret);

#if TIME_PYTHON
    t1 = av_gettime_relative();
    call += (t1 - t0);
    t0 = t1;
#endif

    /* convert back from python */
#if !PYTHON_ZERO_COPY
    for ( int p = 0; p < sctx->planes; p++ )
    {
        const int h = (p == 1 || p == 2) ? AV_CEIL_RSHIFT(inlink->h, sctx->vsub) : inlink->h;
        const int w = (p == 1 || p == 2) ? AV_CEIL_RSHIFT(inlink->w, sctx->hsub) : inlink->w;
        const int linesize = out->linesize[p];
        uint8_t *ptr = out->data[p];
        PyObject *plane = pyfuncs->PyList_GetItem(data, p);
        for ( int i = 0; i < h; i++ )
        {
            PyObject *row = pyfuncs->PyList_GetItem(plane, i);
            for ( int j = 0; j < w; j++ )
            {
                PyObject *pixval = pyfuncs->PyList_GetItem(row, j);
                ptr[j] = pyfuncs->PyLong_AsLong(pixval);
            }
            ptr += linesize;
        }
    }
#endif
    pyfuncs->Py_DecRef(pyargs);

#if TIME_PYTHON
    t1 = av_gettime_relative();
    convert2 += (t1 - t0);
    t0 = t1;
#endif

#if TIME_PYTHON
    printf("time taken convert1 %" PRId64 " call %" PRId64 " convert2 %" PRId64 "\n", convert1, call, convert2);
#endif

end:
    return ret;
}

/*********************************************************************/
static void python_uninit(ScriptContext *sctx)
{
    PythonContext *pctx = &sctx->pctx;
    PythonFunctions *pyfuncs = &pctx->pyfuncs;
    pyfuncs->Py_Finalize();
    dlclose(pctx->libpython_so);
}

/*********************************************************************/
static av_cold int script_init(AVFilterContext *ctx)
{
    ScriptContext *sctx = ctx->priv;
    int ret = 0;

    if ( (sctx->js_fname == NULL && sctx->py_fname == NULL)
      || (sctx->js_fname != NULL && sctx->py_fname != NULL) )
    {
        av_log(sctx, AV_LOG_ERROR, "Either a JavaScript or a Python script must be specified but not both\n");
        ret = AVERROR(EINVAL);
        goto end;
    }

    if ( sctx->js_fname != NULL )
        ret = quickjs_init(sctx);
    if ( sctx->py_fname != NULL )
        ret = python_init(sctx);

end:
    return ret;
}

/*********************************************************************/
static int script_query_formats(AVFilterContext *ctx)
{
    static const enum AVPixelFormat yuv_pix_fmts[] = {
        AV_PIX_FMT_YUV444P,  AV_PIX_FMT_YUV422P,  AV_PIX_FMT_YUV420P,
        AV_PIX_FMT_YUV411P,  AV_PIX_FMT_YUV410P,  AV_PIX_FMT_YUV440P,
        AV_PIX_FMT_YUVA444P, AV_PIX_FMT_YUVA422P, AV_PIX_FMT_YUVA420P,
        AV_PIX_FMT_GRAY8,
        AV_PIX_FMT_NONE
    };
    AVFilterFormats *fmts_list;

    fmts_list = ff_make_format_list(yuv_pix_fmts);
    if ( !fmts_list )
        return AVERROR(ENOMEM);

    return ff_set_common_formats(ctx, fmts_list);
}

/*********************************************************************/
static int script_config_props(AVFilterLink *inlink)
{
    ScriptContext *sctx = inlink->dst->priv;
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(inlink->format);

    av_assert0(desc);

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
    ScriptContext *sctx = ctx->priv;
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

    if ( sctx->js_fname != NULL )
        ret = quickjs_filter(sctx, inlink, out, frame_num, pts);
    if ( sctx->py_fname != NULL )
        ret = python_filter(sctx, inlink, out, frame_num, pts);
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
    ScriptContext *sctx = ctx->priv;

    if ( sctx->js_fname != NULL )
        quickjs_uninit(sctx);
    if ( sctx->py_fname != NULL )
        python_uninit(sctx);
}

/*********************************************************************/
static const AVFilterPad script_inputs[] = {
    {
        .name         = "default",
        .type         = AVMEDIA_TYPE_VIDEO,
        .config_props = script_config_props,
        .filter_frame = script_filter_frame,
    },
    { NULL }
};

/*********************************************************************/
static const AVFilterPad script_outputs[] = {
    {
        .name = "default",
        .type = AVMEDIA_TYPE_VIDEO,
    },
    { NULL }
};

/*********************************************************************/
AVFilter ff_vf_script = {
    .name          = "script",
    .description   = NULL_IF_CONFIG_SMALL("Run external script on data"),
    .priv_size     = sizeof(ScriptContext),
    .init          = script_init,
    .uninit        = script_uninit,
    .query_formats = script_query_formats,
    .inputs        = script_inputs,
    .outputs       = script_outputs,
    .priv_class    = &script_class,
    .flags         = AVFILTER_FLAG_SUPPORT_TIMELINE_GENERIC,
};
