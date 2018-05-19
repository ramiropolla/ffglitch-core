
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
    int ret;

    if ( xp->o_pb != NULL )
        return 0;

    xp->av_class = &ffe_transplicate_class;

    xp->o_pkt = av_packet_alloc();
    if ( xp->o_pkt == NULL )
        return AVERROR(ENOMEM);

    // allocate buffer at least 50% bigger than original packet size
    // and at least 1.5 MB.
    pkt_size = FFMAX(pkt_size, 0x100000);
    pkt_size += pkt_size >> 1;

    ret = ff_alloc_packet2(avctx, xp->o_pkt, pkt_size, 0);
    if ( ret < 0 )
    {
        av_packet_free(&xp->o_pkt);
        return ret;
    }

    xp->o_pb = av_malloc(sizeof(PutBitContext));
    if ( xp->o_pb == NULL )
    {
        av_packet_free(&xp->o_pkt);
        return AVERROR(ENOMEM);
    }

    init_put_bits(xp->o_pb, xp->o_pkt->data, pkt_size);

    return 0;
}

/*-------------------------------------------------------------------*/
void ffe_transplicate_free(FFEditTransplicateContext *xp)
{
    if ( xp->o_pb == NULL )
        return;
    av_freep(&xp->o_pb);
    av_packet_free(&xp->o_pkt);
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
    int bitcount;
    if ( xp->o_pb == NULL )
        return;
    bitcount = put_bits_count(xp->o_pb);
    flush_put_bits(xp->o_pb);
    avctx->ffedit_out_size = (bitcount + 7) >> 3;
    avctx->ffedit_out = av_malloc(avctx->ffedit_out_size);
    memcpy(avctx->ffedit_out, xp->o_pkt->data, avctx->ffedit_out_size);
    ffe_transplicate_free(xp);
#if CONFIG_FFEDIT_XP_DEBUG
    /* Check only when replicating */
    if ( avctx->ffedit_apply == (1 << FFEDIT_FEAT_LAST) )
    {
        const uint8_t *inptr = avctx->ffedit_out;
        size_t out_size = avctx->ffedit_out_size;
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
                av_log(NULL, AV_LOG_FATAL, "orig   %d: %s\n", start * 8, buf1);
                av_log(NULL, AV_LOG_FATAL, "ffedit %d: %s\n", start * 8, buf2);
                av_log(NULL, AV_LOG_FATAL, "ffedit replication mismatch at bit %d + %d (bitcount %d)\n", i * 8, j, bitcount);
                av_assert0(0);
            }
        }
    }
#endif
}

/*-------------------------------------------------------------------*/
PutBitContext *ffe_transplicate_pb(FFEditTransplicateContext *xp)
{
    return xp->o_pb;
}

/*-------------------------------------------------------------------*/
PutBitContext *ffe_transplicate_save(FFEditTransplicateContext *xp)
{
    xp->saved = *(xp->o_pb);
    return &xp->saved;
}

/*-------------------------------------------------------------------*/
void ffe_transplicate_restore(
        FFEditTransplicateContext *xp,
        PutBitContext *saved)
{
    *(xp->o_pb) = *saved;
}
