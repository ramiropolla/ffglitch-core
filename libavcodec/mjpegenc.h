/*
 * MJPEG encoder
 * Copyright (c) 2000, 2001 Fabrice Bellard
 * Copyright (c) 2003 Alex Beregszaszi
 * Copyright (c) 2003-2004 Michael Niedermayer
 *
 * Support for external huffman table, various fixes (AVID workaround),
 * aspecting, new decode_frame mechanism and apple mjpeg-b support
 *                                  by Alex Beregszaszi
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

/**
 * @file
 * MJPEG encoder.
 */

#ifndef AVCODEC_MJPEGENC_H
#define AVCODEC_MJPEGENC_H

#include <stdint.h>

#include "mjpeg.h"
#include "mpegvideo.h"
#include "put_bits.h"

/**
 * Enum for the Huffman encoding strategy.
 */
enum HuffmanTableOption {
    HUFFMAN_TABLE_DEFAULT = 0, ///< Use the default Huffman tables.
    HUFFMAN_TABLE_OPTIMAL = 1, ///< Compute and use optimal Huffman tables.
    NB_HUFFMAN_TABLE_OPTION = 2
};

static inline void put_marker(PutBitContext *p, enum JpegMarker code)
{
    put_bits(p, 8, 0xff);
    put_bits(p, 8, code);
}

int  ff_mjpeg_encode_init(MpegEncContext *s);
void ff_mjpeg_amv_encode_picture_header(MpegEncContext *s);
void ff_mjpeg_encode_mb(MpegEncContext *s, int16_t block[12][64]);
int  ff_mjpeg_encode_stuffing(MpegEncContext *s);

typedef struct MJpegContext MJpegContext;

void ff_mjpeg_encode_block(
        void *ctx,
        PutBitContext *pb,
        MJpegContext *m,
        uint8_t permutated_scantable[64],
        int *last_dc,
        int *block_last_index,
        int16_t *block,
        int n);

#endif /* AVCODEC_MJPEGENC_H */
