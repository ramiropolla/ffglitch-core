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

#include "ffedit.h"

#define DEBUG_FLUSH 0

/*-------------------------------------------------------------------*/
static const AVClass ffedit_output_class = {
    .class_name = "FFEditOutput",
    .item_name  = av_default_item_name,
    .version    = LIBAVUTIL_VERSION_INT,
};

int ffe_output_open(FFEditOutputContext **pectx, const char *filename)
{
    FFEditOutputContext *ectx;

    ectx = av_mallocz(sizeof(FFEditOutputContext));
    if ( ectx == NULL )
        return AVERROR(ENOMEM);

    ectx->av_class = &ffedit_output_class;

    if ( filename )
    {
        int ret = avio_open2(&ectx->o_pb, filename, AVIO_FLAG_WRITE, NULL, NULL);
        if ( ret < 0 )
        {
            char errbuf[100];
            av_strerror(ret, errbuf, sizeof(errbuf));
            av_log(ectx, AV_LOG_ERROR, "Unable to open %s: %s\n", filename, errbuf);
            av_freep(&ectx);
            return ret;
        }
    }

    *pectx = ectx;

    return 0;
}

void ffe_set_directwrite(FFEditOutputContext *ectx)
{
    if ( ectx == NULL )
        return;
    ectx->directwrite = 1;
}

static int output_packet_internal(
        FFEditOutputContext *ectx,
        int64_t i_pos,
        size_t i_size,
        uint8_t *data,
        size_t o_size,
        int alignment)
{
    size_t nb_packets = ectx->nb_packets;
    size_t idx = nb_packets++;
    FFEditPacket *packet;
    int ret;

    ret = av_reallocp_array(&ectx->packets, nb_packets, sizeof(FFEditPacket));
    if ( ret < 0 )
        return ret;

    ectx->nb_packets = nb_packets;

    packet = &ectx->packets[idx];
    packet->i_pos = i_pos;
    packet->i_size = i_size;
    packet->o_size = o_size;
    packet->alignment = alignment;
    if ( alignment != 0 )
    {
        packet->data = av_malloc(alignment);
        memset(packet->data, (uint8_t) o_size, alignment);
    }
    else
    {
        packet->data = av_malloc(o_size);
        memcpy(packet->data, data, o_size);
    }

    return 0;
}

int ffe_output_packet(
        FFEditOutputContext *ectx,
        int64_t i_pos,
        size_t i_size,
        uint8_t *data,
        size_t o_size)
{
    if ( ectx == NULL )
        return 0;
    /* if directwrite is set, write data directly to output */
    if ( ectx->directwrite )
    {
        avio_write(ectx->o_pb, data, o_size);
        return 0;
    }
    ectx->last_file_size = i_pos + i_size;
    ectx->file_size_delta += o_size - i_size;
    return output_packet_internal(ectx, i_pos, i_size, data, o_size, 0);
}

int ffe_output_padding(
        FFEditOutputContext *ectx,
        int64_t i_pos,
        size_t i_size,
        uint8_t val,
        int alignment)
{
    if ( ectx == NULL )
        return 0;
    return output_packet_internal(ectx, i_pos, i_size, NULL, val, alignment);
}

int ffe_output_fixup(
        FFEditOutputContext *ectx,
        enum FFEditFixupType type,
        int64_t pos,
        int64_t val,
        int64_t arg1,
        int64_t arg2)
{
    FFEditFixup *fixup;
    size_t nb_fixups;
    size_t idx;
    int ret;

    if ( ectx == NULL )
        return 0;

    nb_fixups = ectx->nb_fixups;
    idx = nb_fixups++;

    ret = av_reallocp_array(&ectx->fixups, nb_fixups, sizeof(FFEditFixup));
    if ( ret < 0 )
        return ret;

    ectx->nb_fixups = nb_fixups;

    fixup = &ectx->fixups[idx];
    fixup->pos = pos;
    fixup->a1 = arg1;
    switch ( type )
    {
    case FFEDIT_FIXUP_OFFSET_LE32:
    case FFEDIT_FIXUP_OFFSET_BE32:
        fixup->a2 = 0;
        break;
    case FFEDIT_FIXUP_SIZE_LE32:
    case FFEDIT_FIXUP_SIZE_BE32:
        fixup->a2 = arg1 + arg2 - 1;
        break;
    }
    fixup->val = val;
    fixup->type = type;

    return 0;
}

int ffe_output_merge(FFEditOutputContext *dst, FFEditOutputContext *src)
{
    size_t nb_packets = dst->nb_packets + src->nb_packets;
    size_t nb_fixups = dst->nb_fixups + src->nb_fixups;
    int ret;

    ret = av_reallocp_array(&dst->packets, nb_packets, sizeof(FFEditPacket));
    if ( ret < 0 )
        return ret;

    memcpy(&dst->packets[dst->nb_packets], src->packets, src->nb_packets * sizeof(FFEditPacket));
    dst->nb_packets = nb_packets;
    src->nb_packets = 0;

    ret = av_reallocp_array(&dst->fixups, nb_fixups, sizeof(FFEditFixup));
    if ( ret < 0 )
        return ret;

    memcpy(&dst->fixups[dst->nb_fixups], src->fixups, src->nb_fixups * sizeof(FFEditFixup));
    dst->nb_fixups = nb_fixups;
    src->nb_fixups = 0;

    return 0;
}

static void update_fixups(
        FFEditOutputContext *ectx,
        int64_t pos,
        int64_t delta,
        int is_padding)
{
    /* delta is non-zero */

    for ( size_t i = 0; i < ectx->nb_fixups; i++ )
    {
        FFEditFixup *fixup = &ectx->fixups[i];
        int ok = 0;
        switch ( fixup->type )
        {
        case FFEDIT_FIXUP_OFFSET_LE32:
        case FFEDIT_FIXUP_OFFSET_BE32:
            if ( pos < fixup->a1 )
                fixup->val += delta;
            break;
        case FFEDIT_FIXUP_SIZE_LE32:
        case FFEDIT_FIXUP_SIZE_BE32:
            // padding packets should not modify fixups that have the same
            // start position. they are padding the previous packet, and not
            // adding to the beginning of the next packet.
            ok = is_padding ? (pos > fixup->a1) : (pos >= fixup->a1);
            if ( ok && pos <= fixup->a2 )
                fixup->val += delta;
            break;
        }
        if ( pos < fixup->pos )
            fixup->pos += delta;
        if ( pos <= fixup->a1 )
            fixup->a1 += delta;
        if ( pos <= fixup->a2 )
            fixup->a2 += delta;
    }
}

static void apply_fixups(FFEditOutputContext *ectx)
{
    AVIOContext *o_pb = ectx->o_pb;
    for ( size_t i = 0; i < ectx->nb_fixups; i++ )
    {
        FFEditFixup *fixup = &ectx->fixups[i];
        avio_seek(o_pb, fixup->pos, SEEK_SET);
        switch ( fixup->type )
        {
        case FFEDIT_FIXUP_OFFSET_LE32:
        case FFEDIT_FIXUP_SIZE_LE32:
            avio_wl32(o_pb, fixup->val);
            break;
        case FFEDIT_FIXUP_OFFSET_BE32:
        case FFEDIT_FIXUP_SIZE_BE32:
            avio_wb32(o_pb, fixup->val);
            break;
        }
    }
}

static void replicate(AVIOContext *o_pb, AVIOContext *i_pb, size_t size)
{
    uint8_t *buf;
    if ( size == 0 )
        return;
    buf = av_malloc(size);
    size = avio_read(i_pb, buf, size);
    avio_write(o_pb, buf, size);
    av_freep(&buf);
}

static int cmp_packet(const void *a, const void *b)
{
    FFEditPacket *fa = (FFEditPacket *) a;
    FFEditPacket *fb = (FFEditPacket *) b;
    return fa->i_pos - fb->i_pos;
}

#if DEBUG_FLUSH
static const char *const fixup_type_name[] = {
    "offs_le32",
    "offs_be32",
    "size_le32",
    "size_be32",
};
#endif

int ffe_output_flush(FFEditOutputContext *ectx, AVFormatContext *fctx)
{
    AVIOContext *o_pb = ectx->o_pb;
    AVIOContext *i_pb = fctx->pb;

    /* if directwrite is set, all packets have already been written
     * and there is nothing to do here. */
    if ( ectx->directwrite )
        return 0;

    /* sort packets according to input file position */
    qsort(ectx->packets, ectx->nb_packets, sizeof(FFEditPacket), cmp_packet);

#if DEBUG_FLUSH
    /* dump packets */
    av_log(ectx, AV_LOG_DEBUG, "packets (%zu)\n", ectx->nb_packets);
    for ( size_t i = 0; i < ectx->nb_packets; i++ )
    {
        FFEditPacket *packet = &ectx->packets[i];
        int64_t delta = (int64_t) packet->o_size - packet->i_size;
        av_log(ectx, AV_LOG_DEBUG, " [%4zu] pos %08x alignment %d size (%08x - %08x = %d)\n", i, (uint32_t) packet->i_pos, packet->alignment, (uint32_t) packet->i_size, (uint32_t) packet->o_size, (int32_t) delta);
    }

    /* dump fixups */
    av_log(ectx, AV_LOG_DEBUG, "fixups (%zu)\n", ectx->nb_fixups);
    for ( size_t i = 0; i < ectx->nb_fixups; i++ )
    {
        FFEditFixup *fixup = &ectx->fixups[i];
        av_log(ectx, AV_LOG_DEBUG, " [%4zu] [%s] pos %08x a1 %08x a2 %08x val %08x\n", i, fixup_type_name[fixup->type], (uint32_t) fixup->pos, (uint32_t) fixup->a1, (uint32_t) fixup->a2, (uint32_t) fixup->val);
    }
#endif

    /* seek to position 0 */
    avio_seek(i_pb, 0, SEEK_SET);

    /* go through all packets */
    for ( size_t i = 0; i < ectx->nb_packets; i++ )
    {
        FFEditPacket *packet = &ectx->packets[i];
        int is_padding = (packet->alignment != 0);
        int64_t delta;
        int64_t o_pos;

        /* write skipped data */
        replicate(o_pb, i_pb, packet->i_pos - avio_tell(i_pb));

        /* skip old packet from input file */
        avio_skip(i_pb, packet->i_size);

        /* write new packet to output file */
        o_pos = avio_tell(o_pb);
        if ( is_padding )
            packet->o_size = -o_pos & (packet->alignment - 1);
        avio_write(o_pb, packet->data, packet->o_size);

        /* update fixups if needed */
        delta = (int64_t) packet->o_size - packet->i_size;
        if ( delta != 0 )
        {
#if DEBUG_FLUSH
            av_log(ectx, AV_LOG_DEBUG, "%s(pos %08x, delta %" PRId64 ")\n", __func__, (uint32_t) pos, delta);
#endif
            update_fixups(ectx, o_pos, delta, is_padding);
        }
    }

    /* write trailer */
    while ( !avio_feof(i_pb) )
        replicate(o_pb, i_pb, 1024);

    /* apply fixups */
    apply_fixups(ectx);

    return 0;
}

void ffe_output_freep(FFEditOutputContext **pectx)
{
    FFEditOutputContext *ectx = *pectx;
    int i;

    for ( i = 0; i < ectx->nb_packets; i++ )
        av_freep(&ectx->packets[i].data);
    av_freep(&ectx->packets);
    av_freep(&ectx->fixups);

    avio_closep(&ectx->o_pb);

    av_freep(pectx);
}
