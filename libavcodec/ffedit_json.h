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

#ifndef AVCODEC_FFEDIT_JSON_H
#define AVCODEC_FFEDIT_JSON_H

#include "libavutil/json.h"

/* 2d array of blocks */
json_t *
ffe_jblock_new(
        json_ctx_t *jctx,
        int width,
        int height,
        int pflags);
void
ffe_jblock_set(
        json_t *jso,
        int mb_y,
        int mb_x,
        json_t *jval);

/* block arrays */
json_t *
ffe_jmb_new(
        json_ctx_t *jctx,
        int mb_width,
        int mb_height,
        int nb_components,
        int *v_count,
        int *h_count,
        int *quant_index,
        int pflags);
void
ffe_jmb_set_context(
        json_t *jso,
        int nb_components,
        int *v_count,
        int *h_count);
json_t *
ffe_jmb_get(
        json_t *jso,
        int component,
        int mb_y,
        int mb_x,
        int block);
int32_t
ffe_jmb_array_of_ints_get(
        json_t *s,
        int component,
        int mb_y,
        int mb_x,
        int block);
int32_t
ffe_jmb_int_get(
        json_t *jso,
        int component,
        int mb_y,
        int mb_x,
        int block);
void
ffe_jmb_set(
        json_t *jso,
        int component,
        int mb_y,
        int mb_x,
        int block,
        json_t *jval);

#endif /* AVUTIL_FFEDIT_JSON_H */
