
/* This file is included by mjpegdec.c */

#include "ffedit_json.h"

//---------------------------------------------------------------------
// info

// TODO

//---------------------------------------------------------------------
// dct

//---------------------------------------------------------------------
typedef struct {
    PutBitContext *saved;
    int last_dc_component;
} ffe_q_dct_ctx_t;

/* init */

//---------------------------------------------------------------------
static json_t *
ffe_dct_scan_new(MJpegDecodeContext *s)
{
    // {
    //  "data": mb array
    //  "luma_max": int
    //  "chroma_max": int
    // }

    AVFrame *f = s->picture_ptr;
    json_t *jluma_max = json_int_new(f->jctx, s->huff_max_dc_luminance);
    json_t *jchroma_max = json_int_new(f->jctx, s->huff_max_dc_chrominance);
    json_t *jscan;
    int pflags = 0;

    jscan = ffe_jmb_new(f->jctx,
                        s->mb_width, s->mb_height,
                        s->nb_components,
                        s->v_scount, s->h_scount,
                        pflags);

    json_object_add(jscan, "dc_luma_max", jluma_max);
    json_object_add(jscan, "dc_chroma_max", jchroma_max);
    jluma_max = json_int_new(f->jctx, s->huff_max_ac_luminance);
    jchroma_max = json_int_new(f->jctx, s->huff_max_ac_chrominance);
    json_object_add(jscan, "ac_luma_max", jluma_max);
    json_object_add(jscan, "ac_chroma_max", jchroma_max);

    return jscan;
}

//---------------------------------------------------------------------
static void
ffe_dct_export_scan(MJpegDecodeContext *s)
{
    AVFrame *f = s->picture_ptr;
    json_t *jframe = ffe_dct_scan_new(s);
    f->ffedit_sd[FFEDIT_FEAT_Q_DCT] = jframe;
}

//---------------------------------------------------------------------
static void
ffe_dct_import_scan(MJpegDecodeContext *s, int nb_components)
{
    AVFrame *f = s->picture_ptr;
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_Q_DCT];
    ffe_jmb_set_context(jframe, nb_components, s->v_scount, s->h_scount);
}

//---------------------------------------------------------------------
static void
ffe_mjpeg_dct_init(
        MJpegDecodeContext *s,
        ffe_q_dct_ctx_t *qctx,
        int16_t qblock[64],
        int component)
{
    qctx->saved = NULL;
    qctx->last_dc_component = s->last_dc[component];

    memset(qblock, 0, sizeof(int16_t) * 64);
    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_Q_DCT)) != 0 )
        qctx->saved = ffe_transplicate_save(&s->ffe_xp);
}

/* export */

//---------------------------------------------------------------------
static int
ffe_dct_get(
        MJpegDecodeContext *s,
        int component,
        int mb_y,
        int mb_x,
        int block,
        int i)
{
    AVFrame *f = s->picture_ptr;
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_Q_DCT];
    json_t *jso = jframe;
    json_t *jval = ffe_jmb_get(jso, component, mb_y, mb_x, block);
    return json_array_get_int(jval, i);
}

//---------------------------------------------------------------------
static void
ffe_dct_set(
        MJpegDecodeContext *s,
        int component,
        int mb_y,
        int mb_x,
        int block,
        int i,
        int code)
{
    AVFrame *f = s->picture_ptr;
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_Q_DCT];
    json_t *jso = jframe;
    json_t *jmb = ffe_jmb_get(jso, component, mb_y, mb_x, block);
    if ( jmb == NULL )
    {
        jmb = json_array_of_ints_new(f->jctx, 64);
        json_set_pflags(jmb, JSON_PFLAGS_NO_LF);
        ffe_jmb_set(jso, component, mb_y, mb_x, block, jmb);
    }
    json_array_set_int(f->jctx, jmb, i, code);
}

/* cleanup */

//---------------------------------------------------------------------
static void
ffe_dct_zero_fill(
        MJpegDecodeContext *s,
        int component,
        int mb_y,
        int mb_x,
        int block)
{
    AVFrame *f = s->picture_ptr;
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_Q_DCT];
    json_t *jso = jframe;
    json_t *jarr = ffe_jmb_get(jso, component, mb_y, mb_x, block);
    ffe_json_array_zero_fill(f->jctx, jarr, 64);
}

//---------------------------------------------------------------------
static void
ffe_mjpeg_dct_term(
        MJpegDecodeContext *s,
        ffe_q_dct_ctx_t *qctx,
        int16_t qblock[64],
        int16_t *block,
        int component,
        int mb_y,
        int mb_x,
        int blockn,
        int ac_index,
        uint16_t *quant_matrix)
{
    int last_index = 0;

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_Q_DCT)) != 0 )
    {
        for ( size_t i = 0; i < 64; i++ )
        {
            size_t j = s->scantable.permutated[i];
            ffe_dct_set(s, component, mb_y, mb_x, blockn, i, qblock[j]);
        }
        ffe_dct_zero_fill(s, component, mb_y, mb_x, blockn);
    }
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_Q_DCT)) != 0 )
    {
        int level = ffe_dct_get(s, component, mb_y, mb_x, blockn, 0);
        level = level * (unsigned)quant_matrix[0] + qctx->last_dc_component;
        level = av_clip_int16(level);
        s->last_dc[component] = level;
        block[0] = level;
        for ( size_t i = 1; i < 64; i++ )
        {
            size_t j = s->scantable.permutated[i];
            level = ffe_dct_get(s, component, mb_y, mb_x, blockn, i);
            qblock[j] = level;
            if ( qblock[j] != 0 )
            {
                block[j] = level * (unsigned)quant_matrix[i];
                last_index = i;
            }
        }
    }

    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_Q_DCT)) != 0 )
    {
        int last_dc[MAX_COMPONENTS] = { 0 };
        int block_last_index[5];
        int n = (ac_index == 0) ? 0 : 4;
        block_last_index[n] = last_index;
        ff_mjpeg_encode_block(qctx->saved, &s->m, &s->scantable, last_dc,
                              block_last_index, qblock, n);
        ffe_transplicate_restore(&s->ffe_xp, qctx->saved);
    }
}

/*-------------------------------------------------------------------*/
/* common                                                            */
/*-------------------------------------------------------------------*/

//---------------------------------------------------------------------
static void
ffe_mjpeg_build_huffman_codes(
        MJpegDecodeContext *s,
        int class,
        int index,
        uint8_t *huff_size,
        uint16_t *huff_code,
        const uint8_t *bits_table,
        const uint8_t *val_table)
{
    int k = ff_mjpeg_build_huffman_codes(huff_size, huff_code, bits_table, val_table);

    if ( class == 0 && index == 0 )
    {
        memcpy(s->m.huff_size_dc_luminance, huff_size, sizeof(s->m.huff_size_dc_luminance));
        memcpy(s->m.huff_code_dc_luminance, huff_code, sizeof(s->m.huff_code_dc_luminance));
        s->huff_max_dc_luminance = (1 << k) - 1;
    }
    if ( class == 0 && index == 1 )
    {
        memcpy(s->m.huff_size_dc_chrominance, huff_size, sizeof(s->m.huff_size_dc_chrominance));
        memcpy(s->m.huff_code_dc_chrominance, huff_code, sizeof(s->m.huff_code_dc_chrominance));
        s->huff_max_dc_chrominance = (1 << k) - 1;
    }
    if ( class == 1 && index == 0 )
    {
        memcpy(s->m.huff_size_ac_luminance, huff_size, sizeof(s->m.huff_size_ac_luminance));
        memcpy(s->m.huff_code_ac_luminance, huff_code, sizeof(s->m.huff_code_ac_luminance));
        s->huff_max_ac_luminance = (1 << k) - 1;
    }
    if ( class == 1 && index == 1 )
    {
        memcpy(s->m.huff_size_ac_chrominance, huff_size, sizeof(s->m.huff_size_ac_chrominance));
        memcpy(s->m.huff_code_ac_chrominance, huff_code, sizeof(s->m.huff_code_ac_chrominance));
        s->huff_max_ac_chrominance = (1 << k) - 1;
    }
}

//---------------------------------------------------------------------
static void
ffe_mjpeg_frame_unref(AVFrame *f)
{
    void *ffedit_sd[FFEDIT_FEAT_LAST] = { 0 };
    void *jctx = NULL;
    memcpy(ffedit_sd, f->ffedit_sd, sizeof(ffedit_sd));
    jctx = f->jctx;
    av_frame_unref(f);
    memcpy(f->ffedit_sd, ffedit_sd, sizeof(ffedit_sd));
    f->jctx = jctx;
}

//---------------------------------------------------------------------
static void
ffe_mjpeg_init(MJpegDecodeContext *s)
{
    memcpy(s->picture_ptr->ffedit_sd, s->ffedit_sd, sizeof(s->ffedit_sd));
    s->picture_ptr->jctx = s->jctx;
}

//---------------------------------------------------------------------
static void
ffe_mjpeg_prepare_frame(MJpegDecodeContext *s, AVPacket *avpkt)
{
    memcpy(s->ffedit_sd, avpkt->ffedit_sd, sizeof(s->ffedit_sd));
    s->jctx = avpkt->jctx;
    ffe_mjpeg_init(s);
}

/* init */

//---------------------------------------------------------------------
static void
ffe_mjpeg_scan_init(
        MJpegDecodeContext *s,
        int nb_components,
        int chroma_h_shift,
        int chroma_v_shift)
{
    if ( s->progressive
      && ((s->avctx->ffedit_import & (1 << FFEDIT_FEAT_Q_DCT)) != 0
       || (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_Q_DCT)) != 0
       || (s->avctx->ffedit_apply  & (1 << FFEDIT_FEAT_Q_DCT)) != 0) )
    {
        av_log(NULL, AV_LOG_ERROR,
               "FFedit doesn't support progressive JPEGs.\n");
        av_assert0(0);
    }

    s->chroma_h_shift = chroma_h_shift;
    s->chroma_v_shift = chroma_v_shift;
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_Q_DCT)) != 0 )
    {
        ffe_dct_export_scan(s);
    }
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_Q_DCT)) != 0 )
    {
        ffe_dct_import_scan(s, nb_components);
    }
}
