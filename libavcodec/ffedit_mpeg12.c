
/* This file is included by mpeg12dec.c */

#include "ffedit_json.h"
#include "ffedit_mv.h"

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
    json_t *jframe = json_object_new(f->jctx);
    json_t *jso;
    int one = 1;

    jso = json_string_new(f->jctx,
        s->pict_type == AV_PICTURE_TYPE_I ? "I" :
       (s->pict_type == AV_PICTURE_TYPE_P ? "P" :
       (s->pict_type == AV_PICTURE_TYPE_B ? "B" : "S")));

    json_object_add(jframe, "pict_type", jso);

    jso = ffe_jmb_new(f->jctx,
                      s->mb_width, s->mb_height,
                      one, &one, &one,
                      JSON_PFLAGS_NO_LF);

    json_object_add(jframe, "mb_type", jso);

    f->ffedit_sd[FFEDIT_FEAT_INFO] = jframe;
}

/* export */

static void
ffe_mpeg12_export_info(MpegEncContext *s, int mb_type)
{
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_INFO)) != 0 )
    {
        AVFrame *f = s->current_picture_ptr->f;
        json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_INFO];
        json_t *jmb_type = json_object_get(jframe, "mb_type");
        json_t *jso;
        char buf[10];
        char *ptr = buf;

        *ptr++ = (mb_type & MB_TYPE_INTRA)   ? 'I' : ' ';
        *ptr++ = (mb_type & MB_TYPE_CBP)     ? 'c' : ' ';
        *ptr++ = (mb_type & MB_TYPE_QUANT)   ? 'q' : ' ';
        *ptr++ = (mb_type & MB_TYPE_16x16)   ? 'F' : ' ';
        *ptr++ = (mb_type & MB_TYPE_L0)      ? 'f' : ' ';
        *ptr++ = (mb_type & MB_TYPE_L1)      ? 'b' : ' ';
        *ptr++ = (mb_type & MB_TYPE_ZERO_MV) ? '0' : ' ';
        *ptr = '\0';

        jso = json_string_new(f->jctx, buf);
        ffe_jmb_set(jmb_type, 0, s->mb_y, s->mb_x, 0, jso);
    }
}

/* cleanup */

static void
ffe_mpeg12_export_info_cleanup(MpegEncContext *s, AVFrame *f)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_INFO];
    json_object_done(f->jctx, jframe);
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
//   ]
// }
//
// - mv:
//   - null
//   - array_of_ints [ mv_x, mv_y ]
//
// note: either "forward", "backward", or both may be missing,
//       depending on the frame type.

/* forward declarations */

static int mpeg_decode_motion(MpegEncContext *s, int fcode, int pred);

/* init */

static void
ffe_mpeg12_export_mv_init(MpegEncContext *s, AVFrame *f)
{
    ffe_mv_export_init(f->jctx, f, s->mb_height, s->mb_width);
}

static void
ffe_mpeg12_import_mv_init(MpegEncContext *s, AVFrame *f)
{
    ffe_mv_import_init(f->jctx, f);
}

/* export */

static void ffe_mpeg12_mv_init_mb(
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

static int ffe_decode_mpegmv(
        MpegEncContext *s,
        int fcode,
        int pred,
        int x_or_y)     // 0 = x, 1 = y
{
    AVFrame *f = s->current_picture_ptr->f;
    int code;

    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_MV)) != 0 )
        s->pb = *ffe_transplicate_save(&s->ffe_xp);

    code = mpeg_decode_motion(s, fcode, pred);
    if ( code == 0xffff )
        return code;

    if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MV)) != 0 )
        code = ffe_mv_get(f, x_or_y);
    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_MV)) != 0 )
    {
        ff_mpeg1_encode_motion(s, code - pred, fcode);
        ffe_transplicate_restore(&s->ffe_xp, &s->pb);
    }
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV)) != 0 )
        ffe_mv_set(f, x_or_y, code);

    return code;
}

/*-------------------------------------------------------------------*/
/* qscale                                                            */
/*-------------------------------------------------------------------*/

/* needed stuff */

typedef struct
{
    json_t *slice;
    size_t slice_count;
    json_t *mb;
    size_t mb_count;
} qscale_ctx;

/* init */

static void
ffe_mpeg12_export_qscale_init(MpegEncContext *s, AVFrame *f)
{
    // {
    //  "slice": [ ]
    //  "mb":    [ ] # line
    //           [ ] # column
    //           null or qscale
    // }

    json_t *jframe = json_object_new(f->jctx);
    qscale_ctx *ctx = json_allocator_get0(f->jctx, sizeof(qscale_ctx));
    json_userdata_set(jframe, ctx);

    ctx->slice = json_array_new(f->jctx);
    json_set_pflags(ctx->slice, JSON_PFLAGS_NO_LF);
    json_object_add(jframe, "slice", ctx->slice);

    ctx->mb = ffe_jblock_new(f->jctx,
                             s->mb_width, s->mb_height,
                             JSON_PFLAGS_NO_LF);
    json_object_add(jframe, "mb", ctx->mb);

    f->ffedit_sd[FFEDIT_FEAT_QSCALE] = jframe;
}

static void
ffe_mpeg12_import_qscale_init(MpegEncContext *s, AVFrame *f)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_QSCALE];
    qscale_ctx *ctx = json_allocator_get0(f->jctx, sizeof(qscale_ctx));
    json_userdata_set(jframe, ctx);
    ctx->slice = json_object_get(jframe, "slice");
    ctx->mb = json_object_get(jframe, "mb");
}

/* cleanup */

static void
ffe_mpeg12_export_qscale_cleanup(MpegEncContext *s, AVFrame *f)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_QSCALE];
    qscale_ctx *ctx = json_userdata_get(jframe);
    if ( ctx->mb_count == 0 )
        json_object_del(jframe, "mb");
    json_object_done(f->jctx, jframe);
}

/* export */

static int64_t
ffe_qscale_get_mb(MpegEncContext *s)
{
    AVFrame *f = s->current_picture_ptr->f;
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_QSCALE];
    qscale_ctx *ctx = json_userdata_get(jframe);
    json_t *jdata = ctx->mb;
    json_t *jmb_y = json_array_get(jdata, s->mb_y);
    return json_array_get_int(jmb_y, s->mb_x);
}

static int64_t
ffe_qscale_get_slice(MpegEncContext *s)
{
    AVFrame *f = s->current_picture_ptr->f;
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_QSCALE];
    qscale_ctx *ctx = json_userdata_get(jframe);
    // TODO FIXME
    json_t *jdata = ctx->slice;
    return json_array_get_int(jdata, 0);
}

static void
ffe_qscale_set_mb(MpegEncContext *s, int val)
{
    AVFrame *f = s->current_picture_ptr->f;
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_QSCALE];
    qscale_ctx *ctx = json_userdata_get(jframe);
    json_t *jdata = ctx->mb;
    json_t *jmb_y = json_array_get(jdata, s->mb_y);
    json_t *jval = json_int_new(f->jctx, val);
    json_array_set(jmb_y, s->mb_x, jval);
    ctx->mb_count++;
}

static void
ffe_qscale_set_slice(MpegEncContext *s, int val)
{
    AVFrame *f = s->current_picture_ptr->f;
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_QSCALE];
    qscale_ctx *ctx = json_userdata_get(jframe);
    json_t *jdata = ctx->slice;
    json_t *jval = json_int_new(f->jctx, val);
    json_array_add(jdata, jval);
    ctx->slice_count++;
}

static int ffe_get_qscale(MpegEncContext *s, int is_mb)
{
    PutBitContext *saved;
    int qscale;

    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_QSCALE)) != 0 )
        saved = ffe_transplicate_save(&s->ffe_xp);

    qscale = mpeg_get_qscale(s);

    if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_QSCALE)) != 0 )
    {
        if ( is_mb )
            qscale = ffe_qscale_get_mb(s) << 1;
        else
            qscale = ffe_qscale_get_slice(s) << 1;
    }
    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_QSCALE)) != 0 )
    {
        put_bits(saved, 5, qscale >> 1);
        ffe_transplicate_restore(&s->ffe_xp, saved);
    }
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_QSCALE)) != 0 )
    {
        if ( is_mb )
            ffe_qscale_set_mb(s, qscale >> 1);
        else
            ffe_qscale_set_slice(s, qscale >> 1);
    }

    return qscale;
}

/*-------------------------------------------------------------------*/
/* common                                                            */
/*-------------------------------------------------------------------*/

static void
ffe_mpeg12_prepare_frame(MpegEncContext *s, AVPacket *avpkt)
{
    memcpy(s->ffedit_sd, avpkt->ffedit_sd, sizeof(s->ffedit_sd));
    s->jctx = avpkt->jctx;
}

static void
ffe_mpeg12_init(MpegEncContext *s)
{
    AVFrame *f = s->current_picture_ptr->f;
    memcpy(f->ffedit_sd, s->ffedit_sd, sizeof(f->ffedit_sd));
    f->jctx = s->jctx;

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_INFO)) != 0 )
        ffe_mpeg12_export_info_init(s, f);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV)) != 0 )
        ffe_mpeg12_export_mv_init(s, f);
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MV)) != 0 )
        ffe_mpeg12_import_mv_init(s, f);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_QSCALE)) != 0 )
        ffe_mpeg12_export_qscale_init(s, f);
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_QSCALE)) != 0 )
        ffe_mpeg12_import_qscale_init(s, f);
}

static void
ffe_mpeg12_export_cleanup(MpegEncContext *s, AVFrame *f)
{
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_INFO)) != 0 )
        ffe_mpeg12_export_info_cleanup(s, f);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV)) != 0 )
        ffe_mv_export_cleanup(f->jctx, f);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_QSCALE)) != 0 )
        ffe_mpeg12_export_qscale_cleanup(s, f);
}
