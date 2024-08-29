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

#ifndef AVUTIL_SCRIPT_QUICKJS_H
#define AVUTIL_SCRIPT_QUICKJS_H

/*********************************************************************/
#include <stdarg.h>

/*********************************************************************/
#include "libavutil/json.h"
#include "libavutil/log.h"
#include "libavutil/quickjs/quickjs-libc.h"
#include "libavutil/quickjs/quickjs-zmq.h"

/*********************************************************************/
typedef struct FFQuickJSContext {
    /* common from FFScriptContext */
    const AVClass *class;
    const char *script_fname;
    int script_is_py;
    int script_is_js;
    int flags;

    /* quickjs */
    JSRuntime *rt;
    JSContext *ctx;
    JSValue global_object;
    JSModuleDef *module;

    /* array_of_ints cache */
#define MAX_AOI_CACHE_LEN 256
    JSValue *aoi_cache[MAX_AOI_CACHE_LEN];
    int aoi_cache_len[MAX_AOI_CACHE_LEN];
    int aoi_cache_offset[MAX_AOI_CACHE_LEN];
} FFQuickJSContext;

typedef struct {
    JSValue jval;
} FFQuickJSObject;

/*********************************************************************/
FFQuickJSContext *ff_quickjs_init(const char *script_fname, int flags);
void ff_quickjs_uninit(FFQuickJSContext **pjs_ctx);

FFQuickJSObject *ff_quickjs_get_func(FFQuickJSContext *js_ctx, const char *func_name, int required);
int ff_quickjs_call_func(FFQuickJSContext *js_ctx, FFQuickJSObject **pjs_ret, FFQuickJSObject *js_func, va_list vl);

FFQuickJSObject *ff_quickjs_from_json(FFQuickJSContext *js_ctx, json_t *jso);
json_t *ff_quickjs_to_json(json_ctx_t *jctx, FFQuickJSContext *js_ctx, FFQuickJSObject *val);
void ff_quickjs_free_obj(FFQuickJSContext *js_ctx, FFQuickJSObject *js_obj);

#endif
