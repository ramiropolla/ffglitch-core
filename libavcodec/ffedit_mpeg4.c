
/* This file is included by mpeg4videodec.c */

#include "ffedit_json.h"
#include "ffedit_mb.h"
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

        jso = json_string_new(s->jctx, buf);
        ffe_jmb_set(jmb_type, 0, s->mb_y, s->mb_x, 0, jso);
    }
}

//---------------------------------------------------------------------
// mv

//---------------------------------------------------------------------
static void
ffe_mpeg4_mv_init_mb(
        ffe_mv_mb_ctx *mbctx,
        MpegEncContext *s,
        int nb_directions,
        int nb_blocks)
{
    AVFrame *f = s->current_picture_ptr->f;
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV)) != 0 )
        ffe_mv_export_init_mb(mbctx, s->jctx, f, s->mb_y, s->mb_x, nb_directions, nb_blocks);
    if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MV)) != 0 )
        ffe_mv_import_init_mb(mbctx, s->jctx, f, s->mb_y, s->mb_x, nb_directions, nb_blocks);
}

static void
ffe_mpeg4_mv_delta_init_mb(
        ffe_mv_mb_ctx *mbctx,
        MpegEncContext *s,
        int nb_directions,
        int nb_blocks)
{
    AVFrame *f = s->current_picture_ptr->f;
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV_DELTA)) != 0 )
        ffe_mv_delta_export_init_mb(mbctx, s->jctx, f, s->mb_y, s->mb_x, nb_directions, nb_blocks);
    if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MV_DELTA)) != 0 )
        ffe_mv_delta_import_init_mb(mbctx, s->jctx, f, s->mb_y, s->mb_x, nb_directions, nb_blocks);
}

//---------------------------------------------------------------------
static int16_t *
ffe_h263_pred_motion(
        ffe_mv_mb_ctx *mbctx,
        ffe_mv_mb_ctx *mbctx_delta,
        MpegEncContext *s,
        int block,
        int dir,
        int *px,
        int *py)
{
    AVFrame *f = s->current_picture_ptr->f;
    ffe_mv_select(mbctx, f, dir, block);
    ffe_mv_delta_select(mbctx_delta, f, dir, block);
    return ff_h263_pred_motion(s, block, dir, px, py);
}

//---------------------------------------------------------------------
static inline int modulo_decoding(
        MpegEncContext *s,
        int pred,
        int val,
        int f_code)
{
    /* modulo decoding */
    if (!s->h263_long_vectors) {
        val = sign_extend(val, 5 + f_code);
    } else {
        /* horrible H.263 long vector mode */
        if (pred < -31 && val < -63)
            val += 64;
        if (pred > 32 && val > 63)
            val -= 64;
    }
    return val;
}

static int
ffe_mpeg4_decode_motion(
        ffe_mv_mb_ctx *mbctx,
        ffe_mv_mb_ctx *mbctx_delta,
        MpegEncContext *s,
        int pred,
        int f_code,
        int x_or_y)     // 0 = x, 1 = y
{
    AVFrame *f = s->current_picture_ptr->f;
    int delta;
    int val;

    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_MV)) != 0
      || (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_MV_DELTA)) != 0 )
    {
        s->pb = *ffe_transplicate_save(&s->ffe_xp);
    }

    delta = ff_h263_decode_motion_delta(s, f_code);
    if ( delta == 0xffff )
        return delta;

    if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MV_DELTA)) != 0 )
        delta = ffe_mv_delta_get(mbctx_delta, f, x_or_y);

    /* NOTE maintaining FFmpeg behaviour that does not sign-extend when
     *      delta is zero. I don't know whether this is correct or not.
     */
    val = delta + pred;
    if ( delta != 0 )
        val = modulo_decoding(s, pred, val, f_code);

    if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MV)) != 0 )
        val = ffe_mv_get(mbctx, f, x_or_y);
    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_MV)) != 0
      || (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_MV_DELTA)) != 0 )
    {
        if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_MV)) != 0 )
            delta = ffe_mv_overflow(mbctx, pred, val, f_code, 6);
        else
            delta = ffe_mv_delta_overflow(mbctx_delta, delta, f_code, 6);
        ff_h263_encode_motion(&s->pb, delta, f_code);
        ffe_transplicate_restore(&s->ffe_xp, &s->pb);
        val = modulo_decoding(s, pred, delta + pred, f_code);
    }
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV)) != 0 )
        ffe_mv_set(mbctx, s->jctx, x_or_y, val);
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV_DELTA)) != 0 )
        ffe_mv_delta_set(mbctx_delta, s->jctx, x_or_y, delta);

    return val;
}

//---------------------------------------------------------------------
static void
ffe_mpeg4_mv_not_supported(MpegEncContext *s)
{
    if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MV)) != 0
      || (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV)) != 0
      || (s->avctx->ffedit_apply  & (1 << FFEDIT_FEAT_MV)) != 0
      || (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MV_DELTA)) != 0
      || (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV_DELTA)) != 0
      || (s->avctx->ffedit_apply  & (1 << FFEDIT_FEAT_MV_DELTA)) != 0 )
    {
        av_log(NULL, AV_LOG_ERROR,
               "FFedit doesn't support the motion vectors in this file.\n");
        av_assert0(0);
    }
}

//---------------------------------------------------------------------

static int mpeg4_decode_mb(MpegEncContext *s, int16_t block[6][64]);

static int
ffe_mpeg4_decode_mb(MpegEncContext *s, int16_t block[12][64])
{
    AVFrame *f = s->current_picture_ptr->f;
    FFEditTransplicateContext *xp = NULL;
    ffe_mb_mb_ctx mbctx;
    int ret;

    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_MB)) != 0 )
        xp = &s->ffe_xp;

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MB)) != 0 )
        ffe_mb_export_init_mb(&mbctx, &s->gb);
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MB)) != 0 )
        ffe_mb_import_init_mb(&mbctx, f, &s->gb, xp, s->mb_y, s->mb_x);

    ret = mpeg4_decode_mb(s, s->block);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MB)) != 0 )
        ffe_mb_export_flush_mb(&mbctx, s->jctx, f, &s->gb, s->mb_y, s->mb_x);
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MB)) != 0 )
        ffe_mb_import_flush_mb(&mbctx, f, &s->gb, xp, s->mb_y, s->mb_x);

    return ret;
}
