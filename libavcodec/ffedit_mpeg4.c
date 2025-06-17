
/* This file is included by mpeg4videodec.c */

#include "ffedit_json.h"
#include "ffedit_mb.h"
#include "ffedit_mv.h"

//---------------------------------------------------------------------
// info

//---------------------------------------------------------------------
static void
ffe_mpeg4_export_info(MpegEncContext *s, int ffe_mb_type, int ffe_mb_cbp)
{
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_INFO)) != 0 )
    {
        AVFrame *f = s->current_picture_ptr->f;
        json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_INFO];
        json_t *jmb_type = json_object_get(jframe, "mb_type");
        json_t *jso;
        char buf[32];
        char *ptr = buf;

        if ( ffe_mb_type == -1 )
        {
            jso = json_null_new(s->jctx);
        }
        else
        {
            *ptr++ = (ffe_mb_type & FFE_MB_TYPE_INTRA)      ? 'I' : ' ';
            *ptr++ = (ffe_mb_type & FFE_MB_TYPE_ACPRED)     ? 'a' : ' ';
            *ptr++ = (ffe_mb_type & FFE_MB_TYPE_QUANT)      ? 'q' : ' ';
            *ptr++ = (ffe_mb_type & FFE_MB_TYPE_FORWARD)    ? 'f' : ' ';
            *ptr++ = (ffe_mb_type & FFE_MB_TYPE_BACKWARD)   ? 'b' : ' ';
            *ptr++ = (ffe_mb_type & FFE_MB_TYPE_DIRECT)     ? 'd' : ' ';
            *ptr++ = (ffe_mb_type & FFE_MB_TYPE_GMC)        ? 'G' : ' ';
            *ptr++ = (ffe_mb_type & FFE_MB_TYPE_MV4)        ? '4' : ' ';
            *ptr++ = (ffe_mb_type & FFE_MB_TYPE_INTERLACED) ? 'i' : ' ';
            *ptr++ = (ffe_mb_cbp & 0x20)                    ? '1' : ' ';
            *ptr++ = (ffe_mb_cbp & 0x10)                    ? '2' : ' ';
            *ptr++ = (ffe_mb_cbp & 0x08)                    ? '3' : ' ';
            *ptr++ = (ffe_mb_cbp & 0x04)                    ? '4' : ' ';
            *ptr++ = (ffe_mb_cbp & 0x02)                    ? '5' : ' ';
            *ptr++ = (ffe_mb_cbp & 0x01)                    ? '6' : ' ';
            *ptr = '\0';

            jso = json_string_new(s->jctx, buf);
        }
        ffe_jblock_set(jmb_type, s->mb_y, s->mb_x, jso);
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
        ffe_mv_export_init_mb(mbctx, f, s->mb_y, s->mb_x, nb_directions, nb_blocks);
    if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MV)) != 0 )
        ffe_mv_import_init_mb(mbctx, f, s->mb_y, s->mb_x, nb_directions, nb_blocks);
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
        ffe_mv_delta_export_init_mb(mbctx, f, s->mb_y, s->mb_x, nb_directions, nb_blocks);
    if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MV_DELTA)) != 0 )
        ffe_mv_delta_import_init_mb(mbctx, f, s->mb_y, s->mb_x, nb_directions, nb_blocks);
}

//---------------------------------------------------------------------
/* select */
static void ffe_mpeg4_mv_select(
        ffe_mv_mb_ctx *mbctx,
        MpegEncContext *s,
        int direction,
        int blockn)
{
    AVFrame *f = s->current_picture_ptr->f;
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV)) != 0 )
        ffe_mv_export_select(mbctx, f, direction, blockn);
    if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MV)) != 0 )
        ffe_mv_import_select(mbctx, f, direction, blockn);
}

static void ffe_mpeg4_mv_delta_select(
        ffe_mv_mb_ctx *mbctx,
        MpegEncContext *s,
        int direction,
        int blockn)
{
    AVFrame *f = s->current_picture_ptr->f;
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV_DELTA)) != 0 )
        ffe_mv_delta_export_select(mbctx, f, direction, blockn);
    if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MV_DELTA)) != 0 )
        ffe_mv_delta_import_select(mbctx, f, direction, blockn);
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
    ffe_mpeg4_mv_select(mbctx, s, dir, block);
    ffe_mpeg4_mv_delta_select(mbctx_delta, s, dir, block);
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
    int delta;
    int val;

    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_MV)) != 0
      || (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_MV_DELTA)) != 0 )
    {
        s->pb = *ffe_transplicate_bits_save(&s->ffe_xp);
    }

    delta = ff_h263_decode_motion_delta(s, f_code);
    if ( delta == 0xffff )
        return delta;

    if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MV_DELTA)) != 0 )
    {
        delta = ffe_mv_delta_get(mbctx_delta, x_or_y);
        delta = ffe_mv_delta_overflow(mbctx_delta, delta, f_code, 6);
    }

    /* NOTE maintaining FFmpeg behaviour that does not sign-extend when
     *      delta is zero. I don't know whether this is correct or not.
     */
    val = delta + pred;
    if ( delta != 0 )
        val = modulo_decoding(s, pred, val, f_code);

    if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MV)) != 0 )
    {
        val = ffe_mv_get(mbctx, x_or_y);
        delta = ffe_mv_overflow(mbctx, pred, val, f_code, 6);
        val = modulo_decoding(s, pred, delta + pred, f_code);
    }
    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_MV)) != 0
      || (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_MV_DELTA)) != 0 )
    {
        ff_h263_encode_motion(&s->pb, delta, f_code);
        ffe_transplicate_bits_restore(&s->ffe_xp, &s->pb);
    }
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV)) != 0 )
        ffe_mv_set(mbctx, x_or_y, val);
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV_DELTA)) != 0 )
        ffe_mv_delta_set(mbctx_delta, x_or_y, delta);

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
    FFEditTransplicateBitsContext *xp = NULL;
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
