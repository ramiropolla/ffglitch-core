
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
    // and at least 1.5 MB.
    pkt_size = FFMAX(pkt_size, 0x100000);
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

/*-------------------------------------------------------------------*/
void ffe_transplicate_flush(
        AVCodecContext *avctx,
        FFEditTransplicateContext *xp,
        AVPacket *pkt)
{
    int bitcount;
    if ( xp->pb == NULL )
        return;
    bitcount = put_bits_count(xp->pb);
    flush_put_bits(xp->pb);
    avctx->ffedit_out_size = (bitcount + 7) >> 3;
    avctx->ffedit_out = av_malloc(avctx->ffedit_out_size);
    memcpy(avctx->ffedit_out, xp->data, avctx->ffedit_out_size);
    ffe_transplicate_free(xp);
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
