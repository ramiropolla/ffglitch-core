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
#include <stdarg.h>

/*********************************************************************/
#include "libavutil/avutil.h"
#include "libavutil/avassert.h"
#include "libavutil/avstring.h"
#include "libavutil/file_open.h"
#include "libavutil/mem.h"
#include "libavutil/script.h"

/*********************************************************************/
static const AVClass script_class = {
    .class_name = "script",
    .item_name  = av_default_item_name,
    .version    = LIBAVUTIL_VERSION_INT,
};

static struct {
    const AVClass *class;
} script_log_ctx = { &script_class };

/*********************************************************************/
char *ff_script_read_file(const char *script_fname, size_t *psize)
{
    size_t size;
    char *buf;
    FILE *fp;

    fp = avpriv_fopen_utf8(script_fname, "rb");
    if ( fp == NULL )
        return NULL;

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    buf = av_malloc(size+1);
    buf[size] = '\0';

    fread(buf, size, 1, fp);
    fclose(fp);

    *psize = size;

    return buf;
}

/*********************************************************************/
/* TODO copied from fftools/cmdutils.c */
void *ff_grow_array(void *array, int elem_size, int *size, int new_size)
{
    if (new_size >= INT_MAX / elem_size) {
        av_log(NULL, AV_LOG_ERROR, "Array too big.\n");
        av_assert0(0);
    }
    if (*size < new_size) {
        uint8_t *tmp = av_realloc_array(array, new_size, elem_size);
        if (!tmp) {
            av_log(NULL, AV_LOG_ERROR, "Could not alloc buffer.\n");
            av_assert0(0);
        }
        memset(tmp + *size*elem_size, 0, (new_size-*size) * elem_size);
        *size = new_size;
        return tmp;
    }
    return array;
}

/*********************************************************************/
/* TODO copied from libavformat/format.c */
static int match_ext(const char *filename, const char *extensions)
{
    const char *ext;

    if (!filename)
        return 0;

    ext = strrchr(filename, '.');
    if (ext)
        return av_match_name(ext + 1, extensions);
    return 0;
}

/*********************************************************************/
FFScriptContext *ff_script_init(const char *script_fname, int flags)
{
    int script_is_py = match_ext(script_fname, "py");
    int script_is_js = match_ext(script_fname, "js");

    if ( !script_is_js && !script_is_py )
    {
        av_log(&script_log_ctx, AV_LOG_FATAL, "Only JavaScript (\".js\") or Python3 (\".py\") scripts are supported!\n");
        return NULL;
    }

    if ( script_is_py )
        return (FFScriptContext *) ff_python_init(script_fname, flags);
    if ( script_is_js )
        return (FFScriptContext *) ff_quickjs_init(script_fname, flags);
    /* never reached */
    av_assert0(0);
    return NULL;
}

/*********************************************************************/
void ff_script_uninit(FFScriptContext **pctx)
{
    FFScriptContext *ctx;
    int script_is_py;
    int script_is_js;
    if ( pctx == NULL )
        return;
    ctx = *pctx;
    if ( ctx == NULL )
        return;
    script_is_py = ctx->script_is_py;
    script_is_js = ctx->script_is_js;
    if ( script_is_py )
        ff_python_uninit((FFPythonContext **) pctx);
    if ( script_is_js )
        ff_quickjs_uninit((FFQuickJSContext **) pctx);
}

/*********************************************************************/
FFScriptObject *ff_script_get_func(FFScriptContext *ctx, const char *func_name, int required)
{
    if ( ctx->script_is_py )
        return (FFScriptObject *) ff_python_get_func((FFPythonContext *) ctx, func_name, required);
    if ( ctx->script_is_js )
        return (FFScriptObject *) ff_quickjs_get_func((FFQuickJSContext *) ctx, func_name, required);
    /* never reached */
    av_assert0(0);
    return NULL;
}

/*********************************************************************/
static int ff_script_call_func_va(FFScriptContext *ctx, FFScriptObject **pret, FFScriptObject *func, va_list vl)
{
    if ( ctx->script_is_py )
        return ff_python_call_func((FFPythonContext *) ctx, (FFPythonObject **) pret, (FFPythonObject *) func, vl);
    if ( ctx->script_is_js )
        return ff_quickjs_call_func((FFQuickJSContext *) ctx, (FFQuickJSObject **) pret, (FFQuickJSObject *) func, vl);
    /* never reached */
    av_assert0(0);
    return -1;
}

int ff_script_call_func(FFScriptContext *ctx, FFScriptObject **pret, FFScriptObject *func, ...)
{
    int ret;
    va_list vl;
    va_start(vl, func);
    ret = ff_script_call_func_va(ctx, pret, func, vl);
    va_end(vl);
    return ret;
}

/*********************************************************************/
FFScriptObject *ff_script_from_json(FFScriptContext *ctx, json_t *jso)
{
    if ( ctx->script_is_py )
        return (FFScriptObject *) ff_python_from_json((FFPythonContext *) ctx, jso);
    if ( ctx->script_is_js )
        return (FFScriptObject *) ff_quickjs_from_json((FFQuickJSContext *) ctx, jso);
    /* never reached */
    av_assert0(0);
    return NULL;
}

/*********************************************************************/
json_t *ff_script_to_json(json_ctx_t *jctx, FFScriptContext *ctx, FFScriptObject *val)
{
    if ( ctx->script_is_py )
        return ff_python_to_json(jctx, (FFPythonContext *) ctx, (FFPythonObject *) val);
    if ( ctx->script_is_js )
        return ff_quickjs_to_json(jctx, (FFQuickJSContext *) ctx, (FFQuickJSObject *) val);
    /* never reached */
    av_assert0(0);
    return NULL;
}

/*********************************************************************/
void ff_script_free_obj(FFScriptContext *ctx, FFScriptObject *obj)
{
    if ( obj == NULL )
        return;
    if ( ctx->script_is_py )
        ff_python_free_obj((FFPythonContext *) ctx, (FFPythonObject *) obj);
    if ( ctx->script_is_js )
        ff_quickjs_free_obj((FFQuickJSContext *) ctx, (FFQuickJSObject *) obj);
}
