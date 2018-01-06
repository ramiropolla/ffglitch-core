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

#ifndef AVFORMAT_FFEDIT_H
#define AVFORMAT_FFEDIT_H

#include "avformat.h"

/* forward declarations */
struct FFEditOutputContext;
typedef struct FFEditOutputContext FFEditOutputContext;

/*----------------------------- PACKETS -----------------------------*/
typedef struct FFEditPacket {
    int64_t i_pos;
    size_t i_size;

    uint8_t *data;
    size_t o_size;

    int alignment;
} FFEditPacket;

int ffe_output_packet(
        FFEditOutputContext *ectx,
        int64_t i_pos,
        size_t i_size,
        uint8_t *data,
        size_t o_size);

int ffe_output_padding(
        FFEditOutputContext *ectx,
        int64_t i_pos,
        size_t i_size,
        uint8_t val,
        int alignment);

/*----------------------------- FIXUPS ------------------------------*/
enum FFEditFixupType {
    /*
     * fixup to an (absolute) offset in the file
     *
     * a1: points_to_here
     * a2: (unused)
     *
     *    .pos  .points_to_here
     * .------------------------.
     * |  |     |               |
     * '--|-----|---------------'
     *    '-->--'
     *    .points_to_here   .pos
     * .------------------------.
     * |  |                 |   |
     * '--|-----------------|---'
     *    '--------<--------'
     */
    FFEDIT_FIXUP_OFFSET_LE32,
    FFEDIT_FIXUP_OFFSET_BE32,
    /*
     * fixup to a field with size information
     * - range_start points to the first byte of the range
     * - range_end points to the last byte of the range
     *
     * a1: range_start
     * a2: range_end
     *
     *    .pos .range_start
     * .-----------------------.
     * |  |    |<---->|        |
     * '-----------------------'
     *                .range_end
     */
    FFEDIT_FIXUP_SIZE_LE32,
    FFEDIT_FIXUP_SIZE_BE32,
};

typedef struct FFEditFixup {
    int64_t pos;
    int64_t val;

    int64_t a1;
    int64_t a2;

    enum FFEditFixupType type;
} FFEditFixup;

int ffe_output_fixup(
        FFEditOutputContext *ectx,
        enum FFEditFixupType type,
        int64_t pos,
        int64_t val,
        int64_t arg1,
        int64_t arg2);

/*----------------------------- OUTPUT ------------------------------*/
typedef struct FFEditOutputContext {
    const AVClass *av_class;

    AVIOContext *o_pb;

    FFEditPacket *packets;
    size_t     nb_packets;

    FFEditFixup *fixups;
    size_t    nb_fixups;

    size_t last_file_size;
    size_t file_size_delta;

    int directwrite;
} FFEditOutputContext;

int ffe_output_open(FFEditOutputContext **pectx, const char *filename);

void ffe_set_directwrite(FFEditOutputContext *ectx);

int ffe_output_merge(FFEditOutputContext *dst, FFEditOutputContext *src);

int ffe_output_flush(FFEditOutputContext *ectx, AVFormatContext *fctx);

void ffe_output_freep(FFEditOutputContext **pectx);

#endif /* AVFORMAT_FFEDIT_H */
