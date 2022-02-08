/*
 * Copyright (C) 2020-2022 Ramiro Polla
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

/*********************************************************************/
#include "config.h"
#include "libavutil/error.h"
#include "libavutil/mem.h"
#include "libavutil/script.h"

/*********************************************************************/
static const AVClass quickjs_class = {
    .class_name = "quickjs",
    .item_name  = av_default_item_name,
    .version    = LIBAVUTIL_VERSION_INT,
};

/*********************************************************************/
static JSValue js_aoi_cache_get(FFQuickJSContext *js_ctx, int32_t **pint32, uint32_t len)
{
    JSContext *ctx = js_ctx->ctx;
    JSValue val;
    if ( js_ctx->aoi_cache_offset[len] == js_ctx->aoi_cache_len[len] )
    {
        GROW_ARRAY(js_ctx->aoi_cache[len], js_ctx->aoi_cache_len[len]);
        val = JS_NewInt32FFArray(ctx, pint32, len, 0);
        js_ctx->aoi_cache[len][js_ctx->aoi_cache_offset[len]++] = val;
    }
    else
    {
        val = js_ctx->aoi_cache[len][js_ctx->aoi_cache_offset[len]++];
        JS_GetInt32FFArray(val, pint32, &len);
    }
    return JS_DupValue(ctx, val);
}

/*********************************************************************/
static JSValue quickjs_from_json(FFQuickJSContext *js_ctx, json_t *jso)
{
    JSContext *ctx = js_ctx->ctx;
    JSValue val = JS_NULL;
    JSValue *parray;
    int32_t *pint32;
    size_t len;
    if ( jso == NULL )
        return JS_NULL;
    switch ( JSON_TYPE(jso->flags) )
    {
    case JSON_TYPE_OBJECT:
        len = json_object_length(jso);
        val = JS_NewObject(ctx);
        for ( size_t i = 0; i < len; i++ )
            JS_SetPropertyStr(ctx, val, jso->obj->names[i], quickjs_from_json(js_ctx, jso->obj->values[i]));
        break;
    case JSON_TYPE_ARRAY:
        len = json_array_length(jso);
        val = JS_NewFastArray(ctx, &parray, len, 1);
        for ( size_t i = 0; i < len; i++ )
            parray[i] = quickjs_from_json(js_ctx, jso->array[i]);
        break;
    case JSON_TYPE_ARRAY_OF_INTS:
        len = json_array_length(jso);
        if ( len < MAX_AOI_CACHE_LEN && (js_ctx->flags & FFSCRIPT_FLAGS_AOI_CACHE) != 0 )
            val = js_aoi_cache_get(js_ctx, &pint32, len);
        else
            val = JS_NewInt32FFArray(ctx, &pint32, len, 0);
        memcpy(pint32, jso->array_of_ints, len * sizeof(int32_t));
        break;
    case JSON_TYPE_MV_2DARRAY:
        {
            json_mv2darray_t *mv2d = jso->mv2darray;
            const size_t width = mv2d->width;
            const size_t height = mv2d->height;
            if ( mv2d->max_nb_blocks == 1 )
            {
                int32_t *ptr32;
                val = JS_NewMV2DArray(ctx, &ptr32, width, height, 0);
                memcpy(ptr32, mv2d->mvs[0], 2 * width * height * sizeof(int32_t));
            }
            else
            {
                const uint8_t *nb_blocks_array = mv2d->nb_blocks_array;
                val = JS_NewFastArray(ctx, &parray, height, 1);
                for ( size_t i = 0; i < height; i++ )
                {
                    JSValue *prow;
                    parray[i] = JS_NewFastArray(ctx, &prow, width, 1);
                    for ( size_t j = 0; j < width; j++ )
                    {
                        size_t idx = i * width + j;
                        uint8_t nb_blocks = nb_blocks_array[idx];
                        if ( nb_blocks == 0 )
                        {
                            prow[j] = JS_NULL;
                        }
                        else if ( nb_blocks == 1 )
                        {
                            int32_t *mv = &mv2d->mvs[0][idx << 1];
                            prow[j] = js_aoi_cache_get(js_ctx, &pint32, 2);
                            pint32[0] = mv[0];
                            pint32[1] = mv[1];
                        }
                        else
                        {
                            JSValue *pmvs;
                            prow[j] = JS_NewFastArray(ctx, &pmvs, nb_blocks, 1);
                            for ( size_t k = 0; k < nb_blocks; k++ )
                            {
                                int32_t *mv = &mv2d->mvs[k][idx << 1];
                                pmvs[k] = js_aoi_cache_get(js_ctx, &pint32, 2);
                                pint32[0] = mv[0];
                                pint32[1] = mv[1];
                            }
                        }
                    }
                }
            }
        }
        break;
    case JSON_TYPE_STRING:
        val = JS_NewString(ctx, jso->str);
        break;
    case JSON_TYPE_NUMBER:
        if ( jso->val == JSON_NULL )
            val = JS_NULL;
        else
            val = JS_NewInt64(ctx, jso->val);
        break;
    case JSON_TYPE_BOOL:
        val = JS_NewBool(ctx, jso->val);
        break;
    }
    return val;
}

/*********************************************************************/
static json_t *quickjs_to_json(json_ctx_t *jctx, JSContext *ctx, JSValue val)
{
    uint32_t length;
    uint32_t width;
    uint32_t height;
    int32_t *pint32;
    JSValue *pvalues;
    if ( JS_IsInt32(val) )
        return json_int_new(jctx, JS_VALUE_GET_INT(val));
    if ( JS_IsFloat64(val) )
        return json_int_new(jctx, JS_VALUE_GET_FLOAT64(val));
    if ( JS_IsString(val) )
    {
        const char *str = JS_ToCString(ctx, val);
        json_t *ret = json_string_new(jctx, str);
        JS_FreeCString(ctx, str);
        return ret;
    }
    if ( JS_IsBool(val) )
        return json_bool_new(jctx, JS_ToBool(ctx, val));
    if ( JS_GetInt32FFArray(val, &pint32, &length) )
    {
        json_t *array = json_array_of_ints_new(jctx, length);
        memcpy(array->array_of_ints, pint32, length * sizeof(int32_t));
        return array;
    }
    if ( JS_GetMV2DArray(val, &pvalues, &width, &height) )
    {
        json_t *ret = json_mv2darray_new(jctx, width, height, 1, 0);
        json_mv2darray_t *mv2d = ret->mv2darray;
        int32_t *dst_ptr = mv2d->mvs[0];
        for ( size_t i = 0; i < height; i++ )
        {
            JS_GetMVPtr(pvalues[i], &pint32, &length); /* ignore length, it's width */
            memcpy(&dst_ptr[(i * width) << 1], pint32, 2 * width * sizeof(int32_t));
        }
        return ret;
    }
    if ( JS_IsArray(ctx, val) )
    {
        json_t *array;

        JSValue *parray = NULL;

        JS_GetFastArray(val, &parray, &length);

        if ( parray != NULL )
        {
            int is_array_of_ints = 0;
            for ( size_t i = 0; i < length; i++ )
            {
                is_array_of_ints = JS_IsNumber(parray[i]);
                if ( !is_array_of_ints )
                    break;
            }
            if ( is_array_of_ints )
            {
                array = json_array_of_ints_new(jctx, length);
                for ( size_t i = 0; i < length; i++ )
                    array->array_of_ints[i] = JS_VALUE_GET_INT(parray[i]);
            }
            else
            {
                array = json_array_new_uninit(jctx, length);
                for ( size_t i = 0; i < length; i++ )
                    array->array[i] = quickjs_to_json(jctx, ctx, parray[i]);
            }
        }
        else
        {
            int is_array_of_ints = 0;

            JSValue length_val = JS_GetPropertyStr(ctx, val, "length");
            JS_ToUint32(ctx, &length, length_val);
            JS_FreeValue(ctx, length_val);

            for ( size_t i = 0; i < length; i++ )
            {
                JSValue val_i = JS_GetPropertyUint32(ctx, val, i);
                is_array_of_ints = JS_IsNumber(val_i);
                JS_FreeValue(ctx, val_i);
                if ( !is_array_of_ints )
                    break;
            }

            if ( is_array_of_ints )
            {
                array = json_array_of_ints_new(jctx, length);
                for ( size_t i = 0; i < length; i++ )
                {
                    JSValue val_i = JS_GetPropertyUint32(ctx, val, i);
                    array->array_of_ints[i] = JS_VALUE_GET_INT(val_i);
                    JS_FreeValue(ctx, val_i);
                }
            }
            else
            {
                array = json_array_new_uninit(jctx, length);
                for ( size_t i = 0; i < length; i++ )
                {
                    JSValue val_i = JS_GetPropertyUint32(ctx, val, i);
                    array->array[i] = quickjs_to_json(jctx, ctx, val_i);
                    JS_FreeValue(ctx, val_i);
                }
            }
        }

        return array;
    }
    if ( JS_IsObject(val) )
    {
        json_t *object = json_object_new(jctx);
        JSPropertyEnum *tab;
        JS_GetOwnPropertyNames(ctx, &tab, &length, val, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY);
        for ( size_t i = 0; i < length; i++ )
        {
            const char *str = JS_AtomToCString(ctx, tab[i].atom);
            JSValue val_i = JS_GetProperty(ctx, val, tab[i].atom);
            json_object_add(object, str, quickjs_to_json(jctx, ctx, val_i));
            JS_FreeCString(ctx, str);
            JS_FreeValue(ctx, val_i);
            JS_FreeAtom(ctx, tab[i].atom);
        }
        js_free(ctx, tab);
        json_object_done(jctx, object);
        return object;
    }
    return NULL;
}

/*********************************************************************/
static void js_dump_obj(FFQuickJSContext *js_ctx, JSValueConst val)
{
    JSContext *ctx = js_ctx->ctx;
    const char *str = JS_ToCString(ctx, val);
    if (str) {
        char *line = (char *) str;
        for ( char *ptr = line; *ptr != '\0'; ptr++ )
        {
            if ( *ptr == '\n' )
            {
                *ptr = '\0';
                av_log(js_ctx, AV_LOG_ERROR, "%s\n", line);
                line = ptr + 1;
            }
        }
        av_log(js_ctx, AV_LOG_ERROR, "%s\n", line);
        JS_FreeCString(ctx, str);
    } else {
        av_log(js_ctx, AV_LOG_ERROR, "[exception]\n");
    }
}

static void js_av_log_error(FFQuickJSContext *js_ctx)
{
    JSContext *ctx = js_ctx->ctx;
    JSValue exception_val = JS_GetException(ctx);
    JS_BOOL is_error = JS_IsError(ctx, exception_val);
    JSValue val;
    js_dump_obj(js_ctx, exception_val);
    if (is_error) {
        val = JS_GetPropertyStr(ctx, exception_val, "stack");
        if (!JS_IsUndefined(val)) {
            js_dump_obj(js_ctx, val);
        }
        JS_FreeValue(ctx, val);
    }
    JS_FreeValue(ctx, exception_val);
}

/*********************************************************************/
static size_t js_fwrite(const void *ptr, size_t size, size_t nmemb, void *stream)
{
    (void) nmemb;
    av_log(stream, AV_LOG_INFO, "%.*s", (int) size, (const char *) ptr);
    return 0;
}

/*********************************************************************/
FFQuickJSContext *ff_quickjs_init(const char *script_fname, int flags)
{
    FFQuickJSContext *js_ctx = av_mallocz(sizeof(FFQuickJSContext));
    JSRuntime *qjs_rt;
    JSContext *qjs_ctx;
    JSValue jval;
    uint8_t *buf = NULL;
    size_t len;
    int ret = AVERROR(EINVAL);

    /* init context */
    js_ctx->class = &quickjs_class;
    js_ctx->script_fname = script_fname;
    js_ctx->script_is_js = 1;
    js_ctx->flags = flags;

    /* TODO check for errors */
    qjs_rt = JS_NewRuntime();
    js_ctx->rt = qjs_rt;
    js_std_init_handlers(qjs_rt);
    qjs_ctx = JS_NewContext(qjs_rt);
    js_ctx->ctx = qjs_ctx;
    JS_SetContextOpaque(qjs_ctx, js_ctx);
    JS_SetWriteFunc(qjs_ctx, js_fwrite, js_ctx);

    /* loader for ES6 modules */
    JS_SetModuleLoaderFunc(qjs_rt, NULL, js_module_loader, NULL);
    js_std_add_helpers(qjs_ctx, 0, NULL);

    js_ctx->global_object = JS_GetGlobalObject(qjs_ctx);

    /* system modules */
    js_init_module_std(qjs_ctx, "std");
    js_init_module_os(qjs_ctx, "os");

    /* load file */
    buf = ff_script_read_file(script_fname, &len);
    if ( buf == NULL )
    {
        av_log(js_ctx, AV_LOG_FATAL, "Could not open script file %s\n", script_fname);
        goto end;
    }

    jval = JS_EvalModule(qjs_ctx, (const char *) buf, len, script_fname, &js_ctx->module);
    if ( JS_IsException(jval) )
    {
        js_av_log_error(js_ctx);
        goto end;
    }
    JS_FreeValue(qjs_ctx, jval);

    ret = 0;

end:
    av_free(buf);
    if ( ret != 0 )
        av_freep(&js_ctx);

    return js_ctx;
}

/*********************************************************************/
void ff_quickjs_uninit(FFQuickJSContext **pjs_ctx)
{
    FFQuickJSContext *js_ctx = *pjs_ctx;
    JSContext *ctx = js_ctx->ctx;

    /* free array_of_ints cache */
    for ( size_t i = 0; i < MAX_AOI_CACHE_LEN; i++ )
    {
        for ( size_t j = 0; j < js_ctx->aoi_cache_len[i]; j++ )
            JS_FreeValue(ctx, js_ctx->aoi_cache[i][j]);
        av_freep(&js_ctx->aoi_cache[i]);
    }

    /* free quickjs */
    JS_FreeValue(ctx, js_ctx->global_object);
    js_std_free_handlers(js_ctx->rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(js_ctx->rt);
    av_freep(pjs_ctx);
}

/*********************************************************************/
static FFQuickJSObject *new_FFQuickJSObject(JSValue jval)
{
    FFQuickJSObject *js_obj = av_malloc(sizeof(FFQuickJSObject));
    js_obj->jval = jval;
    return js_obj;
}

/*********************************************************************/
FFQuickJSObject *ff_quickjs_get_func(FFQuickJSContext *js_ctx, const char *func_name, int required)
{
    JSValue jval = JS_GetModuleExport(js_ctx->ctx, js_ctx->module, func_name);
    if ( JS_IsUndefined(jval) )
    {
        if ( required )
        {
            av_log(js_ctx, AV_LOG_FATAL, "Could not find function %s() in module %s\n", func_name, js_ctx->script_fname);
            av_log(js_ctx, AV_LOG_FATAL, "Starting with FFglitch 0.10, functions must be exported from the script.\n");
            av_log(js_ctx, AV_LOG_FATAL, "Did you add the 'export' keyword before the function?\n");
            av_log(js_ctx, AV_LOG_FATAL, "For example:\n");
            av_log(js_ctx, AV_LOG_FATAL, "export function %s(...)\n", func_name);
        }
        return NULL;
    }
    return new_FFQuickJSObject(jval);
}

/*********************************************************************/
int ff_quickjs_call_func(FFQuickJSContext *js_ctx, FFQuickJSObject **pjs_ret, FFQuickJSObject *js_func, va_list vl)
{
    JSContext *ctx = js_ctx->ctx;
    JSValue jret;
    JSValue *argv = NULL;
    int argc = 0;
    int ret = 0;
    while ( 42 )
    {
        FFQuickJSObject *arg = va_arg(vl, FFQuickJSObject *);
        if ( arg == NULL )
            break;
        GROW_ARRAY(argv, argc);
        argv[argc-1] = arg->jval;
    }
    jret = JS_Call(ctx, js_func->jval, JS_UNDEFINED, argc, argv);
    if ( JS_IsException(jret) )
    {
        js_av_log_error(js_ctx);
        ret = -1;
    }
    if ( pjs_ret != NULL )
        *pjs_ret = new_FFQuickJSObject(jret);
    else
        JS_FreeValue(ctx, jret);
    av_free(argv);
    return ret;
}

/*********************************************************************/
FFQuickJSObject *ff_quickjs_from_json(FFQuickJSContext *js_ctx, json_t *jso)
{
    JSValue jval;
    if ( (js_ctx->flags & FFSCRIPT_FLAGS_AOI_CACHE) != 0 )
        memset(js_ctx->aoi_cache_offset, 0x00, sizeof(js_ctx->aoi_cache_offset));
    jval = quickjs_from_json(js_ctx, jso);
    return new_FFQuickJSObject(jval);
}

/*********************************************************************/
json_t *ff_quickjs_to_json(json_ctx_t *jctx, FFQuickJSContext *js_ctx, FFQuickJSObject *val)
{
    JSContext *ctx = js_ctx->ctx;
    return quickjs_to_json(jctx, ctx, val->jval);
}

/*********************************************************************/
void ff_quickjs_free_obj(FFQuickJSContext *js_ctx, FFQuickJSObject *js_obj)
{
    JSContext *ctx = js_ctx->ctx;
    JS_FreeValue(ctx, js_obj->jval);
    av_free(js_obj);
}
