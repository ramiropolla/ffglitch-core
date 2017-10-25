
#include "cas9.h"

/*-------------------------------------------------------------------*/
static const AVClass cas9_output_class = {
    .class_name = "CAS9Output",
    .item_name  = av_default_item_name,
    .version    = LIBAVUTIL_VERSION_INT,
};

int cas9_output_open(
        CAS9OutputContext **pc9,
        AVFormatContext *fctx,
        const char *filename)
{
    CAS9OutputContext *c9;
    int ret;

    c9 = av_malloc(sizeof(CAS9OutputContext));
    if ( c9 == NULL )
        return AVERROR(ENOMEM);

    memset(c9, 0, sizeof(CAS9OutputContext));
    c9->av_class = &cas9_output_class;
    c9->fctx = fctx;
    fctx->c9 = c9;

    ret = avio_open2(&c9->o_pb, filename, AVIO_FLAG_WRITE, NULL, NULL);
    if ( ret < 0 )
    {
        char errbuf[100];
        av_strerror(ret, errbuf, sizeof(errbuf));
        av_log(c9, AV_LOG_ERROR, "Unable to open %s: %s\n", filename, errbuf);
        av_freep(&c9);
        return ret;
    }

    *pc9 = c9;

    return 0;
}

int cas9_output_packet(
        CAS9OutputContext *c9,
        int64_t i_pos,
        size_t i_size,
        uint8_t *data,
        size_t o_size)
{
    size_t nb_packets = c9->nb_packets;
    size_t idx = nb_packets++;
    cas9_packet *c9_packet;
    int ret;

    ret = av_reallocp_array(&c9->packets, nb_packets, sizeof(cas9_packet));
    if ( ret < 0 )
        return ret;

    c9->nb_packets = nb_packets;

    c9_packet = &c9->packets[idx];
    c9_packet->i_pos = i_pos;
    c9_packet->i_size = i_size;
    c9_packet->o_size = o_size;
    c9_packet->data = av_malloc(o_size);
    memcpy(c9_packet->data, data, o_size);

    return 0;
}

int cas9_output_fixup(
        CAS9OutputContext *c9,
        enum CAS9FixupType type,
        int64_t pos,
        int64_t val,
        int64_t a1,
        int64_t a2)
{
    cas9_fixup *fixup;
    size_t nb_fixups;
    size_t idx;
    int ret;

    if ( c9 == NULL )
        return 0;

    nb_fixups = c9->nb_fixups;
    idx = nb_fixups++;

    ret = av_reallocp_array(&c9->fixups, nb_fixups, sizeof(cas9_fixup));
    if ( ret < 0 )
        return ret;

    c9->nb_fixups = nb_fixups;

    fixup = &c9->fixups[idx];
    fixup->pos = pos;
    fixup->a1 = a1;
    switch ( type )
    {
    case CAS9_FIXUP_OFFSET:
        fixup->a2 = 0;
        break;
    case CAS9_FIXUP_SIZE:
        fixup->a2 = a1 + a2 - 1;
        break;
    }
    fixup->val = val;
    fixup->type = type;

    return 0;
}

static void update_fixups(CAS9OutputContext *c9, int64_t pos, int64_t delta)
{
    /* delta is non-zero */

    for ( size_t i = 0; i < c9->nb_fixups; i++ )
    {
        cas9_fixup *fixup = &c9->fixups[i];
        switch ( fixup->type )
        {
        case CAS9_FIXUP_OFFSET:
            if ( pos <= fixup->a1 )
                fixup->val += delta;
            break;
        case CAS9_FIXUP_SIZE:
            if ( pos >= fixup->a1 && pos <= fixup->a2 )
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

static void apply_fixups(CAS9OutputContext *c9)
{
    AVIOContext *o_pb = c9->o_pb;
    for ( size_t i = 0; i < c9->nb_fixups; i++ )
    {
        cas9_fixup *fixup = &c9->fixups[i];
        avio_seek(o_pb, fixup->pos, SEEK_SET);
        avio_wl32(o_pb, fixup->val);
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
    cas9_packet *fa = (cas9_packet *) a;
    cas9_packet *fb = (cas9_packet *) b;
    return fa->i_pos - fb->i_pos;
}

int cas9_output_flush(CAS9OutputContext *c9)
{
    AVFormatContext *s = c9->fctx;
    AVIOContext *o_pb = c9->o_pb;
    AVIOContext *i_pb = s->pb;

    /* sort packets according to input file position */
    qsort(c9->packets, c9->nb_packets, sizeof(cas9_packet), cmp_packet);

    /* seek to position 0 */
    avio_seek(i_pb, 0, SEEK_SET);

    /* go through all packets */
    for ( size_t i = 0; i < c9->nb_packets; i++ )
    {
        cas9_packet *packet = &c9->packets[i];
        int64_t delta;
        int64_t o_pos;

        /* write skipped data */
        replicate(o_pb, i_pb, packet->i_pos - avio_tell(i_pb));

        /* skip old packet from input file */
        avio_skip(i_pb, packet->i_size);

        /* write new packet to output file */
        o_pos = avio_tell(o_pb);
        avio_write(o_pb, packet->data, packet->o_size);

        /* update fixups if needed */
        delta = (int64_t) packet->o_size - packet->i_size;
        if ( delta != 0 )
            update_fixups(c9, o_pos, delta);
    }

    /* write trailer */
    while ( !avio_feof(i_pb) )
        replicate(o_pb, i_pb, 1024);

    /* apply fixups */
    apply_fixups(c9);

    return 0;
}

void cas9_output_freep(CAS9OutputContext **pc9)
{
    CAS9OutputContext *c9 = *pc9;
    int i;

    for ( i = 0; i < c9->nb_packets; i++ )
        av_freep(&c9->packets[i].data);
    av_freep(&c9->packets);
    av_freep(&c9->fixups);

    avio_closep(&c9->o_pb);

    av_freep(pc9);
}
