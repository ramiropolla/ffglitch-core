
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
