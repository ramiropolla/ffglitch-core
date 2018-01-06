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

#ifndef AVCODEC_FFEDIT_XP_H
#define AVCODEC_FFEDIT_XP_H

#include "avcodec.h"
#include "put_bits.h"
#include "get_bits.h"

typedef struct FFEditTransplicateContext {
    const AVClass *av_class;

    uint8_t *data;
    PutBitContext *pb;

    PutBitContext saved; /* saved */
} FFEditTransplicateContext;

int ffe_transplicate_init(
        AVCodecContext *avctx,
        FFEditTransplicateContext *xp,
        size_t pkt_size);

void ffe_transplicate_merge(
        AVCodecContext *avctx,
        FFEditTransplicateContext *dst_xp,
        FFEditTransplicateContext *src_xp);

void ffe_transplicate_flush(
        AVCodecContext *avctx,
        FFEditTransplicateContext *xp,
        const AVPacket *pkt);

void ffe_transplicate_free(FFEditTransplicateContext *xp);

PutBitContext *ffe_transplicate_pb(FFEditTransplicateContext *xp);

PutBitContext *ffe_transplicate_save(FFEditTransplicateContext *xp);

void ffe_transplicate_restore(
        FFEditTransplicateContext *xp,
        PutBitContext *saved);

#endif /* AVCODEC_FFEDIT_XP_H */
