
/* This file is included by h263dec.c */

#include "ffedit_json.h"
#include "ffedit_mv.h"

#include "ffedit_mpegvideo.c"

//---------------------------------------------------------------------
// info

/* init */

//---------------------------------------------------------------------
static void
ffe_h263_export_info_init(MpegEncContext *s)
{
    AVFrame *f = s->current_picture_ptr->f;
    json_t *jframe = json_object_new(s->jctx);
    json_t *jobj;
    int one = 1;

    jobj = json_string_new(s->jctx,
         s->pict_type == AV_PICTURE_TYPE_I ? "I" :
        (s->pict_type == AV_PICTURE_TYPE_P ? "P" :
        (s->pict_type == AV_PICTURE_TYPE_B ? "B" : "S")));

    json_object_add(jframe, "pict_type", jobj);

    jobj = ffe_jmb_new(s->jctx,
                       s->mb_width, s->mb_height,
                       one, &one, &one,
                       JSON_PFLAGS_NO_LF);

    json_object_add(jframe, "mb_type", jobj);

    f->ffedit_sd[FFEDIT_FEAT_INFO] = jframe;
}

/* cleanup */

/*-------------------------------------------------------------------*/
/* common                                                            */
/*-------------------------------------------------------------------*/

static void
ffe_h263_prepare_frame(AVCodecContext *avctx, MpegEncContext *s, AVPacket *avpkt)
{
    if ( s->avctx->ffedit_import != 0 )
    {
        memcpy(s->ffedit_sd, avpkt->ffedit_sd, sizeof(s->ffedit_sd));
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
        memcpy(f->ffedit_sd, s->ffedit_sd, sizeof(f->ffedit_sd));

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_INFO)) != 0 )
        ffe_h263_export_info_init(s);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV)) != 0 )
    {
        ffe_mv_export_init(s->jctx, f, s->mb_height, s->mb_width, 1);
        ffe_mv_export_fcode(s->jctx, f, 0, 0, s->f_code);
        ffe_mv_export_fcode(s->jctx, f, 1, 0, s->b_code);
    }
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MV)) != 0 )
    {
        ffe_mv_import_init(s->jctx, f);
    }

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV_DELTA)) != 0 )
    {
        ffe_mv_delta_export_init(s->jctx, f, s->mb_height, s->mb_width, 1);
        ffe_mv_delta_export_fcode(s->jctx, f, 0, 0, s->f_code);
        ffe_mv_delta_export_fcode(s->jctx, f, 1, 0, s->b_code);
    }
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MV_DELTA)) != 0 )
    {
        ffe_mv_delta_import_init(s->jctx, f);
    }
}

//---------------------------------------------------------------------
static void
ffe_h263_export_cleanup(MpegEncContext *s, AVFrame *f)
{
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV)) != 0 )
        ffe_mv_export_cleanup(s->jctx, f);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV_DELTA)) != 0 )
        ffe_mv_delta_export_cleanup(s->jctx, f);
}
