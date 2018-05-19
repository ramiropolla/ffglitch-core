
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

//---------------------------------------------------------------------
static enum FFEditFeature
i_dc_or_dct(MJpegDecodeContext *s)
{
    return (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_Q_DCT)) != 0
         ? FFEDIT_FEAT_Q_DCT
         : FFEDIT_FEAT_Q_DC;
}

//---------------------------------------------------------------------
static enum FFEditFeature
e_dc_or_dct(MJpegDecodeContext *s)
{
    return (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_Q_DCT)) != 0
         ? FFEDIT_FEAT_Q_DCT
         : FFEDIT_FEAT_Q_DC;
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

    AVFrame *f = s->picture_ptr;
    json_t *jluma_max = json_int_new(f->jctx, s->huff_max_dc_luminance);
    json_t *jchroma_max = json_int_new(f->jctx, s->huff_max_dc_chrominance);
    json_t *jscan;
    int pflags = 0;

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_Q_DC)) != 0 )
        pflags = JSON_PFLAGS_NO_LF;

    jscan = ffe_jmb_new(f->jctx,
                        s->mb_width, s->mb_height,
                        s->nb_components,
                        s->v_scount, s->h_scount,
                        pflags);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_Q_DC)) != 0 )
    {
        json_object_add(jscan, "luma_max", jluma_max);
        json_object_add(jscan, "chroma_max", jchroma_max);
    }
    else
    {
        json_object_add(jscan, "dc_luma_max", jluma_max);
        json_object_add(jscan, "dc_chroma_max", jchroma_max);
        jluma_max = json_int_new(f->jctx, s->huff_max_ac_luminance);
        jchroma_max = json_int_new(f->jctx, s->huff_max_ac_chrominance);
        json_object_add(jscan, "ac_luma_max", jluma_max);
        json_object_add(jscan, "ac_chroma_max", jchroma_max);
    }

    return jscan;
}

//---------------------------------------------------------------------
static void
ffe_dct_export_scan(MJpegDecodeContext *s)
{
    enum FFEditFeature feature = e_dc_or_dct(s);
    AVFrame *f = s->picture_ptr;
    json_t *jframe = ffe_dct_scan_new(s);
    f->ffedit_sd[feature] = jframe;
}

//---------------------------------------------------------------------
static void
ffe_dct_import_scan(MJpegDecodeContext *s, int nb_components)
{
    enum FFEditFeature feature = i_dc_or_dct(s);
    AVFrame *f = s->picture_ptr;
    json_t *jframe = f->ffedit_sd[feature];
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
    enum FFEditFeature feature = i_dc_or_dct(s);
    AVFrame *f = s->picture_ptr;
    json_t *jframe = f->ffedit_sd[feature];
    json_t *jso = jframe;
    json_t *jval = ffe_jmb_get(jso, component, mb_y, mb_x, block);
    if ( feature == FFEDIT_FEAT_Q_DC )
        return (int) jval;
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
    enum FFEditFeature feature = e_dc_or_dct(s);
    AVFrame *f = s->picture_ptr;
    json_t *jframe = f->ffedit_sd[feature];
    json_t *jso = jframe;
    if ( feature == FFEDIT_FEAT_Q_DCT )
    {
        json_t *jmb = ffe_jmb_get(jso, component, mb_y, mb_x, block);
        if ( jmb == NULL )
        {
            jmb = json_array_of_ints_new(f->jctx, 64);
            json_set_pflags(jmb, JSON_PFLAGS_NO_LF);
            ffe_jmb_set(jso, component, mb_y, mb_x, block, jmb);
        }
        json_array_set_int(f->jctx, jmb, i, code);
    }
    else
    {
        json_t *jval = json_int_new(f->jctx, code);
        ffe_jmb_set(jso, component, mb_y, mb_x, block, jval);
    }
}

//---------------------------------------------------------------------
static inline int mjpeg_decode_dc(MJpegDecodeContext *s, int dc_index);
static inline int ffe_mjpeg_decode_dc(
        MJpegDecodeContext *s,
        int dc_index,
        int component,
        int mb_y,
        int mb_x,
        int block)
{
    PutBitContext *saved = NULL;
    int code;

    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_Q_DC)) != 0 )
        saved = ffe_transplicate_save(&s->ffe_xp);

    code = mjpeg_decode_dc(s, dc_index);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_Q_DC)) != 0 )
        ffe_dct_set(s, component, mb_y, mb_x, block, 0, code);
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_Q_DC)) != 0 )
        code = ffe_dct_get(s, component, mb_y, mb_x, block, 0);
    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_Q_DC)) != 0 )
    {
        if ( dc_index == 0 )
            ff_mjpeg_encode_dc(saved, code, s->m.huff_size_dc_luminance, s->m.huff_code_dc_luminance);
        else
            ff_mjpeg_encode_dc(saved, code, s->m.huff_size_dc_chrominance, s->m.huff_code_dc_chrominance);
        ffe_transplicate_restore(&s->ffe_xp, saved);
    }

    return code;
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
    enum FFEditFeature feature = e_dc_or_dct(s);
    AVFrame *f = s->picture_ptr;
    json_t *jframe = f->ffedit_sd[feature];
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
        ff_mjpeg_encode_block(s, qctx->saved, &s->m, &s->scantable, last_dc,
                              block_last_index, qblock, n);
        ffe_transplicate_restore(&s->ffe_xp, qctx->saved);
    }
}

//---------------------------------------------------------------------
// dqt

//---------------------------------------------------------------------
typedef struct {
    PutBitContext *saved;
    json_t *jdata;
    json_t *jcur;
    int max_index;
} ffe_dqt_ctx_t;

/* init */

//---------------------------------------------------------------------
static void
ffe_mjpeg_dqt_init(MJpegDecodeContext *s, ffe_dqt_ctx_t *dctx)
{
    dctx->saved = NULL;
    dctx->jdata = NULL;
    dctx->jcur = NULL;
    dctx->max_index = -1;

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_DQT)) != 0 )
    {
        // {
        //  "data": [ ] # plane
        //          [ ] # dqt
        // }
        AVFrame *f = s->picture_ptr;
        json_t *jframe = json_object_new(f->jctx);
        dctx->jdata = json_array_new(f->jctx);
        json_array_alloc(f->jctx, dctx->jdata, 4);
        json_object_add(jframe, "data", dctx->jdata);
        f->ffedit_sd[FFEDIT_FEAT_DQT] = jframe;
    }
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_DQT)) != 0 )
    {
        AVFrame *f = s->picture_ptr;
        json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_DQT];
        dctx->jdata = json_object_get(jframe, "data");
    }
}

/* export */

//---------------------------------------------------------------------
static void
ffe_mjpeg_dqt_table(MJpegDecodeContext *s, ffe_dqt_ctx_t *dctx, int index)
{
    AVFrame *f = s->picture_ptr;

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_DQT)) != 0 )
    {
        dctx->jcur = json_array_of_ints_new(f->jctx, 64);
        if ( dctx->max_index < index )
            dctx->max_index = index;
        json_array_set(dctx->jdata, index, dctx->jcur);
    }
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_DQT)) != 0 )
    {
        dctx->jcur = json_array_get(dctx->jdata, index);
    }

    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_DQT)) != 0 )
        dctx->saved = ffe_transplicate_save(&s->ffe_xp);
}

//---------------------------------------------------------------------
static int
ffe_mjpeg_dqt_val(
        MJpegDecodeContext *s,
        ffe_dqt_ctx_t *dctx,
        int precision,
        int i)
{
    AVFrame *f = s->picture_ptr;
    int code = get_bits(&s->gb, precision);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_DQT)) != 0 )
        json_array_set_int(f->jctx, dctx->jcur, i, code);
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_DQT)) != 0 )
        code = json_array_get_int(dctx->jcur, i);

    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_DQT)) != 0 )
        put_bits(dctx->saved, precision, code);

    return code;
}

/* cleanup */

//---------------------------------------------------------------------
static void
ffe_mjpeg_dqt_term(
        MJpegDecodeContext *s,
        ffe_dqt_ctx_t *dctx)
{
    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_DQT)) != 0 )
        ffe_transplicate_restore(&s->ffe_xp, dctx->saved);
    if ( dctx->max_index >= 0 )
        json_set_len(dctx->jdata, dctx->max_index+1);
}

//---------------------------------------------------------------------
// dht

//---------------------------------------------------------------------
typedef struct {
    PutBitContext *saved;
    json_t *jtables;
    int table_count;
} ffe_dht_ctx_t;

/* init */

//---------------------------------------------------------------------
static void
ffe_mjpeg_dht_init(MJpegDecodeContext *s, ffe_dht_ctx_t *dctx)
{
    dctx->saved = NULL;
    dctx->jtables = NULL;
    dctx->table_count = 0;

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_DHT)) != 0 )
    {
        // {
        //  "tables": [
        //   {
        //    "class": int,
        //    "index": int,
        //    "bits" : [
        //     [ ][16], # values for 1 to 16 bits
        //    ]
        //   },
        //   [...]
        //  ]
        // }
        AVFrame *f = s->picture_ptr;
        json_t *jframe = json_object_new(f->jctx);
        dctx->jtables = json_array_new(f->jctx);
        json_object_add(jframe, "tables", dctx->jtables);
        f->ffedit_sd[FFEDIT_FEAT_DHT] = jframe;
    }
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_DHT)) != 0 )
    {
        AVFrame *f = s->picture_ptr;
        json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_DHT];
        dctx->jtables = json_object_get(jframe, "tables");
        dctx->table_count = json_array_length(dctx->jtables);
    }

    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_DHT)) != 0 )
        dctx->saved = ffe_transplicate_save(&s->ffe_xp);
}

/* export */

//---------------------------------------------------------------------
static int
ffe_mjpeg_dht_len(MJpegDecodeContext *s, ffe_dht_ctx_t *dctx)
{
    int len = get_bits(&s->gb, 16) - 2;

    if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_DHT)) != 0 )
    {
        len = 0;
        for ( size_t i = 0; i < dctx->table_count; i++ )
        {
            json_t *jtable = json_array_get(dctx->jtables, i);
            json_t *jbits;

            jbits = json_object_get(jtable, "bits");
            for ( int j = 0; j < 16; j++ )
            {
                json_t *jvals = json_array_get(jbits, j);
                len += json_array_length(jvals);
            }
            len += 17;
        }
        dctx->table_count = 0;
    }
    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_DHT)) != 0 )
        put_bits(dctx->saved, 16, len + 2);

    return len;
}

//---------------------------------------------------------------------
static void
ffe_mjpeg_dht_table(
        MJpegDecodeContext *s,
        ffe_dht_ctx_t *dctx,
        int *pclass,
        int *pindex,
        uint8_t bits_table[17],
        uint8_t val_table[256])
{
    if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_DHT)) != 0 )
    {
        json_t *jtable = json_array_get(dctx->jtables, dctx->table_count++);
        json_t *jclass = json_object_get(jtable, "class");
        json_t *jindex = json_object_get(jtable, "index");
        json_t *jbits = json_object_get(jtable, "bits");
        int val_idx = 0;

        *pclass = json_int_val(jclass);
        *pindex = json_int_val(jindex);

        for ( size_t i = 0; i < 16; i++ )
        {
            json_t *jvals = json_array_get(jbits, i);
            int bitn_len = json_array_length(jvals);
            for ( int j = 0; j < bitn_len; j++ )
                val_table[val_idx++] = json_array_get_int(jvals, j);
            bits_table[1+i] = bitn_len;
        }
    }

    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_DHT)) != 0 )
    {
        put_bits(dctx->saved, 4, *pclass);
        put_bits(dctx->saved, 4, *pindex);
    }
}

//---------------------------------------------------------------------
static int
ffe_mjpeg_dht_bits(
        MJpegDecodeContext *s,
        ffe_dht_ctx_t *dctx,
        uint8_t bits_table[17],
        size_t i)
{
    int v = get_bits(&s->gb, 8);
    if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_DHT)) != 0 )
        v = bits_table[i];
    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_DHT)) != 0 )
        put_bits(dctx->saved, 8, v);
    return v;
}

//---------------------------------------------------------------------
static int
ffe_mjpeg_dht_val(
        MJpegDecodeContext *s,
        ffe_dht_ctx_t *dctx,
        uint8_t val_table[256],
        size_t i)
{
    int v = get_bits(&s->gb, 8);
    if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_DHT)) != 0 )
        v = val_table[i];
    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_DHT)) != 0 )
        put_bits(dctx->saved, 8, v);
    return v;
}

//---------------------------------------------------------------------
static void
ffe_mjpeg_dht_export(
        MJpegDecodeContext *s,
        ffe_dht_ctx_t *dctx,
        int class,
        int index,
        uint8_t bits_table[17],
        uint8_t val_table[256])
{
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_DHT)) != 0 )
    {
        AVFrame *f = s->picture_ptr;
        json_t *jtable = json_object_new(f->jctx);
        json_t *jclass = json_int_new(f->jctx, class);
        json_t *jindex = json_int_new(f->jctx, index);
        json_t *jbits = json_array_new(f->jctx);
        int val_idx = 0;

        json_array_alloc(f->jctx, jbits, 16);

        json_array_add(dctx->jtables, jtable);
        dctx->table_count++;
        json_object_add(jtable, "class", jclass);
        json_object_add(jtable, "index", jindex);
        json_object_add(jtable, "bits", jbits);
        json_object_done(f->jctx, jtable);

        for ( size_t i = 0; i < 16; i++ )
        {
            json_t *jvals = json_array_of_ints_new(f->jctx, bits_table[1+i]);
            json_set_pflags(jvals, JSON_PFLAGS_NO_LF);
            for ( int j = 0; j < bits_table[1+i]; j++ )
                json_array_set_int(f->jctx, jvals, j, val_table[val_idx++]);
            json_array_set(jbits, i, jvals);
        }
    }
}

/* cleanup */

//---------------------------------------------------------------------
static void
ffe_mjpeg_dht_term(
        MJpegDecodeContext *s,
        ffe_dht_ctx_t *dctx)
{
    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_DHT)) != 0 )
        ffe_transplicate_restore(&s->ffe_xp, dctx->saved);
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
      && ((s->avctx->ffedit_import & (1 << FFEDIT_FEAT_Q_DC)) != 0
       || (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_Q_DC)) != 0
       || (s->avctx->ffedit_apply  & (1 << FFEDIT_FEAT_Q_DC)) != 0
       || (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_Q_DCT)) != 0
       || (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_Q_DCT)) != 0
       || (s->avctx->ffedit_apply  & (1 << FFEDIT_FEAT_Q_DCT)) != 0) )
    {
        av_log(NULL, AV_LOG_ERROR,
               "FFedit doesn't support progressive JPEGs.\n");
        av_assert0(0);
    }

    s->chroma_h_shift = chroma_h_shift;
    s->chroma_v_shift = chroma_v_shift;
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_Q_DCT)) != 0
      || (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_Q_DC)) != 0 )
    {
        ffe_dct_export_scan(s);
    }
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_Q_DCT)) != 0
           || (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_Q_DC)) != 0 )
    {
        ffe_dct_import_scan(s, nb_components);
    }
}
