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

#ifndef AVUTIL_SCRIPT_H
#define AVUTIL_SCRIPT_H

/*********************************************************************/
#include "libavutil/json.h"
#include "libavutil/log.h"
#include "libavutil/script_python.h"
#include "libavutil/script_quickjs.h"

/*********************************************************************/
char *ff_script_read_file(const char *script_fname, size_t *psize);
void *ff_grow_array(void *array, int elem_size, int *size, int new_size);
#ifndef GROW_ARRAY
#  define GROW_ARRAY(array, nb_elems) array = ff_grow_array(array, sizeof(*array), &nb_elems, nb_elems + 1)
#endif

#define FFSCRIPT_FLAGS_AOI_CACHE (1 << 0)

/*********************************************************************/
typedef struct FFScriptContext {
    /* common from FFScriptContext */
    const AVClass *class;
    const char *script_fname;
    int script_is_py;
    int script_is_js;
    int flags;

    /* python or quickjs */
    uint8_t rest_of_struct[];
} FFScriptContext;

typedef struct FFScriptObject FFScriptObject;

FFScriptContext *ff_script_init(const char *script_fname, int flags);
void ff_script_uninit(FFScriptContext **pctx);

FFScriptObject *ff_script_get_func(FFScriptContext *ctx, const char *func_name, int required);
int ff_script_call_func(FFScriptContext *ctx, FFScriptObject **pret, FFScriptObject *func, ...);

FFScriptObject *ff_script_from_json(FFScriptContext *ctx, json_t *jso);
json_t *ff_script_to_json(json_ctx_t *jctx, FFScriptContext *ctx, FFScriptObject *val);
void ff_script_free_obj(FFScriptContext *ctx, FFScriptObject *obj);

#endif
