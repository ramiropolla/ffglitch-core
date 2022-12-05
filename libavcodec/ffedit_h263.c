
/* This file is included by h263dec.c */

#include "ffedit_json.h"
#include "ffedit_mb.h"
#include "ffedit_mv.h"

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

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV)) != 0 )
    {
        ffe_mv_export_init(s->jctx, f, s->mb_height, s->mb_width, 1, 4);
        ffe_mv_export_fcode(s->jctx, f, 0, 0, s->f_code);
        ffe_mv_export_fcode(s->jctx, f, 1, 0, s->b_code);
    }
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MV)) != 0 )
    {
        ffe_mv_import_init(s->jctx, f);
    }

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV_DELTA)) != 0 )
    {
        ffe_mv_delta_export_init(s->jctx, f, s->mb_height, s->mb_width, 1, 4);
        ffe_mv_delta_export_fcode(s->jctx, f, 0, 0, s->f_code);
        ffe_mv_delta_export_fcode(s->jctx, f, 1, 0, s->b_code);
    }
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MV_DELTA)) != 0 )
    {
        ffe_mv_delta_import_init(s->jctx, f);
    }

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MB)) != 0 )
        ffe_mb_export_init(s->jctx, f, s->mb_height, s->mb_width);
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MB)) != 0 )
        ffe_mb_import_init(s->jctx, f);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_GMC)) != 0 )
    {
        if ( s->jgmc != NULL )
        {
            /* move data from header_jctx to jctx */
            size_t length = json_array_length(s->jgmc);
            json_t *jgmc = json_array_new(s->jctx, length);
            for ( size_t i = 0; i < length; i++ )
            {
                json_t *src = json_array_get(s->jgmc, i);
                json_t *dst = json_array_of_ints_new(s->jctx, 2);
                json_set_pflags(dst, JSON_PFLAGS_NO_LF);
                dst->array_of_ints[0] = src->array_of_ints[0];
                dst->array_of_ints[1] = src->array_of_ints[1];
                json_array_set(jgmc, i, dst);
            }
            f->ffedit_sd[FFEDIT_FEAT_GMC] = jgmc;
            s->jgmc = NULL;
        }
    }
}

//---------------------------------------------------------------------
static void
ffe_h263_export_cleanup(MpegEncContext *s, AVFrame *f)
{
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_INFO)) != 0 )
        ffe_h263_export_info_cleanup(s, f);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV)) != 0 )
        ffe_mv_export_cleanup(s->jctx, f);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV_DELTA)) != 0 )
        ffe_mv_delta_export_cleanup(s->jctx, f);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MB)) != 0 )
        ffe_mb_export_cleanup(s->jctx, f);
}
