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
#include "ffedit_xp_bytes.h"
#include "internal.h"

/*-------------------------------------------------------------------*/
static const AVClass ffe_transplicate_bytes_class = {
    .class_name = "FFEditTransplicateBytes",
    .item_name  = av_default_item_name,
    .version    = LIBAVUTIL_VERSION_INT,
};

/*-------------------------------------------------------------------*/
int ffe_transplicate_bytes_init(
        AVCodecContext *avctx,
        FFEditTransplicateBytesContext *xp,
        size_t pkt_size)
{
    if ( xp->pb != NULL )
        return 0;

    xp->av_class = &ffe_transplicate_bytes_class;

    // allocate buffer at least 50% bigger than original packet size
    // and at least 3 MB.
    pkt_size = FFMAX(pkt_size, 0x200000);
    pkt_size += pkt_size >> 1;

    xp->data = av_malloc(pkt_size);
    if ( xp->data == NULL )
        return AVERROR(ENOMEM);

    xp->pb = av_malloc(sizeof(PutByteContext));
    if ( xp->pb == NULL )
    {
        av_freep(&xp->data);
        return AVERROR(ENOMEM);
    }

    bytestream2_init_writer(xp->pb, xp->data, pkt_size);

    return 0;
}

/*-------------------------------------------------------------------*/
void ffe_transplicate_bytes_free(FFEditTransplicateBytesContext *xp)
{
    if ( xp->pb == NULL )
        return;
    av_freep(&xp->pb);
    av_freep(&xp->data);
}

/*-------------------------------------------------------------------*/
void ffe_transplicate_bytes_flush(
        AVCodecContext *avctx,
        FFEditTransplicateBytesContext *xp,
        const AVPacket *pkt)
{
    FFEditTransplicatePacket *packet;
    size_t nb_ffe_xp_packets;
    size_t idx;
    int bytecount;
    int ret;

    if ( xp->pb == NULL )
        return;

    bytecount = bytestream2_tell_p(xp->pb);

    nb_ffe_xp_packets = avctx->nb_ffe_xp_packets;
    idx = nb_ffe_xp_packets++;

    ret = av_reallocp_array(&avctx->ffe_xp_packets, nb_ffe_xp_packets, sizeof(FFEditTransplicatePacket));
    if ( ret < 0 )
        return;

    avctx->nb_ffe_xp_packets = nb_ffe_xp_packets;

    packet = &avctx->ffe_xp_packets[idx];
    packet->i_pos = pkt->pos;
    packet->i_size = pkt->size;
    packet->o_size = bytecount;
    packet->data = av_malloc(packet->o_size);
    memcpy(packet->data, xp->data, packet->o_size);

    ffe_transplicate_bytes_free(xp);
}

/*-------------------------------------------------------------------*/
PutByteContext *ffe_transplicate_bytes_pb(FFEditTransplicateBytesContext *xp)
{
    return xp->pb;
}

/*-------------------------------------------------------------------*/
PutByteContext *ffe_transplicate_bytes_save(FFEditTransplicateBytesContext *xp)
{
    xp->saved = *(xp->pb);
    return &xp->saved;
}

/*-------------------------------------------------------------------*/
void ffe_transplicate_bytes_restore(
        FFEditTransplicateBytesContext *xp,
        PutByteContext *saved)
{
    *(xp->pb) = *saved;
}
