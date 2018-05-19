
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
    memcpy(ffedit_sd, f->ffedit_sd, sizeof(ffedit_sd));
    jctx = f->jctx;
    av_frame_unref(f);
    memcpy(f->ffedit_sd, ffedit_sd, sizeof(ffedit_sd));
    f->jctx = jctx;
}

//---------------------------------------------------------------------
static void
ffe_mjpeg_init(MJpegDecodeContext *s)
{
    memcpy(s->picture_ptr->ffedit_sd, s->ffedit_sd, sizeof(s->ffedit_sd));
    s->picture_ptr->jctx = s->jctx;
}

//---------------------------------------------------------------------
static void
ffe_mjpeg_prepare_frame(MJpegDecodeContext *s, AVPacket *avpkt)
{
    memcpy(s->ffedit_sd, avpkt->ffedit_sd, sizeof(s->ffedit_sd));
    s->jctx = avpkt->jctx;
    ffe_mjpeg_init(s);
}
