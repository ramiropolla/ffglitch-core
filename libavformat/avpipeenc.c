/*
 * AVPipe muxer
 * Copyright (c) 2023 Ramiro Polla
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

#include "internal.h"
#include "mux.h"

static int avpipe_write_header(AVFormatContext *s)
{
    AVIOContext *pb = s->pb;
    avio_wl32(pb, MKTAG('a', 'v', 'p', 'p'));
    avio_wl32(pb, s->nb_streams);
    for ( int i = 0; i < s->nb_streams; i++ )
    {
        AVStream *st = s->streams[i];
        AVCodecParameters *par = st->codecpar;
        avio_wl32(pb, par->codec_type);
        avio_wl32(pb, par->codec_id);
        avio_wl32(pb, par->width);
        avio_wl32(pb, par->height);
        avio_wl32(pb, st->time_base.den);
        avio_wl32(pb, st->time_base.num);
    }
    return 0;
}

static int avpipe_write_packet(AVFormatContext *s, AVPacket *pkt)
{
    AVIOContext *pb = s->pb;
    avio_wl32(pb, pkt->size);
    if ( pkt->size != 0 )
      avio_write(pb, pkt->data, pkt->size);
    avio_wl32(pb, pkt->stream_index);
    avio_wl32(pb, pkt->flags);
    avio_wl64(pb, pkt->pts);
    avio_wl64(pb, pkt->dts);
    return 0;
}

const FFOutputFormat ff_avpipe_muxer = {
    .p.name            = "avpipe",
    .p.long_name       = NULL_IF_CONFIG_SMALL("AVPipe"),
    .p.flags           = AVFMT_VARIABLE_FPS,
    .p.extensions      = "avpipe",
    .p.audio_codec     = AV_CODEC_ID_NONE,
    .p.video_codec     = AV_CODEC_ID_RAWVIDEO,
    .write_header      = avpipe_write_header,
    .write_packet      = avpipe_write_packet,
};
