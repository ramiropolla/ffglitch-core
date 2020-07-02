
/* This file is included by mpeg12dec.c */

#include <stdatomic.h>

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
    json_t *jframe = json_object_new(s->jctx);
    json_t *jso;
    int one = 1;

    jso = json_string_new(s->jctx,
        s->pict_type == AV_PICTURE_TYPE_I ? "I" :
       (s->pict_type == AV_PICTURE_TYPE_P ? "P" :
       (s->pict_type == AV_PICTURE_TYPE_B ? "B" : "S")));

    json_object_add(jframe, "pict_type", jso);

    jso = ffe_jmb_new(s->jctx,
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

        jso = json_string_new(s->jctx, buf);
        ffe_jmb_set(jmb_type, 0, s->mb_y, s->mb_x, 0, jso);
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
    ffe_mv_export_init(s->jctx, f, s->mb_height, s->mb_width);
}

static void
ffe_mpeg12_import_mv_init(MpegEncContext *s, AVFrame *f)
{
    ffe_mv_import_init(s->jctx, f);
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
        ffe_mv_export_init_mb(mbctx, s->jctx, f, s->mb_y, s->mb_x, nb_directions, nb_blocks);
    if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MV)) != 0 )
        ffe_mv_import_init_mb(mbctx, s->jctx, f, s->mb_y, s->mb_x, nb_directions, nb_blocks);
}

//---------------------------------------------------------------------
static int ffe_decode_mpegmv(
        ffe_mv_mb_ctx *mbctx,
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
        code = ffe_mv_get(mbctx, f, x_or_y);
    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_MV)) != 0 )
    {
        ff_mpeg1_encode_motion(s, code - pred, fcode);
        ffe_transplicate_restore(&s->ffe_xp, &s->pb);
    }
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV)) != 0 )
        ffe_mv_set(mbctx, s->jctx, x_or_y, code);

    return code;
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
    json_userdata_set(jframe, ctx);

    ctx->slice = json_array_new(s->jctx);
    json_array_alloc(s->jctx, ctx->slice, s->mb_height);
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
    qscale_ctx *ctx = json_userdata_get(jframe);
    json_t *jdata = ctx->mb;
    json_t *jmb_y = json_array_get(jdata, s->mb_y);
    return json_array_get_int(jmb_y, s->mb_x);
}

static int64_t
ffe_qscale_get_slice(MpegEncContext *s, int mb_y)
{
    AVFrame *f = s->current_picture_ptr->f;
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_QSCALE];
    qscale_ctx *ctx = json_userdata_get(jframe);
    json_t *jdata = ctx->slice;
    json_t *jmb_y = json_array_get(jdata, mb_y);
    json_t *jint;
    char buf[10];
    snprintf(buf, sizeof(buf), "%d", s->mb_x);
    jint = json_object_get(jmb_y, buf);
    if ( jint == NULL )
    {
        av_log(NULL, AV_LOG_ERROR,
               "FFedit: qscale value for slice at line %d row %d not found!\n",
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
    qscale_ctx *ctx = json_userdata_get(jframe);
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
    qscale_ctx *ctx = json_userdata_get(jframe);
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
        saved = ffe_transplicate_save(&s->ffe_xp);

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
        ffe_transplicate_restore(&s->ffe_xp, saved);
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
static void
ffe_mpeg12_export_dct_init(MpegEncContext *s, AVFrame *f)
{
    json_t *jframe;
    int nb_components = 3;
    int h_count[3] = { 2, 1, 1 };
    int v_count[3] = { 2, 1, 1 };

    if ( !s->chroma_y_shift )
        v_count[1] = v_count[2] = 2;

    jframe = ffe_jmb_new(s->jctx,
                         s->mb_width, s->mb_height,
                         nb_components,
                         v_count, h_count,
                         0);

    f->ffedit_sd[FFEDIT_FEAT_Q_DCT] = jframe;
}

static void
ffe_mpeg12_import_dct_init(MpegEncContext *s, AVFrame *f)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_Q_DCT];
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
    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_Q_DCT)) != 0 )
    {
        memcpy(ctx->last_dc, s->last_dc, sizeof(ctx->last_dc));
        s->pb = *ffe_transplicate_save(&s->ffe_xp);
    }
}

static void ffe_mpeg12_use_block(
        MpegEncContext *s,
        ffe_mpeg12_block *ctx,
        int i)
{
    AVFrame *f = s->current_picture_ptr->f;
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_Q_DCT];
    int component = (i < 4) ? 0 : (1 + ((i - 4) & 1));
    int blockn = (i < 4) ? i : ((i - 4) >> 1);
    int j;

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_Q_DCT)) != 0 )
    {
        json_t *jso = json_array_of_ints_new(s->jctx, 64);
        json_set_pflags(jso, JSON_PFLAGS_NO_LF);

        for ( j = 0; j < 64; j++ )
        {
            int k = s->intra_scantable.permutated[j];
            json_array_set_int(s->jctx, jso, j, ctx->qblock[k]);
        }

        ffe_jmb_set(jframe, component, s->mb_y, s->mb_x, blockn, jso);
    }
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_Q_DCT)) != 0 )
    {
        json_t *jso;

        jso = ffe_jmb_get(jframe, component, s->mb_y, s->mb_x, blockn);
        for ( j = 0; j < 64; j++ )
        {
            int k = s->intra_scantable.permutated[j];
            ctx->qblock[k] = json_array_get_int(jso, j);
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
        if ( s->codec_id == AV_CODEC_ID_MPEG2VIDEO)
        {
            int mismatch = 1;
            for ( j = 0; j < 64; j++ )
                mismatch ^= s->block[i][j];
            s->block[i][63] ^= (mismatch & 1);
        }
    }

    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_Q_DCT)) != 0 )
    {
        int last_dc[3];
        memcpy(last_dc, s->last_dc, sizeof(last_dc));
        memcpy(s->last_dc, ctx->last_dc, sizeof(s->last_dc));
        ffe_mpeg1_encode_block(s, ctx->qblock, i);
        ffe_transplicate_restore(&s->ffe_xp, &s->pb);
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
        ffe_mpeg12_use_block(s, &bctx, n);
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
        ffe_mpeg12_use_block(s, &bctx, n);
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
        ffe_mpeg12_use_block(s, &bctx, n);
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
        ffe_mpeg12_use_block(s, &bctx, n);
    return ret;
}

//---------------------------------------------------------------------
// mb

typedef struct
{
    json_t *jsizes;
    json_t *jdatas;
} ffe_mb_ctx;

typedef struct
{
    GetBitContext saved;
    PutBitContext pb;
    uint8_t *data;
} ffe_mb_mb_ctx;

static void
ffe_mb_export_init(MpegEncContext *s, ffe_mb_mb_ctx *mbctx)
{
    int size = (get_bits_left(&s->gb) + 7) >> 3; // in bytes

    mbctx->data = av_malloc(size);

    init_put_bits(&mbctx->pb, mbctx->data, size);

    s->gb.pb = &mbctx->pb;
}

static char
hexchar(uint8_t nibble)
{
    if ( nibble <= 9 )
        return '0' + nibble;
    return 'A' + nibble - 10;
}

static void
ffe_mb_export_flush(MpegEncContext *s, ffe_mb_mb_ctx *mbctx)
{
    AVFrame *f = s->current_picture_ptr->f;
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_MB];
    ffe_mb_ctx *ctx = json_userdata_get(jframe);
    size_t idx = s->mb_y * s->mb_width + s->mb_x;
    int size = put_bits_count(&mbctx->pb); // in bits
    int i;

    json_t *jsize = json_int_new(s->jctx, size);
    json_t *jdata;
    char *str;

    size = (size + 7) >> 3; // in bytes
    str = av_malloc(size*2 + 1);
    str[size*2] = '\0';

    flush_put_bits(&mbctx->pb);
    for ( i = 0; i < size; i++ )
    {
        str[i*2+0] = hexchar(mbctx->pb.buf[i] >> 4);
        str[i*2+1] = hexchar(mbctx->pb.buf[i] & 0x0F);
    }

    jdata = json_string_new(s->jctx, str);

    json_array_set(ctx->jsizes, idx, jsize);
    json_array_set(ctx->jdatas, idx, jdata);

    av_free(mbctx->data);
    s->gb.pb = NULL;
}

static int
hexval(char c)
{
    if ( c >= '0' && c <= '9' )
        return c - '0';
    return c - 'A' + 10;
}

static void
ffe_mb_import_init(MpegEncContext *s, ffe_mb_mb_ctx *mbctx)
{
    AVFrame *f = s->current_picture_ptr->f;
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_MB];
    ffe_mb_ctx *ctx = json_userdata_get(jframe);
    size_t idx = s->mb_y * s->mb_width + s->mb_x;

    json_t *jdata = json_array_get(ctx->jdatas, idx);
    const char *str = json_string_get(jdata);
    int size = strlen(str) / 2;
    int i;

    mbctx->data = av_mallocz(size + 4);

    for ( i = 0; i < size; i++ )
    {
        int c1 = hexval(str[i*2+0]);
        int c2 = hexval(str[i*2+1]);
        mbctx->data[i] = (c1 << 4) | c2;
    }

    mbctx->saved = s->gb;
    init_get_bits(&s->gb, mbctx->data, (size + 4) * 8);

    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_MB)) != 0 )
        s->gb.pb = ffe_transplicate_pb(&s->ffe_xp);
}

static void
ffe_mb_import_flush(MpegEncContext *s, ffe_mb_mb_ctx *mbctx)
{
    AVFrame *f = s->current_picture_ptr->f;
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_MB];
    ffe_mb_ctx *ctx = json_userdata_get(jframe);
    size_t idx = s->mb_y * s->mb_width + s->mb_x;

    int size = json_array_get_int(ctx->jsizes, idx); // in bits

    av_free(mbctx->data);
    s->gb = mbctx->saved;

    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_MB)) != 0 )
        s->gb.pb = NULL;

    while ( size > 32 )
    {
        skip_bits_long(&s->gb, 32);
        size -= 32;
    }
    if ( size > 0 )
        skip_bits_long(&s->gb, size);

    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_MB)) != 0 )
        s->gb.pb = ffe_transplicate_pb(&s->ffe_xp);
}

static void
ffe_mpeg12_export_mb_cleanup(MpegEncContext *s, AVFrame *f)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_MB];
    json_object_done(s->jctx, jframe);
}

static void
ffe_mpeg12_export_mb_init(MpegEncContext *s, AVFrame *f)
{
    int mb_count = s->mb_width * s->mb_height;

    // {
    //  "sizes": [ ] # MUST NOT CHANGE!
    //  "data": [ ]
    //  ]
    // }

    json_t *jframe = json_object_new(s->jctx);
    ffe_mb_ctx *ctx = json_allocator_get0(s->jctx, sizeof(ffe_mb_ctx));
    json_userdata_set(jframe, ctx);

    ctx->jdatas = json_array_new(s->jctx);
    ctx->jsizes = json_array_new(s->jctx);
    json_array_alloc(s->jctx, ctx->jdatas, mb_count);
    json_array_alloc(s->jctx, ctx->jsizes, mb_count);
    json_object_add(jframe, "data", ctx->jdatas);
    json_object_add(jframe, "sizes", ctx->jsizes);
    json_set_pflags(ctx->jsizes, JSON_PFLAGS_NO_LF);

    f->ffedit_sd[FFEDIT_FEAT_MB] = jframe;
}

static void
ffe_mpeg12_import_mb_init(MpegEncContext *s, AVFrame *f)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_MB];
    ffe_mb_ctx *ctx = json_allocator_get0(s->jctx, sizeof(ffe_mb_ctx));
    json_userdata_set(jframe, ctx);
    ctx->jsizes = json_object_get(jframe, "sizes");
    ctx->jdatas = json_object_get(jframe, "data");
}

static int mpeg_decode_mb(MpegEncContext *s, int16_t block[12][64]);

static int
ffe_mpeg_decode_mb(MpegEncContext *s, int16_t block[12][64])
{
    ffe_mb_mb_ctx mbctx;
    int ret;

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MB)) != 0 )
        ffe_mb_export_init(s, &mbctx);
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MB)) != 0 )
        ffe_mb_import_init(s, &mbctx);

    ret = mpeg_decode_mb(s, s->block);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MB)) != 0 )
        ffe_mb_export_flush(s, &mbctx);
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MB)) != 0 )
        ffe_mb_import_flush(s, &mbctx);

    return ret;
}

/*-------------------------------------------------------------------*/
/* common                                                            */
/*-------------------------------------------------------------------*/

static void
ffe_mpeg12_prepare_frame(AVCodecContext *avctx, MpegEncContext *s, AVPacket *avpkt)
{
    memcpy(s->ffedit_sd, avpkt->ffedit_sd, sizeof(s->ffedit_sd));
    s->jctx = avctx->jctx;
}

static void
ffe_mpeg12_init(MpegEncContext *s)
{
    AVFrame *f = s->current_picture_ptr->f;
    memcpy(f->ffedit_sd, s->ffedit_sd, sizeof(f->ffedit_sd));

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

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_Q_DCT)) != 0 )
        ffe_mpeg12_export_dct_init(s, f);
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_Q_DCT)) != 0 )
        ffe_mpeg12_import_dct_init(s, f);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MB)) != 0 )
        ffe_mpeg12_export_mb_init(s, f);
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_MB)) != 0 )
        ffe_mpeg12_import_mb_init(s, f);
}

static void
ffe_mpeg12_export_cleanup(MpegEncContext *s, AVFrame *f)
{
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_INFO)) != 0 )
        ffe_mpeg12_export_info_cleanup(s, f);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MV)) != 0 )
        ffe_mv_export_cleanup(s->jctx, f);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_QSCALE)) != 0 )
        ffe_mpeg12_export_qscale_cleanup(s, f);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_MB)) != 0 )
        ffe_mpeg12_export_mb_cleanup(s, f);
}
