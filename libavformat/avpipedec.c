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

#include "libavutil/intreadwrite.h"
#include "internal.h"
#include "demux.h"

static int avpipe_probe(const AVProbeData *p)
{
    if (AV_RL32(p->buf) == MKTAG('a', 'v', 'p', 'p'))
        return AVPROBE_SCORE_MAX;
    return 0;
}

static int avpipe_read_header(AVFormatContext *s)
{
    AVIOContext *pb = s->pb;
    int nb_streams;

    avio_skip(pb, 4);
    nb_streams = avio_rl32(pb);

    for ( int i = 0; i < nb_streams; i++ )
    {
        AVStream *st = avformat_new_stream(s, NULL);
        AVCodecParameters *par = st->codecpar;
        par->codec_type = avio_rl32(pb);
        par->codec_id = avio_rl32(pb);
        par->width = avio_rl32(pb);
        par->height = avio_rl32(pb);
        avpriv_set_pts_info(st, 64, avio_rl32(pb), avio_rl32(pb));
    }

    return 0;
}

static int avpipe_read_packet(AVFormatContext *s, AVPacket *pkt)
{
    AVIOContext *pb = s->pb;
    uint32_t size;

    if ( avio_feof(pb) )
        return AVERROR_EOF;

    size = avio_rl32(pb);
    av_get_packet(pb, pkt, size);
    pkt->stream_index = avio_rl32(pb);
    pkt->flags = avio_rl32(pb);
    pkt->pts = avio_rl64(pb);
    pkt->dts = avio_rl64(pb);

    if ( size == 0 )
        return AVERROR_EOF;

    return 0;
}

const FFInputFormat ff_avpipe_demuxer = {
    .p.name         = "avpipe",
    .p.long_name    = NULL_IF_CONFIG_SMALL("AVPipe"),
    .p.extensions   = "avpipe",
    .p.flags        = AVFMT_GENERIC_INDEX | AVFMT_TS_DISCONT | AVFMT_FFEDIT_BITSTREAM | AVFMT_FFEDIT_RAWSTREAM,
    .read_probe     = avpipe_probe,
    .read_header    = avpipe_read_header,
    .read_packet    = avpipe_read_packet,
};
