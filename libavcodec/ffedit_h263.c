
/* This file is included by h263dec.c */

#include "ffedit_json.h"

#include "ffedit_mpegvideo.c"

//---------------------------------------------------------------------
// info

/* init */

//---------------------------------------------------------------------
static void
ffe_h263_export_info_init(MpegEncContext *s, AVFrame *f)
{
    json_t *jframe = json_object_new(s->jctx);
    json_t *jso;
    const char *pict_type = s->pict_type == AV_PICTURE_TYPE_I ? "I"
                          : s->pict_type == AV_PICTURE_TYPE_P ? "P"
                          : s->pict_type == AV_PICTURE_TYPE_B ? "B"
                          :                                     "S";

    jso = json_string_new(s->jctx, pict_type);

    json_object_add(jframe, "pict_type", jso);

    jso = ffe_jblock_new(s->jctx,
                         s->mb_width, s->mb_height,
                         JSON_PFLAGS_NO_LF);

    json_object_add(jframe, "mb_type", jso);

    f->ffedit_sd[FFEDIT_FEAT_INFO] = jframe;
}

/* cleanup */

static void
ffe_h263_export_info_cleanup(MpegEncContext *s, AVFrame *f)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_INFO];
    json_object_done(s->jctx, jframe);
}

/*-------------------------------------------------------------------*/
/* common                                                            */
/*-------------------------------------------------------------------*/

static void
ffe_h263_prepare_frame(AVCodecContext *avctx, MpegEncContext *s, AVPacket *avpkt)
{
    if ( s->avctx->ffedit_import != 0 )
    {
        memcpy(s->ffedit_sd, avpkt->ffedit_sd, sizeof(json_t *)*FFEDIT_FEAT_LAST);
        s->jctx = avpkt->jctx;
    }
}

//---------------------------------------------------------------------
static void
ffe_h263_export_init(MpegEncContext *s)
{
    AVFrame *f = s->current_picture_ptr->f;

    ffe_mpegvideo_jctx_init(s);
    if ( s->avctx->ffedit_import != 0 )
        memcpy(f->ffedit_sd, s->ffedit_sd, sizeof(json_t *)*FFEDIT_FEAT_LAST);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_INFO)) != 0 )
        ffe_h263_export_info_init(s, f);
}

//---------------------------------------------------------------------
static void
ffe_h263_export_cleanup(MpegEncContext *s, AVFrame *f)
{
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_INFO)) != 0 )
        ffe_h263_export_info_cleanup(s, f);
}
