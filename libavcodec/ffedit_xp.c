
#include "ffedit.h"
#include "ffedit_xp.h"
#include "internal.h"

/*-------------------------------------------------------------------*/
static const AVClass ffe_transplicate_class = {
    .class_name = "FFEditTransplicate",
    .item_name  = av_default_item_name,
    .version    = LIBAVUTIL_VERSION_INT,
};

/*-------------------------------------------------------------------*/
int ffe_transplicate_init(
        AVCodecContext *avctx,
        FFEditTransplicateContext *xp,
        size_t pkt_size)
{
    if ( xp->pb != NULL )
        return 0;

    xp->av_class = &ffe_transplicate_class;

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
void ffe_transplicate_merge(
        AVCodecContext *avctx,
        FFEditTransplicateContext *dst_xp,
        FFEditTransplicateContext *src_xp)
{
    if ( dst_xp->pb != NULL && src_xp != NULL )
        avpriv_copy_bits(dst_xp->pb, src_xp->pb->buf, put_bits_count(src_xp->pb));
}

/*-------------------------------------------------------------------*/
void ffe_transplicate_free(FFEditTransplicateContext *xp)
{
    if ( xp->pb == NULL )
        return;
    av_freep(&xp->pb);
    av_freep(&xp->data);
}

#if CONFIG_FFEDIT_XP_DEBUG
/*-------------------------------------------------------------------*/
static char *dump_byte(char *buf, uint8_t val)
{
    for ( int i = 0; i < 8; i++ )
    {
        int bit = val & (1<<(8-i-1));
        *buf++ = !!bit + '0';
    }
    *buf = '\0';
    return buf;
}
#endif

/*-------------------------------------------------------------------*/
void ffe_transplicate_flush(
        AVCodecContext *avctx,
        FFEditTransplicateContext *xp,
        AVPacket *pkt)
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

    ffe_transplicate_free(xp);

#if CONFIG_FFEDIT_XP_DEBUG
    /* Check only when replicating */
    if ( avctx->ffedit_apply == (1 << FFEDIT_FEAT_LAST) )
    {
        const uint8_t *inptr = packet->data;
        size_t out_size = packet->o_size;
        for ( int i = 0; i < out_size; i++ )
        {
            if ( inptr[i] != pkt->data[i] )
            {
                char buf1[(8+1)*8 + 1];
                char buf2[(8+1)*8 + 1];
                char *ptr1 = buf1;
                char *ptr2 = buf2;
                int start = FFMAX(0, i-2);
                int end = FFMIN(out_size, i+6);
                int j;
                for ( j = 0; j < 8; j++ )
                    if ( (inptr[i] & (1<<(8-j-1))) != (pkt->data[i] & (1<<(8-j-1))) )
                        break;
                if ( i * 8 + j >= bitcount )
                    continue;
                for ( int k = start; k < end; k++ )
                {
                    *ptr1++ = ' ';
                    ptr1 = dump_byte(ptr1, pkt->data[k]);
                    *ptr2++ = ' ';
                    ptr2 = dump_byte(ptr2, inptr[k]);
                }
                av_log(ffe_class, AV_LOG_FATAL, "orig   %d: %s\n", start * 8, buf1);
                av_log(ffe_class, AV_LOG_FATAL, "ffedit %d: %s\n", start * 8, buf2);
                av_log(ffe_class, AV_LOG_FATAL, "ffedit replication mismatch at bit %d + %d (bitcount %d)\n", i * 8, j, bitcount);
                av_assert0(0);
            }
        }
    }
#endif
}

/*-------------------------------------------------------------------*/
PutBitContext *ffe_transplicate_pb(FFEditTransplicateContext *xp)
{
    return xp->pb;
}

/*-------------------------------------------------------------------*/
PutBitContext *ffe_transplicate_save(FFEditTransplicateContext *xp)
{
    xp->saved = *(xp->pb);
    return &xp->saved;
}

/*-------------------------------------------------------------------*/
void ffe_transplicate_restore(
        FFEditTransplicateContext *xp,
        PutBitContext *saved)
{
    *(xp->pb) = *saved;
}
