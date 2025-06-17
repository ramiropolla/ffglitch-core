
/* This file is included by mpeg12dec.c */

#include <stdatomic.h>

#include "ffedit_json.h"
#include "ffedit_mb.h"
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
        s->pb = *ffe_transplicate_bits_save(&s->ffe_xp);
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
        ffe_transplicate_bits_restore(&s->ffe_xp, &s->pb);
    }
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV)) != 0 )
        ffe_mv_set(mbctx, x_or_y, val);
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV_DELTA)) != 0 )
        ffe_mv_delta_set(mbctx_delta, x_or_y, delta);

    return val;
}

/*-------------------------------------------------------------------*/
/* qscale                                                            */
/*-------------------------------------------------------------------*/

/* needed stuff */

typedef struct
{
    json_t *slice;
    json_t *mb;
    atomic_size_t mb_count;
} qscale_ctx;

/* init */

static void
ffe_mpeg12_export_qscale_init(MpegEncContext *s, AVFrame *f)
{
    // {
    //  "slice": [ ] # line
    //           { }
    //  "mb":    [ ] # line
    //           [ ] # column
    //           null or qscale
    // }

    json_t *jframe = json_object_new(s->jctx);
    qscale_ctx *ctx = json_allocator_get0(s->jctx, sizeof(qscale_ctx));
    json_object_userdata_set(jframe, ctx);

    ctx->slice = json_array_new(s->jctx, s->mb_height);
    for ( size_t mb_y = 0; mb_y < s->mb_height; mb_y++ )
    {
        json_t *jcolumns = json_object_new(s->jctx);
        json_set_pflags(jcolumns, JSON_PFLAGS_NO_LF);
        json_array_set(ctx->slice, mb_y, jcolumns);
    }
    json_object_add(jframe, "slice", ctx->slice);

    ctx->mb = ffe_jblock_new(s->jctx,
                             s->mb_width, s->mb_height,
                             JSON_PFLAGS_NO_LF);
    atomic_init(&ctx->mb_count, 0);
    json_object_add(jframe, "mb", ctx->mb);

    f->ffedit_sd[FFEDIT_FEAT_QSCALE] = jframe;
}

static void
ffe_mpeg12_import_qscale_init(MpegEncContext *s, AVFrame *f)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_QSCALE];
    qscale_ctx *ctx = json_allocator_get0(s->jctx, sizeof(qscale_ctx));
    json_object_userdata_set(jframe, ctx);
    ctx->slice = json_object_get(jframe, "slice");
    ctx->mb = json_object_get(jframe, "mb");
}

/* cleanup */

static void
ffe_mpeg12_export_qscale_cleanup(MpegEncContext *s, AVFrame *f)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_QSCALE];
    qscale_ctx *ctx = json_object_userdata_get(jframe);
    for ( size_t mb_y = 0; mb_y < s->mb_height; mb_y++ )
    {
        json_t *jmb_y = json_array_get(ctx->slice, mb_y);
        json_object_done(s->jctx, jmb_y);
    }
    if ( atomic_load(&ctx->mb_count) == 0 )
        json_object_del(jframe, "mb");
    json_object_done(s->jctx, jframe);
}

/* export */

static int64_t
ffe_qscale_get_mb(MpegEncContext *s)
{
    AVFrame *f = s->current_picture_ptr->f;
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_QSCALE];
    qscale_ctx *ctx = json_object_userdata_get(jframe);
    json_t *jdata = ctx->mb;
    json_t *jmb_y = json_array_get(jdata, s->mb_y);
    return jmb_y->array_of_ints[s->mb_x];
}

static int64_t
ffe_qscale_get_slice(MpegEncContext *s, int mb_y)
{
    AVFrame *f = s->current_picture_ptr->f;
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_QSCALE];
    qscale_ctx *ctx = json_object_userdata_get(jframe);
    json_t *jdata = ctx->slice;
    json_t *jmb_y = json_array_get(jdata, mb_y);
    json_t *jint;
    char buf[10];
    snprintf(buf, sizeof(buf), "%d", s->mb_x);
    jint = json_object_get(jmb_y, buf);
    if ( jint == NULL )
    {
        av_log(NULL, AV_LOG_ERROR,
               "qscale value for slice at line %d row %d not found!\n",
               mb_y, s->mb_x);
        av_assert0(0);
    }
    return json_int_val(jint);
}

static void
ffe_qscale_set_mb(MpegEncContext *s, int val)
{
    AVFrame *f = s->current_picture_ptr->f;
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_QSCALE];
    qscale_ctx *ctx = json_object_userdata_get(jframe);
    json_t *jdata = ctx->mb;
    json_t *jmb_y = json_array_get(jdata, s->mb_y);
    json_t *jval = json_int_new(s->jctx, val);
    json_array_set(jmb_y, s->mb_x, jval);
    atomic_fetch_add(&ctx->mb_count, 1);
}

static void
ffe_qscale_set_slice(MpegEncContext *s, int mb_y, int val)
{
    // TODO not multi-thread friendly!
    AVFrame *f = s->current_picture_ptr->f;
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_QSCALE];
    qscale_ctx *ctx = json_object_userdata_get(jframe);
    json_t *jdata = ctx->slice;
    json_t *jmb_y = json_array_get(jdata, mb_y);
    json_t *jval = json_int_new(s->jctx, val);
    char buf[10];
    snprintf(buf, sizeof(buf), "%d", s->mb_x);
    json_object_add(jmb_y, buf, jval);
}

static int ffe_get_qscale(MpegEncContext *s, int mb_y, int is_mb)
{
    PutBitContext *saved;
    int orig_qscale;
    int qscale;

    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_QSCALE)) != 0 )
        saved = ffe_transplicate_bits_save(&s->ffe_xp);

    // mpeg_get_qscale
    orig_qscale = get_bits(&s->gb, 5);

    if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_QSCALE)) != 0 )
    {
        if ( is_mb )
            orig_qscale = ffe_qscale_get_mb(s);
        else
            orig_qscale = ffe_qscale_get_slice(s, mb_y);
    }
    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_QSCALE)) != 0 )
    {
        put_bits(saved, 5, orig_qscale);
        ffe_transplicate_bits_restore(&s->ffe_xp, saved);
    }
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_QSCALE)) != 0 )
    {
        if ( is_mb )
            ffe_qscale_set_mb(s, orig_qscale);
        else
            ffe_qscale_set_slice(s, mb_y, orig_qscale);
    }

    if ( s->q_scale_type )
        qscale = ff_mpeg2_non_linear_qscale[orig_qscale];
    else
        qscale = orig_qscale << 1;

    return qscale;
}

/*-------------------------------------------------------------------*/
/* dct                                                               */
/*-------------------------------------------------------------------*/

//---------------------------------------------------------------------
static enum FFEditFeature
e_which_dct_feat(MpegEncContext *s)
{
    return (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_Q_DCT))       != 0 ? FFEDIT_FEAT_Q_DCT
         : (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_Q_DCT_DELTA)) != 0 ? FFEDIT_FEAT_Q_DCT_DELTA
         : (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_Q_DC))        != 0 ? FFEDIT_FEAT_Q_DC
         : (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_Q_DC_DELTA))  != 0 ? FFEDIT_FEAT_Q_DC_DELTA
         :                                                                   FFEDIT_FEAT_LAST;
}

//---------------------------------------------------------------------
static enum FFEditFeature
i_which_dct_feat(MpegEncContext *s)
{
    return (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_Q_DCT))       != 0 ? FFEDIT_FEAT_Q_DCT
         : (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_Q_DCT_DELTA)) != 0 ? FFEDIT_FEAT_Q_DCT_DELTA
         : (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_Q_DC))        != 0 ? FFEDIT_FEAT_Q_DC
         : (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_Q_DC_DELTA))  != 0 ? FFEDIT_FEAT_Q_DC_DELTA
         :                                                                   FFEDIT_FEAT_LAST;
}

//---------------------------------------------------------------------
static enum FFEditFeature
a_which_dct_feat(MpegEncContext *s)
{
    return (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_Q_DCT))       != 0 ? FFEDIT_FEAT_Q_DCT
         : (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_Q_DCT_DELTA)) != 0 ? FFEDIT_FEAT_Q_DCT_DELTA
         : (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_Q_DC))        != 0 ? FFEDIT_FEAT_Q_DC
         : (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_Q_DC_DELTA))  != 0 ? FFEDIT_FEAT_Q_DC_DELTA
         :                                                                  FFEDIT_FEAT_LAST;
}

static void
ffe_mpeg12_export_dct_init(MpegEncContext *s, AVFrame *f)
{
    enum FFEditFeature e_dct_feat = e_which_dct_feat(s);
    json_t *jframe;
    int nb_components = 3;
    int h_count[3] = { 2, 1, 1 };
    int v_count[3] = { 2, 1, 1 };
    int pflags = 0;

    if ( !s->chroma_y_shift )
        v_count[1] = v_count[2] = 2;

    if ( e_dct_feat == FFEDIT_FEAT_Q_DC
      || e_dct_feat == FFEDIT_FEAT_Q_DC_DELTA )
    {
        pflags = JSON_PFLAGS_NO_LF;
    }

    jframe = ffe_jmb_new(s->jctx,
                         s->mb_width, s->mb_height,
                         nb_components,
                         v_count, h_count,
                         NULL,
                         pflags);

    f->ffedit_sd[e_dct_feat] = jframe;
}

static void
ffe_mpeg12_import_dct_init(MpegEncContext *s, AVFrame *f)
{
    enum FFEditFeature i_dct_feat = i_which_dct_feat(s);
    json_t *jframe = f->ffedit_sd[i_dct_feat];
    int nb_components = 3;
    int h_count[3] = { 2, 1, 1 };
    int v_count[3] = { 2, 1, 1 };

    if ( !s->chroma_y_shift )
        v_count[1] = v_count[2] = 2;

    ffe_jmb_set_context(jframe, nb_components, v_count, h_count);
}

typedef struct {
    int16_t qblock[64];
    int last_dc[3];
} ffe_mpeg12_block;

static void ffe_mpeg12_init_block(
        MpegEncContext *s,
        ffe_mpeg12_block *ctx)
{
    memset(ctx->qblock, 0x00, sizeof(ctx->qblock));
    memcpy(ctx->last_dc, s->last_dc, sizeof(ctx->last_dc));
    if ( a_which_dct_feat(s) != FFEDIT_FEAT_LAST )
        s->pb = *ffe_transplicate_bits_save(&s->ffe_xp);
}

static void ffe_mpeg12_use_block(
        MpegEncContext *s,
        ffe_mpeg12_block *ctx,
        int i,
        int is_intra)
{
    enum FFEditFeature e_dct_feat = e_which_dct_feat(s);
    enum FFEditFeature i_dct_feat = i_which_dct_feat(s);
    enum FFEditFeature a_dct_feat = a_which_dct_feat(s);

    AVFrame *f = s->current_picture_ptr->f;
    int component = (i < 4) ? 0 : (1 + ((i - 4) & 1));
    int blockn = (i < 4) ? i : ((i - 4) >> 1);
    int j;

    if ( e_dct_feat != FFEDIT_FEAT_LAST )
    {
        json_t *jframe = f->ffedit_sd[e_dct_feat];
        json_t *jso;

        if ( is_intra
          && (e_dct_feat == FFEDIT_FEAT_Q_DCT_DELTA
           || e_dct_feat == FFEDIT_FEAT_Q_DC_DELTA) )
        {
            ctx->qblock[0] -= ctx->last_dc[component];
        }

        if ( e_dct_feat == FFEDIT_FEAT_Q_DC
          || e_dct_feat == FFEDIT_FEAT_Q_DC_DELTA )
        {
            jso = json_int_new(s->jctx, ctx->qblock[0]);
        }
        else
        {
            jso = json_array_of_ints_new(s->jctx, 64);
            json_set_pflags(jso, JSON_PFLAGS_NO_LF);
            for ( j = 0; j < 64; j++ )
            {
                int k = s->intra_scantable.permutated[j];
                jso->array_of_ints[j] = ctx->qblock[k];
            }
        }

        ffe_jmb_set(jframe, component, s->mb_y, s->mb_x, blockn, jso);
    }
    else if ( i_dct_feat != FFEDIT_FEAT_LAST )
    {
        json_t *jframe = f->ffedit_sd[i_dct_feat];
        if ( i_dct_feat == FFEDIT_FEAT_Q_DC
          || i_dct_feat == FFEDIT_FEAT_Q_DC_DELTA )
        {
            int val = ffe_jmb_int_get(jframe, component, s->mb_y, s->mb_x, blockn);
            ctx->qblock[0] = val;
        }
        else
        {
            json_t *jso = ffe_jmb_get(jframe, component, s->mb_y, s->mb_x, blockn);
            for ( j = 0; j < 64; j++ )
            {
                int k = s->intra_scantable.permutated[j];
                ctx->qblock[k] = jso->array_of_ints[j];
            }
        }
        if ( is_intra
          && (i_dct_feat == FFEDIT_FEAT_Q_DCT_DELTA
           || i_dct_feat == FFEDIT_FEAT_Q_DC_DELTA) )
        {
            ctx->qblock[0] += ctx->last_dc[component];
        }
        for ( j = 63; j > 0; j-- )
            if ( ctx->qblock[s->intra_scantable.permutated[j]] )
                break;
        s->block_last_index[i] = j;

        // Unquantize ctx->qblock into s->block[i]
        memcpy(s->block[i], ctx->qblock, sizeof(s->block[i]));
        if ( s->mb_intra )
            s->dct_unquantize_intra(s, s->block[i], i, s->qscale >> 1);
        else
            s->dct_unquantize_inter(s, s->block[i], i, s->qscale >> 1);
        if ( s->codec_id == AV_CODEC_ID_MPEG2VIDEO )
        {
            int mismatch = 1;
            for ( j = 0; j < 64; j++ )
                mismatch ^= s->block[i][j];
            s->block[i][63] ^= (mismatch & 1);
        }
    }

    if ( a_dct_feat != FFEDIT_FEAT_LAST )
    {
        memcpy(s->last_dc, ctx->last_dc, sizeof(s->last_dc));
        ffe_mpeg1_encode_block(s, ctx->qblock, i);
        ffe_transplicate_bits_restore(&s->ffe_xp, &s->pb);
    }
}

static inline int
mpeg1_decode_block_inter(
        MpegEncContext *s,
        int16_t *block,
        int16_t *qblock,
        int n);

static int
ffe_mpeg1_decode_block_inter(
        MpegEncContext *s,
        int16_t *block,
        int n)
{
    int ret;
    ffe_mpeg12_block bctx;
    ffe_mpeg12_init_block(s, &bctx);
    ret = mpeg1_decode_block_inter(s, block, bctx.qblock, n);
    if (ret >= 0)
        ffe_mpeg12_use_block(s, &bctx, n, 0);
    return ret;
}

static int
ffe_mpeg1_decode_block_intra(
        MpegEncContext *s,
        int16_t *block,
        int n)
{
    int ret;
    ffe_mpeg12_block bctx;
    ffe_mpeg12_init_block(s, &bctx);
    ret = ff_mpeg1_decode_block_intra(&s->gb,
                                      s->intra_matrix,
                                      s->intra_scantable.permutated,
                                      s->last_dc, block,
                                      bctx.qblock,
                                      n, s->qscale);
    if (ret >= 0)
        ffe_mpeg12_use_block(s, &bctx, n, 1);
    return ret;
}

static inline int
mpeg2_decode_block_non_intra(
        MpegEncContext *s,
        int16_t *block,
        int16_t *qblock,
        int n);

static int
ffe_mpeg2_decode_block_non_intra(
        MpegEncContext *s,
        int16_t *block,
        int n)
{
    int ret;
    ffe_mpeg12_block bctx;
    ffe_mpeg12_init_block(s, &bctx);
    ret = mpeg2_decode_block_non_intra(s, block, bctx.qblock, n);
    if (ret >= 0)
        ffe_mpeg12_use_block(s, &bctx, n, 0);
    return ret;
}

static inline int
mpeg2_decode_block_intra(
        MpegEncContext *s,
        int16_t *block,
        int16_t *qblock,
        int n);

static int
ffe_mpeg2_decode_block_intra(
        MpegEncContext *s,
        int16_t *block,
        int n)
{
    int ret;
    ffe_mpeg12_block bctx;
    ffe_mpeg12_init_block(s, &bctx);
    ret = mpeg2_decode_block_intra(s, block, bctx.qblock, n);
    if (ret >= 0)
        ffe_mpeg12_use_block(s, &bctx, n, 1);
    return ret;
}

//---------------------------------------------------------------------
// mb

static int mpeg_decode_mb(MpegEncContext *s, int16_t block[12][64]);

static int
ffe_mpeg_decode_mb(MpegEncContext *s, int16_t block[12][64])
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

    ret = mpeg_decode_mb(s, s->block);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MB)) != 0 )
        ffe_mb_export_flush_mb(&mbctx, s->jctx, f, &s->gb, s->mb_y, s->mb_x);
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MB)) != 0 )
        ffe_mb_import_flush_mb(&mbctx, f, &s->gb, xp, s->mb_y, s->mb_x);

    return ret;
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

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_QSCALE)) != 0 )
        ffe_mpeg12_export_qscale_init(s, f);
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_QSCALE)) != 0 )
        ffe_mpeg12_import_qscale_init(s, f);

    if ( e_which_dct_feat(s) != FFEDIT_FEAT_LAST )
        ffe_mpeg12_export_dct_init(s, f);
    else if ( i_which_dct_feat(s) != FFEDIT_FEAT_LAST )
        ffe_mpeg12_import_dct_init(s, f);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MB)) != 0 )
        ffe_mb_export_init(s->jctx, f, s->mb_height, s->mb_width);
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MB)) != 0 )
        ffe_mb_import_init(s->jctx, f);
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

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_QSCALE)) != 0 )
        ffe_mpeg12_export_qscale_cleanup(s, f);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MB)) != 0 )
        ffe_mb_export_cleanup(s->jctx, f);
}
