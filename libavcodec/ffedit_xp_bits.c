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

#include "ffedit.h"
#include "ffedit_xp_bits.h"
#include "internal.h"

/*-------------------------------------------------------------------*/
static const AVClass ffe_transplicate_bits_class = {
    .class_name = "FFEditTransplicateBits",
    .item_name  = av_default_item_name,
    .version    = LIBAVUTIL_VERSION_INT,
};

/*-------------------------------------------------------------------*/
int ffe_transplicate_bits_init(
        AVCodecContext *avctx,
        FFEditTransplicateBitsContext *xp,
        size_t pkt_size)
{
    if ( xp->pb != NULL )
        return 0;

    xp->av_class = &ffe_transplicate_bits_class;

    // allocate buffer at least 50% bigger than original packet size
    // and at least 3 MB.
    pkt_size = FFMAX(pkt_size, 0x200000);
    pkt_size += pkt_size >> 1;

    xp->data = av_malloc(pkt_size);
    if ( xp->data == NULL )
        return AVERROR(ENOMEM);

    xp->pb = av_malloc(sizeof(PutBitContext));
    if ( xp->pb == NULL )
    {
        av_freep(&xp->data);
        return AVERROR(ENOMEM);
    }

    init_put_bits(xp->pb, xp->data, pkt_size);

    return 0;
}

/*-------------------------------------------------------------------*/
void ffe_transplicate_bits_merge(
        AVCodecContext *avctx,
        FFEditTransplicateBitsContext *dst_xp,
        FFEditTransplicateBitsContext *src_xp)
{
    if ( dst_xp->pb != NULL && src_xp != NULL )
        ff_copy_bits(dst_xp->pb, src_xp->pb->buf, put_bits_count(src_xp->pb));
}

/*-------------------------------------------------------------------*/
void ffe_transplicate_bits_free(FFEditTransplicateBitsContext *xp)
{
    if ( xp->pb == NULL )
        return;
    av_freep(&xp->pb);
    av_freep(&xp->data);
}

/*-------------------------------------------------------------------*/
void ffe_transplicate_bits_flush(
        AVCodecContext *avctx,
        FFEditTransplicateBitsContext *xp,
        const AVPacket *pkt)
{
    FFEditTransplicatePacket *packet;
    size_t nb_ffe_xp_packets;
    size_t idx;
    int bitcount;
    int ret;

    if ( xp->pb == NULL )
        return;

    bitcount = put_bits_count(xp->pb);
    flush_put_bits(xp->pb);

    nb_ffe_xp_packets = avctx->nb_ffe_xp_packets;
    idx = nb_ffe_xp_packets++;

    ret = av_reallocp_array(&avctx->ffe_xp_packets, nb_ffe_xp_packets, sizeof(FFEditTransplicatePacket));
    if ( ret < 0 )
        return;

    avctx->nb_ffe_xp_packets = nb_ffe_xp_packets;

    packet = &avctx->ffe_xp_packets[idx];
    packet->i_pos = pkt->pos;
    packet->i_size = pkt->size;
    packet->o_size = (bitcount + 7) >> 3;
    packet->data = av_malloc(packet->o_size);
    memcpy(packet->data, xp->data, packet->o_size);

    ffe_transplicate_bits_free(xp);
}

/*-------------------------------------------------------------------*/
PutBitContext *ffe_transplicate_bits_pb(FFEditTransplicateBitsContext *xp)
{
    return xp->pb;
}

/*-------------------------------------------------------------------*/
PutBitContext *ffe_transplicate_bits_save(FFEditTransplicateBitsContext *xp)
{
    xp->saved = *(xp->pb);
    return &xp->saved;
}

/*-------------------------------------------------------------------*/
void ffe_transplicate_bits_restore(
        FFEditTransplicateBitsContext *xp,
        PutBitContext *saved)
{
    *(xp->pb) = *saved;
}
