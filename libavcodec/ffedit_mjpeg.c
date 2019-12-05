
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
    int last_q_dc_component;
} ffe_q_dct_ctx_t;

//---------------------------------------------------------------------
static enum FFEditFeature
e_which_dct_feat(MJpegDecodeContext *s)
{
    return (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_Q_DCT))       != 0 ? FFEDIT_FEAT_Q_DCT
         : (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_Q_DCT_DELTA)) != 0 ? FFEDIT_FEAT_Q_DCT_DELTA
         :                                                                   FFEDIT_FEAT_LAST;
}

//---------------------------------------------------------------------
static enum FFEditFeature
i_which_dct_feat(MJpegDecodeContext *s)
{
    return (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_Q_DCT))       != 0 ? FFEDIT_FEAT_Q_DCT
         : (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_Q_DCT_DELTA)) != 0 ? FFEDIT_FEAT_Q_DCT_DELTA
         :                                                                   FFEDIT_FEAT_LAST;
}

//---------------------------------------------------------------------
static enum FFEditFeature
a_which_dct_feat(MJpegDecodeContext *s)
{
    return (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_Q_DCT))       != 0 ? FFEDIT_FEAT_Q_DCT
         : (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_Q_DCT_DELTA)) != 0 ? FFEDIT_FEAT_Q_DCT_DELTA
         :                                                                  FFEDIT_FEAT_LAST;
}

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

    enum FFEditFeature e_dct_feat = e_which_dct_feat(s);
    json_t *jscan;
    int pflags = 0;

    jscan = ffe_jmb_new(s->jctx,
                        s->mb_width, s->mb_height,
                        s->nb_components,
                        s->v_scount, s->h_scount,
                        s->quant_index,
                        pflags);

    return jscan;
}

//---------------------------------------------------------------------
static void
ffe_dct_export_scan(MJpegDecodeContext *s)
{
    enum FFEditFeature e_dct_feat = e_which_dct_feat(s);
    AVFrame *f = s->picture_ptr;
    json_t *jframe = ffe_dct_scan_new(s);
    f->ffedit_sd[e_dct_feat] = jframe;
}

//---------------------------------------------------------------------
static void
ffe_dct_import_scan(MJpegDecodeContext *s, int nb_components)
{
    enum FFEditFeature i_dct_feat = i_which_dct_feat(s);
    AVFrame *f = s->picture_ptr;
    json_t *jframe = f->ffedit_sd[i_dct_feat];
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
    enum FFEditFeature a_dct_feat = a_which_dct_feat(s);

    qctx->saved = NULL;
    qctx->last_q_dc_component = s->last_q_dc[component];

    memset(qblock, 0, sizeof(int16_t) * 64);
    if ( a_dct_feat == FFEDIT_FEAT_Q_DCT
      || a_dct_feat == FFEDIT_FEAT_Q_DCT_DELTA )
    {
        qctx->saved = ffe_transplicate_save(&s->ffe_xp);
    }
}

/* export */

//---------------------------------------------------------------------
static int
ffe_dct_get(
        MJpegDecodeContext *s,
        ffe_q_dct_ctx_t *qctx,
        int component,
        int mb_y,
        int mb_x,
        int block,
        int i)
{
    enum FFEditFeature i_dct_feat = i_which_dct_feat(s);
    AVFrame *f = s->picture_ptr;
    json_t *jframe = f->ffedit_sd[i_dct_feat];
    json_t *jso = jframe;
    int val;
    {
        val = ffe_jmb_get(jso, component, mb_y, mb_x, block)->array_of_ints[i];
    }
    if ( (i == 0)
      && i_dct_feat == FFEDIT_FEAT_Q_DCT )
    {
        val -= qctx->last_q_dc_component;
    }
    return val;
}

//---------------------------------------------------------------------
static void
ffe_dct_set(
        MJpegDecodeContext *s,
        ffe_q_dct_ctx_t *qctx,
        int component,
        int mb_y,
        int mb_x,
        int block,
        int i,
        int code)
{
    enum FFEditFeature e_dct_feat = e_which_dct_feat(s);
    AVFrame *f = s->picture_ptr;
    json_t *jframe = f->ffedit_sd[e_dct_feat];
    json_t *jso = jframe;
    if ( (i == 0)
      && e_dct_feat == FFEDIT_FEAT_Q_DCT )
    {
        code += qctx->last_q_dc_component;
    }
    if ( e_dct_feat == FFEDIT_FEAT_Q_DCT
      || e_dct_feat == FFEDIT_FEAT_Q_DCT_DELTA )
    {
        json_t *jmb = ffe_jmb_get(jso, component, mb_y, mb_x, block);
        if ( jmb == NULL )
        {
            jmb = json_array_of_ints_new(s->jctx, 64);
            json_set_pflags(jmb, JSON_PFLAGS_NO_LF);
            ffe_jmb_set(jso, component, mb_y, mb_x, block, jmb);
        }
        jmb->array_of_ints[i] = code;
    }
    else
    {
        json_t *jval = json_int_new(s->jctx, code);
        ffe_jmb_set(jso, component, mb_y, mb_x, block, jval);
    }
}

/* cleanup */

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
    enum FFEditFeature e_dct_feat = e_which_dct_feat(s);
    enum FFEditFeature i_dct_feat = i_which_dct_feat(s);
    enum FFEditFeature a_dct_feat = a_which_dct_feat(s);
    int last_index = 0;

    if ( e_dct_feat == FFEDIT_FEAT_Q_DCT
      || e_dct_feat == FFEDIT_FEAT_Q_DCT_DELTA )
    {
        for ( size_t i = 0; i < 64; i++ )
        {
            size_t j = s->permutated_scantable[i];
            ffe_dct_set(s, qctx, component, mb_y, mb_x, blockn, i, qblock[j]);
        }
    }
    else if ( i_dct_feat == FFEDIT_FEAT_Q_DCT
           || i_dct_feat == FFEDIT_FEAT_Q_DCT_DELTA )
    {
        int level = ffe_dct_get(s, qctx, component, mb_y, mb_x, blockn, 0);
        qblock[0] = level;
        level += qctx->last_q_dc_component;
        s->last_q_dc[component] = level;
        level *= (unsigned) quant_matrix[0];
        level += s->dc_shift[component];
        block[0] = av_clip_int16(level);
        for ( size_t i = 1; i < 64; i++ )
        {
            size_t j = s->permutated_scantable[i];
            level = ffe_dct_get(s, qctx, component, mb_y, mb_x, blockn, i);
            qblock[j] = level;
            if ( qblock[j] != 0 )
            {
                block[j] = level * (unsigned)quant_matrix[i];
                last_index = i;
            }
        }
    }

    if ( a_dct_feat == FFEDIT_FEAT_Q_DCT
      || a_dct_feat == FFEDIT_FEAT_Q_DCT_DELTA )
    {
        int last_dc[MAX_COMPONENTS] = { 0 };
        int block_last_index[5];
        int n = (ac_index == 0) ? 0 : 4;
        block_last_index[n] = last_index;
        ff_mjpeg_encode_block(qctx->saved, &s->m, s->permutated_scantable,
                              last_dc, block_last_index, qblock, n);
        ffe_transplicate_restore(&s->ffe_xp, qctx->saved);
    }
}

/*-------------------------------------------------------------------*/
/* common                                                            */
/*-------------------------------------------------------------------*/

//---------------------------------------------------------------------
static int ffe_mjpeg_build_vlc(
        MJpegDecodeContext *s,
        int class,
        int index,
        const uint8_t *bits_table,
        const uint8_t *val_table,
        int is_ac,
        void *logctx)
{
    MJpegContext *m = &s->m;
    VLC *vlc = &s->vlcs[class][index];
    int ret = ff_mjpeg_build_vlc(vlc, bits_table, val_table, is_ac, logctx);
    switch ( (class << 4) | index )
    {
    case 0x00:
        memset(s->m.huff_size_dc_luminance, 0x00, sizeof(s->m.huff_size_dc_luminance));
        memset(s->m.huff_code_dc_luminance, 0x00, sizeof(s->m.huff_code_dc_luminance));
        ff_mjpeg_build_huffman_codes(m->huff_size_dc_luminance, m->huff_code_dc_luminance, bits_table, val_table);
        break;
    case 0x01:
        memset(s->m.huff_size_dc_chrominance, 0x00, sizeof(s->m.huff_size_dc_chrominance));
        memset(s->m.huff_code_dc_chrominance, 0x00, sizeof(s->m.huff_code_dc_chrominance));
        ff_mjpeg_build_huffman_codes(m->huff_size_dc_chrominance, m->huff_code_dc_chrominance, bits_table, val_table);
        break;
    case 0x10:
        memset(s->m.huff_size_ac_luminance, 0x00, sizeof(s->m.huff_size_ac_luminance));
        memset(s->m.huff_code_ac_luminance, 0x00, sizeof(s->m.huff_code_ac_luminance));
        ff_mjpeg_build_huffman_codes(m->huff_size_ac_luminance, m->huff_code_ac_luminance, bits_table, val_table);
        break;
    case 0x11:
        memset(s->m.huff_size_ac_chrominance, 0x00, sizeof(s->m.huff_size_ac_chrominance));
        memset(s->m.huff_code_ac_chrominance, 0x00, sizeof(s->m.huff_code_ac_chrominance));
        ff_mjpeg_build_huffman_codes(m->huff_size_ac_chrominance, m->huff_code_ac_chrominance, bits_table, val_table);
        break;
    }
    return ret;
}

//---------------------------------------------------------------------
static void
ffe_mjpeg_frame_unref(AVFrame *f)
{
    void *ffedit_sd[FFEDIT_FEAT_LAST] = { 0 };
    void *jctx = NULL;
    memcpy(ffedit_sd, f->ffedit_sd, sizeof(json_t *)*FFEDIT_FEAT_LAST);
    jctx = f->jctx;
    av_frame_unref(f);
    memcpy(f->ffedit_sd, ffedit_sd, sizeof(json_t *)*FFEDIT_FEAT_LAST);
    f->jctx = jctx;
}

//---------------------------------------------------------------------
static void
ffe_mjpeg_init(MJpegDecodeContext *s)
{
    if ( s->avctx->ffedit_import != 0 )
        memcpy(s->picture_ptr->ffedit_sd, s->ffedit_sd, sizeof(json_t *)*FFEDIT_FEAT_LAST);
}

//---------------------------------------------------------------------
static void
ffe_mjpeg_prepare_frame(AVCodecContext *avctx, MJpegDecodeContext *s, const AVPacket *avpkt)
{
    if ( avctx->ffedit_export != 0 )
    {
        /* create one jctx for each exported frame */
        AVFrame *f = s->picture_ptr;
        f->jctx = av_mallocz(sizeof(json_ctx_t));
        json_ctx_start(f->jctx, 1);
        s->jctx = f->jctx;
    }
    else if ( avctx->ffedit_import != 0 )
    {
        memcpy(s->ffedit_sd, avpkt->ffedit_sd, sizeof(json_t *)*FFEDIT_FEAT_LAST);
        s->jctx = avpkt->jctx;
    }
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
      && (e_which_dct_feat(s) != FFEDIT_FEAT_LAST
       || i_which_dct_feat(s) != FFEDIT_FEAT_LAST
       || a_which_dct_feat(s) != FFEDIT_FEAT_LAST) )
    {
        av_log(s->avctx, AV_LOG_ERROR,
               "FFedit doesn't support progressive JPEGs.\n");
        av_assert0(0);
    }

    s->chroma_h_shift = chroma_h_shift;
    s->chroma_v_shift = chroma_v_shift;
    if ( e_which_dct_feat(s) != FFEDIT_FEAT_LAST )
        ffe_dct_export_scan(s);
    else if ( i_which_dct_feat(s) != FFEDIT_FEAT_LAST )
        ffe_dct_import_scan(s, nb_components);
}
