/*
 * Copyright (c) 2017-2024 Ramiro Polla
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

#ifndef AVCODEC_FFEDIT_XP_BYTES_H
#define AVCODEC_FFEDIT_XP_BYTES_H

#include "avcodec.h"
#include "bytestream.h"

typedef struct FFEditTransplicateBytesContext {
    const AVClass *av_class;

    uint8_t *data;
    PutByteContext *pb;

    PutByteContext saved; /* saved */
} FFEditTransplicateBytesContext;

int ffe_transplicate_bytes_init(
        AVCodecContext *avctx,
        FFEditTransplicateBytesContext *xp,
        size_t pkt_size);

void ffe_transplicate_bytes_flush(
        AVCodecContext *avctx,
        FFEditTransplicateBytesContext *xp,
        const AVPacket *pkt);

void ffe_transplicate_bytes_free(FFEditTransplicateBytesContext *xp);

PutByteContext *ffe_transplicate_bytes_pb(FFEditTransplicateBytesContext *xp);

PutByteContext *ffe_transplicate_bytes_save(FFEditTransplicateBytesContext *xp);

void ffe_transplicate_bytes_restore(
        FFEditTransplicateBytesContext *xp,
        PutByteContext *saved);

#endif /* AVCODEC_FFEDIT_XP_BYTES_H */
