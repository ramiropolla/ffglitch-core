
/* This file is included by pngdec.c */

#include "ffedit.h"
#include "ffedit_json.h"
#include "libavutil/json.h"

/*-------------------------------------------------------------------*/
/* FFEDIT_FEAT_LAST                                                  */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
static int
ffe_png_transplicate_init(
        PNGDecContext *s,
        AVPacket *avpkt,
        FFEditTransplicateBytesContext *xp)
{
    AVCodecContext *avctx = s->avctx;
    if ( (avctx->ffedit_apply & (1 << FFEDIT_FEAT_LAST)) != 0 )
    {
        int ret = ffe_transplicate_bytes_init(avctx, xp, avpkt->size);
        if ( ret < 0 )
            return ret;
        s->gb.pb = ffe_transplicate_bytes_pb(xp);
    }
    return 0;
}

/*-------------------------------------------------------------------*/
static void
ffe_png_transplicate_cleanup(
        PNGDecContext *s,
        AVPacket *avpkt,
        FFEditTransplicateBytesContext *xp)
{
    AVCodecContext *avctx = s->avctx;
    if ( (avctx->ffedit_apply & (1 << FFEDIT_FEAT_LAST)) != 0 )
        ffe_transplicate_bytes_flush(avctx, xp, avpkt);
}

/*-------------------------------------------------------------------*/
static int
ffe_png_prepare_frame(PNGDecContext *s, AVFrame *f, AVPacket *avpkt)
{
    AVCodecContext *avctx = s->avctx;

    /* jctx */
    if ( avctx->ffedit_export != 0 )
        f->jctx = json_ctx_new(1);
    else if ( avctx->ffedit_import != 0 )
        f->jctx = avpkt->jctx;

    /* ffedit_sd */
    if ( avctx->ffedit_import != 0 )
        memcpy(f->ffedit_sd, avpkt->ffedit_sd, sizeof(json_t *)*FFEDIT_FEAT_LAST);

    return 0;
}

/*-------------------------------------------------------------------*/
static void
ffe_png_cleanup_frame(PNGDecContext *s, AVFrame *f)
{
}
