
/* This file is included by mpeg4videodec.c */

#include "ffedit_json.h"
#include "ffedit_mv.h"

//---------------------------------------------------------------------
// info

//---------------------------------------------------------------------
static void
ffe_mpeg4_export_info(MpegEncContext *s, int mb_type)
{
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_INFO)) != 0 )
    {
        AVFrame *f = s->current_picture_ptr->f;
        json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_INFO];
        json_t *jmb_type = json_object_get(jframe, "mb_type");
        json_t *jso;
        char buf[16];
        char *ptr = buf;

        *ptr++ = (mb_type & MB_TYPE_INTRA)      ? 'I'
               : (mb_type & MB_TYPE_DIRECT2)    ? 'D'
               :                                  ' ';
        *ptr++ = (mb_type & MB_TYPE_CBP)        ? 'c' : ' ';
        *ptr++ = (mb_type & MB_TYPE_QUANT)      ? 'q' : ' ';
        *ptr++ = (mb_type & MB_TYPE_16x16)      ? 'F'
               : (mb_type & MB_TYPE_16x8)       ? 'H'
               : (mb_type & MB_TYPE_8x8)        ? '4'
               :                                  ' ';
        *ptr++ = (mb_type & MB_TYPE_GMC)        ? 'G' : ' ';
        *ptr++ = (mb_type & MB_TYPE_L0)         ? 'f' : ' ';
        *ptr++ = (mb_type & MB_TYPE_L1)         ? 'b' : ' ';
        *ptr++ = (mb_type & MB_TYPE_ACPRED)     ? 'a' : ' ';
        *ptr++ = (mb_type & MB_TYPE_INTERLACED) ? 'i' : ' ';
        *ptr++ = (mb_type & MB_TYPE_SKIP)       ? 'S' : ' ';
        *ptr = '\0';

        jso = json_string_new(f->jctx, buf);
        ffe_jmb_set(jmb_type, 0, s->mb_y, s->mb_x, 0, jso);
    }
}

//---------------------------------------------------------------------
// mv

//---------------------------------------------------------------------
static void
ffe_mpeg4_mv_init_mb(
        MpegEncContext *s,
        int nb_directions,
        int nb_blocks)
{
    AVFrame *f = s->current_picture_ptr->f;
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV)) != 0 )
        ffe_mv_export_init_mb(f->jctx, f, s->mb_y, s->mb_x, nb_directions, nb_blocks);
    if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MV)) != 0 )
        ffe_mv_import_init_mb(f->jctx, f, s->mb_y, s->mb_x, nb_directions, nb_blocks);
}

//---------------------------------------------------------------------
static int
ffe_mpeg4_decode_motion(
        MpegEncContext *s,
        int pred,
        int f_code,
        int x_or_y)     // 0 = x, 1 = y
{
    AVFrame *f = s->current_picture_ptr->f;
    int code;

    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_MV)) != 0 )
        s->pb = *ffe_transplicate_save(&s->ffe_xp);

    code = ff_h263_decode_motion(s, pred, f_code);
    if ( code == 0xffff )
        return code;

    if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MV)) != 0 )
        code = ffe_mv_get(f, x_or_y);
    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_MV)) != 0 )
    {
        ff_h263_encode_motion(&s->pb, code - pred, f_code);
        ffe_transplicate_restore(&s->ffe_xp, &s->pb);
    }
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV)) != 0 )
        ffe_mv_set(f, x_or_y, code);

    return code;
}

//---------------------------------------------------------------------
static void
ffe_mpeg4_mv_not_supported(MpegEncContext *s)
{
    if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MV)) != 0
      || (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV)) != 0
      || (s->avctx->ffedit_apply  & (1 << FFEDIT_FEAT_MV)) != 0 )
    {
        av_log(NULL, AV_LOG_ERROR,
               "FFedit doesn't support the motion vectors in this file.\n");
        av_assert0(0);
    }
}
