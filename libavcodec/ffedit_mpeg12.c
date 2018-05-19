
/* This file is included by mpeg12dec.c */

#include "ffedit_json.h"
#include "ffedit_mv.h"

#include "ffedit_mpegvideo.c"

//---------------------------------------------------------------------
// info
//
// {
//   "pict_type": "",
//   "mb_type": [
//     [ "", "", ..., "" ],
//     [ "", "", ..., "" ],
//     ...                ,
//     [ "", "", ..., "" ]
//   ]
// }
//
// - pict_type:
//   - "I"
//   - "P"
//   - "B"
//   - "S"
// - mb_type is a string made up of:
//   - "I" (intra)
//   - "c" (has cbp)
//   - "q" (quant)
//   - "F" (16x16)
//   - "f" (has forward mv)
//   - "b" (has backward mv)
//   - "0" (zero_mv)

/* init */

static void
ffe_mpeg12_export_info_init(MpegEncContext *s, AVFrame *f)
{
    json_t *jframe = json_object_new(s->jctx);
    json_t *jso;
    const char *pict_type = s->pict_type == AV_PICTURE_TYPE_I ? "I"
                          : s->pict_type == AV_PICTURE_TYPE_P ? "P"
                          :                                     "B";
    int is_interlaced = (s->picture_structure != PICT_FRAME);

    jso = json_string_new(s->jctx, pict_type);
    json_object_add(jframe, "pict_type", jso);

    jso = json_bool_new(s->jctx, is_interlaced);
    json_object_add(jframe, "interlaced", jso);

    if ( is_interlaced )
    {
        const char *field = (s->picture_structure == PICT_TOP_FIELD)
                          ? "top"
                          : "bottom";
        jso = json_string_new(s->jctx, field);
        json_object_add(jframe, "field", jso);
    }

    jso = ffe_jblock_new(s->jctx,
                         s->mb_width, s->mb_height,
                         JSON_PFLAGS_NO_LF);
    json_object_add(jframe, "mb_type", jso);

    f->ffedit_sd[FFEDIT_FEAT_INFO] = jframe;
}

/* export */

static void
ffe_mpeg12_export_info(MpegEncContext *s, int ffe_mb_type)
{
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_INFO)) != 0 )
    {
        AVFrame *f = s->current_picture_ptr->f;
        json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_INFO];
        json_t *jmb_type = json_object_get(jframe, "mb_type");
        json_t *jso;
        char buf[10];
        char *ptr = buf;

        *ptr++ = (ffe_mb_type & FFE_MB_TYPE_INTRA)    ? 'I' : ' ';
        *ptr++ = (ffe_mb_type & FFE_MB_TYPE_QUANT)    ? 'q' : ' ';
        *ptr++ = (ffe_mb_type & FFE_MB_TYPE_CBP)      ? 'c' : ' ';
        *ptr++ = (ffe_mb_type & FFE_MB_TYPE_FORWARD)  ? 'f' : ' ';
        *ptr++ = (ffe_mb_type & FFE_MB_TYPE_BACKWARD) ? 'b' : ' ';
        *ptr = '\0';

        jso = json_string_new(s->jctx, buf);
        ffe_jblock_set(jmb_type, s->mb_y, s->mb_x, jso);
    }
}

/* cleanup */

static void
ffe_mpeg12_export_info_cleanup(MpegEncContext *s, AVFrame *f)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_INFO];
    json_object_done(s->jctx, jframe);
}

//---------------------------------------------------------------------
// mv
//
// {
//   "forward": [
//     [ mv, mv, ..., mv ],
//     [ mv, mv, ..., mv ],
//     ...                ,
//     [ mv, mv, ..., mv ]
//   ],
//   "backward": [
//     [ mv, mv, ..., mv ],
//     [ mv, mv, ..., mv ],
//     ...                ,
//     [ mv, mv, ..., mv ]
//   ],
//   "fcode": [ ],
//   "bcode": [ ],
//   "overflow": "assert", "truncate", or "ignore"
// }
//
// - mv:
//   - null
//   - array_of_ints [ mv_x, mv_y ]
//
// note: either "forward", "backward", or both may be missing,
//       depending on the frame type.

/* forward declarations */

static int mpeg_decode_motion_delta(MpegEncContext *s, int fcode);

/* init */

static void
ffe_mpeg12_export_mv_init(MpegEncContext *s, AVFrame *f)
{
    int nb_fcodes = (s->codec_id == AV_CODEC_ID_MPEG2VIDEO) ? 2 : 1;
    ffe_mv_export_init(s->jctx, f, s->mb_height, s->mb_width, nb_fcodes, 2);
    ffe_mv_export_fcode(s->jctx, f, 0, 0, s->mpeg_f_code[0][0]);
    ffe_mv_export_fcode(s->jctx, f, 1, 0, s->mpeg_f_code[1][0]);
    if ( s->codec_id == AV_CODEC_ID_MPEG2VIDEO )
    {
        ffe_mv_export_fcode(s->jctx, f, 0, 1, s->mpeg_f_code[0][1]);
        ffe_mv_export_fcode(s->jctx, f, 1, 1, s->mpeg_f_code[1][1]);
    }
}

static void
ffe_mpeg12_export_mv_delta_init(MpegEncContext *s, AVFrame *f)
{
    int nb_fcodes = (s->codec_id == AV_CODEC_ID_MPEG2VIDEO) ? 2 : 1;
    ffe_mv_delta_export_init(s->jctx, f, s->mb_height, s->mb_width, nb_fcodes, 2);
    ffe_mv_delta_export_fcode(s->jctx, f, 0, 0, s->mpeg_f_code[0][0]);
    ffe_mv_delta_export_fcode(s->jctx, f, 1, 0, s->mpeg_f_code[1][0]);
    if ( s->codec_id == AV_CODEC_ID_MPEG2VIDEO )
    {
        ffe_mv_delta_export_fcode(s->jctx, f, 0, 1, s->mpeg_f_code[0][1]);
        ffe_mv_delta_export_fcode(s->jctx, f, 1, 1, s->mpeg_f_code[1][1]);
    }
}

static void
ffe_mpeg12_import_mv_init(MpegEncContext *s, AVFrame *f)
{
    ffe_mv_import_init(s->jctx, f);
}

static void
ffe_mpeg12_import_mv_delta_init(MpegEncContext *s, AVFrame *f)
{
    ffe_mv_delta_import_init(s->jctx, f);
}

/* export */

static void ffe_mpeg12_mv_init_mb(
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

static void ffe_mpeg12_mv_delta_init_mb(
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

/* select */
static void ffe_mpeg12_mv_select(
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

static void ffe_mpeg12_mv_delta_select(
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
static inline int modulo_decoding(int val, int fcode)
{
    int shift = fcode - 1;
    /* modulo decoding */
    val = sign_extend(val, 5 + shift);
    return val;
}

static int ffe_decode_mpegmv(
        ffe_mv_mb_ctx *mbctx,
        ffe_mv_mb_ctx *mbctx_delta,
        MpegEncContext *s,
        int fcode,
        int pred,
        int x_or_y)     // 0 = x, 1 = y
{
    int delta;
    int val;

    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_MV)) != 0
      || (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_MV_DELTA)) != 0 )
    {
        s->pb = *ffe_transplicate_save(&s->ffe_xp);
    }

    delta = mpeg_decode_motion_delta(s, fcode);
    if ( delta == 0xffff )
        return delta;

    if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MV_DELTA)) != 0 )
    {
        delta = ffe_mv_delta_get(mbctx_delta, x_or_y);
        delta = ffe_mv_delta_overflow(mbctx_delta, delta, fcode, 5);
    }

    /* NOTE maintaining FFmpeg behaviour that does not sign-extend when
     *      delta is zero. I don't know whether this is correct or not.
     */
    val = delta + pred;
    if ( delta != 0 )
        val = modulo_decoding(val, fcode);

    if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MV)) != 0 )
    {
        val = ffe_mv_get(mbctx, x_or_y);
        delta = ffe_mv_overflow(mbctx, pred, val, fcode, 5);
        val = modulo_decoding(delta + pred, fcode);
    }
    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_MV)) != 0
      || (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_MV_DELTA)) != 0 )
    {
        ff_mpeg1_encode_motion(s, delta, fcode);
        ffe_transplicate_restore(&s->ffe_xp, &s->pb);
    }
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV)) != 0 )
        ffe_mv_set(mbctx, x_or_y, val);
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV_DELTA)) != 0 )
        ffe_mv_delta_set(mbctx_delta, x_or_y, delta);

    return val;
}

/*-------------------------------------------------------------------*/
/* common                                                            */
/*-------------------------------------------------------------------*/

static void
ffe_mpeg12_prepare_frame(AVCodecContext *avctx, MpegEncContext *s, AVPacket *avpkt)
{
    if ( s->avctx->ffedit_import != 0 )
    {
        memcpy(s->ffedit_sd, avpkt->ffedit_sd, sizeof(json_t *)*FFEDIT_FEAT_LAST);
        s->jctx = avpkt->jctx;
    }
}

static void
ffe_mpeg12_init(MpegEncContext *s)
{
    AVFrame *f = s->current_picture_ptr->f;

    ffe_mpegvideo_jctx_init(s);
    if ( s->avctx->ffedit_import != 0 )
        memcpy(f->ffedit_sd, s->ffedit_sd, sizeof(json_t *)*FFEDIT_FEAT_LAST);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_INFO)) != 0 )
        ffe_mpeg12_export_info_init(s, f);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV)) != 0 )
        ffe_mpeg12_export_mv_init(s, f);
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MV)) != 0 )
        ffe_mpeg12_import_mv_init(s, f);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV_DELTA)) != 0 )
        ffe_mpeg12_export_mv_delta_init(s, f);
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MV_DELTA)) != 0 )
        ffe_mpeg12_import_mv_delta_init(s, f);
}

static void
ffe_mpeg12_export_cleanup(MpegEncContext *s, AVFrame *f)
{
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_INFO)) != 0 )
        ffe_mpeg12_export_info_cleanup(s, f);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV)) != 0 )
        ffe_mv_export_cleanup(s->jctx, f);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV_DELTA)) != 0 )
        ffe_mv_delta_export_cleanup(s->jctx, f);
}
