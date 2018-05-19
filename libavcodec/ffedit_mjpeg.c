
/* This file is included by mjpegdec.c */

#include "ffedit_json.h"

//---------------------------------------------------------------------
// info

// TODO

/*-------------------------------------------------------------------*/
/* common                                                            */
/*-------------------------------------------------------------------*/

//---------------------------------------------------------------------
static void
ffe_mjpeg_frame_unref(AVFrame *f)
{
    void *ffedit_sd[FFEDIT_FEAT_LAST] = { 0 };
    void *jctx = NULL;
    memcpy(ffedit_sd, f->ffedit_sd, sizeof(json_t *)*FFEDIT_FEAT_LAST);
    jctx = f->jctx;
    av_frame_unref(f);
    memcpy(f->ffedit_sd, ffedit_sd, sizeof(json_t *)*FFEDIT_FEAT_LAST);
    f->jctx = jctx;
}

//---------------------------------------------------------------------
static void
ffe_mjpeg_init(MJpegDecodeContext *s)
{
    if ( s->avctx->ffedit_import != 0 )
        memcpy(s->picture_ptr->ffedit_sd, s->ffedit_sd, sizeof(json_t *)*FFEDIT_FEAT_LAST);
}

//---------------------------------------------------------------------
static void
ffe_mjpeg_prepare_frame(AVCodecContext *avctx, MJpegDecodeContext *s, const AVPacket *avpkt)
{
    if ( avctx->ffedit_export != 0 )
    {
        /* create one jctx for each exported frame */
        AVFrame *f = s->picture_ptr;
        f->jctx = av_mallocz(sizeof(json_ctx_t));
        json_ctx_start(f->jctx, 1);
        s->jctx = f->jctx;
    }
    else if ( avctx->ffedit_import != 0 )
    {
        memcpy(s->ffedit_sd, avpkt->ffedit_sd, sizeof(json_t *)*FFEDIT_FEAT_LAST);
        s->jctx = avpkt->jctx;
    }
    ffe_mjpeg_init(s);
}
