
#include "cas9.h"
#include "cas9_xp.h"
#include "internal.h"

/*-------------------------------------------------------------------*/
static const AVClass cas9_transplicate_class = {
    .class_name = "CAS9Transplicate",
    .item_name  = av_default_item_name,
    .version    = LIBAVUTIL_VERSION_INT,
};

/*-------------------------------------------------------------------*/
int cas9_transplicate_init(
        AVCodecContext *avctx,
        CAS9TransplicateContext *xp,
        size_t pkt_size)
{
    int ret;

    if ( xp->o_pb != NULL )
        return 0;

    xp->av_class = &cas9_transplicate_class;

    xp->o_pkt = av_packet_alloc();
    if ( xp->o_pkt == NULL )
        return AVERROR(ENOMEM);

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
void cas9_transplicate_free(CAS9TransplicateContext *xp)
{
    if ( xp->o_pb == NULL )
        return;
    av_freep(&xp->o_pb);
    av_packet_free(&xp->o_pkt);
}

/*-------------------------------------------------------------------*/
void cas9_transplicate_flush(
        AVCodecContext *avctx,
        CAS9TransplicateContext *xp,
        AVPacket *pkt)
{
    int bitcount;
    if ( xp->o_pb == NULL )
        return;
    bitcount = put_bits_count(xp->o_pb);
    flush_put_bits(xp->o_pb);
    avctx->cas9_out_size = (bitcount + 7) >> 3;
    avctx->cas9_out = av_malloc(avctx->cas9_out_size);
    memcpy(avctx->cas9_out, xp->o_pkt->data, avctx->cas9_out_size);
    cas9_transplicate_free(xp);
}

/*-------------------------------------------------------------------*/
PutBitContext *cas9_transplicate_pb(CAS9TransplicateContext *xp)
{
    return xp->o_pb;
}

/*-------------------------------------------------------------------*/
PutBitContext *cas9_transplicate_save(CAS9TransplicateContext *xp)
{
    xp->saved = *(xp->o_pb);
    return &xp->saved;
}

/*-------------------------------------------------------------------*/
void cas9_transplicate_restore(
        CAS9TransplicateContext *xp,
        PutBitContext *saved)
{
    *(xp->o_pb) = *saved;
}
