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

#ifndef AVCODEC_FFEDIT_MB_H
#define AVCODEC_FFEDIT_MB_H

#include "libavutil/json.h"

#include "avcodec.h"
#include "ffedit_xp_bits.h"
#include "put_bits.h"
#include "get_bits.h"

typedef struct
{
    json_t *jsizes;
    json_t *jdatas;
} ffe_mb_ctx;

typedef struct
{
    GetBitContext saved;
    PutBitContext pb;
    uint8_t *data;
} ffe_mb_mb_ctx;

void ffe_mb_export_init(
        json_ctx_t *jctx,
        AVFrame *f,
        int mb_height,
        int mb_width);
void ffe_mb_export_cleanup(json_ctx_t *jctx, AVFrame *f);
void ffe_mb_import_init(json_ctx_t *jctx, AVFrame *f);

void ffe_mb_export_init_mb(
        ffe_mb_mb_ctx *mbctx,
        GetBitContext *gb);
void ffe_mb_export_flush_mb(
        ffe_mb_mb_ctx *mbctx,
        json_ctx_t *jctx,
        AVFrame *f,
        GetBitContext *gb,
        int mb_y,
        int mb_x);
void ffe_mb_import_init_mb(
        ffe_mb_mb_ctx *mbctx,
        AVFrame *f,
        GetBitContext *gb,
        FFEditTransplicateBitsContext *xp,
        int mb_y,
        int mb_x);
void ffe_mb_import_flush_mb(
        ffe_mb_mb_ctx *mbctx,
        AVFrame *f,
        GetBitContext *gb,
        FFEditTransplicateBitsContext *xp,
        int mb_y,
        int mb_x);

#endif /* AVCODEC_FFEDIT_MB_H */
