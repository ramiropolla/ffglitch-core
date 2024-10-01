
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
         : (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_Q_DC))        != 0 ? FFEDIT_FEAT_Q_DC
         : (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_Q_DC_DELTA))  != 0 ? FFEDIT_FEAT_Q_DC_DELTA
         :                                                                   FFEDIT_FEAT_LAST;
}

//---------------------------------------------------------------------
static enum FFEditFeature
i_which_dct_feat(MJpegDecodeContext *s)
{
    return (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_Q_DCT))       != 0 ? FFEDIT_FEAT_Q_DCT
         : (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_Q_DCT_DELTA)) != 0 ? FFEDIT_FEAT_Q_DCT_DELTA
         : (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_Q_DC))        != 0 ? FFEDIT_FEAT_Q_DC
         : (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_Q_DC_DELTA))  != 0 ? FFEDIT_FEAT_Q_DC_DELTA
         :                                                                   FFEDIT_FEAT_LAST;
}

//---------------------------------------------------------------------
static enum FFEditFeature
a_which_dct_feat(MJpegDecodeContext *s)
{
    return (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_Q_DCT))       != 0 ? FFEDIT_FEAT_Q_DCT
         : (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_Q_DCT_DELTA)) != 0 ? FFEDIT_FEAT_Q_DCT_DELTA
         : (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_Q_DC))        != 0 ? FFEDIT_FEAT_Q_DC
         : (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_Q_DC_DELTA))  != 0 ? FFEDIT_FEAT_Q_DC_DELTA
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

    if ( e_dct_feat == FFEDIT_FEAT_Q_DC
      || e_dct_feat == FFEDIT_FEAT_Q_DC_DELTA )
    {
        pflags = JSON_PFLAGS_NO_LF;
    }

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
        qctx->saved = ffe_transplicate_bits_save(&s->ffe_xp);
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
    if ( i_dct_feat == FFEDIT_FEAT_Q_DC
      || i_dct_feat == FFEDIT_FEAT_Q_DC_DELTA )
    {
        val = ffe_jmb_array_of_ints_get(jso, component, mb_y, mb_x, block);
    }
    else
    {
        val = ffe_jmb_get(jso, component, mb_y, mb_x, block)->array_of_ints[i];
    }
    if ( (i == 0)
      && (i_dct_feat == FFEDIT_FEAT_Q_DCT
       || i_dct_feat == FFEDIT_FEAT_Q_DC) )
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
      && (e_dct_feat == FFEDIT_FEAT_Q_DCT
       || e_dct_feat == FFEDIT_FEAT_Q_DC) )
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

//---------------------------------------------------------------------
static inline int mjpeg_decode_dc(MJpegDecodeContext *s, int dc_index);
static inline int ffe_mjpeg_decode_dc(
        MJpegDecodeContext *s,
        ffe_q_dct_ctx_t *qctx,
        int dc_index,
        int component,
        int mb_y,
        int mb_x,
        int block)
{
    enum FFEditFeature e_dct_feat = e_which_dct_feat(s);
    enum FFEditFeature i_dct_feat = i_which_dct_feat(s);
    enum FFEditFeature a_dct_feat = a_which_dct_feat(s);

    PutBitContext *saved = NULL;
    int code;

    if ( a_dct_feat == FFEDIT_FEAT_Q_DC
      || a_dct_feat == FFEDIT_FEAT_Q_DC_DELTA )
    {
        saved = ffe_transplicate_bits_save(&s->ffe_xp);
    }

    code = mjpeg_decode_dc(s, dc_index);

    if ( e_dct_feat == FFEDIT_FEAT_Q_DC
      || e_dct_feat == FFEDIT_FEAT_Q_DC_DELTA )
    {
        ffe_dct_set(s, qctx, component, mb_y, mb_x, block, 0, code);
    }
    else if ( i_dct_feat == FFEDIT_FEAT_Q_DC
           || i_dct_feat == FFEDIT_FEAT_Q_DC_DELTA )
    {
        code = ffe_dct_get(s, qctx, component, mb_y, mb_x, block, 0);
    }
    if ( a_dct_feat == FFEDIT_FEAT_Q_DC
      || a_dct_feat == FFEDIT_FEAT_Q_DC_DELTA )
    {
        if ( dc_index == 0 )
            ff_mjpeg_encode_dc(saved, code, s->m.huff_size_dc_luminance, s->m.huff_code_dc_luminance);
        else
            ff_mjpeg_encode_dc(saved, code, s->m.huff_size_dc_chrominance, s->m.huff_code_dc_chrominance);
        ffe_transplicate_bits_restore(&s->ffe_xp, saved);
    }

    return code;
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
        ff_mjpeg_encode_block(s, qctx->saved, &s->m, s->permutated_scantable,
                              last_dc, block_last_index, qblock, n);
        ffe_transplicate_bits_restore(&s->ffe_xp, qctx->saved);
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
ffe_mjpeg_export_dqt_init(MJpegDecodeContext *s, AVFrame *f)
{
    // {
    //  "tables": [ ] # plane
    //            [ ] # dqt
    // }
    json_t *jdata = json_array_new(s->jctx, 4);
    json_kvp_t kvps[] = {
        { "tables", jdata },
        { NULL }
    };
    json_t *jframe = json_const_object_from(s->jctx, kvps);
    ffe_dqt_ctx_t *dctx = json_allocator_get0(s->jctx, sizeof(ffe_dqt_ctx_t));
    dctx->jdata = jdata;
    dctx->max_index = -1;
    json_object_userdata_set(jframe, dctx);
    f->ffedit_sd[FFEDIT_FEAT_DQT] = jframe;
}

//---------------------------------------------------------------------
static void
ffe_mjpeg_import_dqt_init(MJpegDecodeContext *s, AVFrame *f)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_DQT];
    ffe_dqt_ctx_t *dctx = json_allocator_get0(s->jctx, sizeof(ffe_dqt_ctx_t));
    json_object_userdata_set(jframe, dctx);
    dctx->jdata = json_object_get(jframe, "tables");
}

//---------------------------------------------------------------------
static ffe_dqt_ctx_t *
ffe_mjpeg_dqt_init(MJpegDecodeContext *s)
{
    ffe_dqt_ctx_t *dctx = NULL;
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_DQT)) != 0
      || (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_DQT)) != 0
      || (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_DQT)) != 0 )
    {
        AVFrame *f = s->picture_ptr;
        json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_DQT];
        dctx = json_object_userdata_get(jframe);
        if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_DQT)) != 0 )
            dctx->saved = ffe_transplicate_bits_save(&s->ffe_xp);
    }
    return dctx;
}

/* export */

//---------------------------------------------------------------------
static void
ffe_mjpeg_dqt_table(
        MJpegDecodeContext *s,
        ffe_dqt_ctx_t *dctx,
        int precision,
        int index)
{
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_DQT)) != 0 )
    {
        json_t *jvals = json_array_of_ints_new(s->jctx, 64);
        json_set_pflags(jvals, JSON_PFLAGS_SPLIT8);
        if ( dctx->max_index < index )
            dctx->max_index = index;
        json_array_set(dctx->jdata, index, jvals);
        dctx->jcur = jvals;
    }
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_DQT)) != 0 )
    {
        dctx->jcur = json_array_get(dctx->jdata, index);
    }

    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_DQT)) != 0 )
    {
        put_bits(dctx->saved, 4, precision);
        put_bits(dctx->saved, 4, index);
    }
}

//---------------------------------------------------------------------
static int
ffe_mjpeg_dqt_val(
        MJpegDecodeContext *s,
        ffe_dqt_ctx_t *dctx,
        int precision,
        int i)
{
    int code = get_bits(&s->gb, precision);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_DQT)) != 0 )
        dctx->jcur->array_of_ints[ff_zigzag_direct[i]] = code;
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_DQT)) != 0 )
        code = dctx->jcur->array_of_ints[ff_zigzag_direct[i]];

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
        ffe_transplicate_bits_restore(&s->ffe_xp, dctx->saved);
}

//---------------------------------------------------------------------
static void
ffe_mjpeg_export_dqt_cleanup(MJpegDecodeContext *s, AVFrame *f)
{
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_DQT)) != 0 )
    {
        json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_DQT];
        ffe_dqt_ctx_t *dctx = json_object_userdata_get(jframe);
        json_set_len(dctx->jdata, dctx->max_index+1);
    }
}

//---------------------------------------------------------------------
// dht

//---------------------------------------------------------------------
typedef struct {
    PutBitContext *saved;
    json_t *jtables;
    json_t *jsegment;
    int cur_segment;
    int cur_table;
} ffe_dht_ctx_t;

/* init */

//---------------------------------------------------------------------
static void
ffe_mjpeg_export_dht_init(MJpegDecodeContext *s, AVFrame *f)
{
    // {
    //  "tables": [
    //   [
    //    {
    //     "class": int,
    //     "index": int,
    //     "bits" : [
    //      [ ][16], # values for 1 to 16 bits
    //     ]
    //    },
    //    [...]
    //   ],
    //   [...]
    //  ]
    // }
    json_t *jtables = json_dynamic_array_new(s->jctx);
    json_kvp_t kvps[] = {
        { "tables", jtables },
        { NULL }
    };
    json_t *jframe = json_const_object_from(s->jctx, kvps);
    ffe_dht_ctx_t *dctx = json_allocator_get0(s->jctx, sizeof(ffe_dht_ctx_t));
    dctx->jtables = jtables;
    json_object_userdata_set(jframe, dctx);
    f->ffedit_sd[FFEDIT_FEAT_DHT] = jframe;
}

//---------------------------------------------------------------------
static void
ffe_mjpeg_import_dht_init(MJpegDecodeContext *s, AVFrame *f)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_DHT];
    ffe_dht_ctx_t *dctx = json_allocator_get0(s->jctx, sizeof(ffe_dht_ctx_t));
    json_object_userdata_set(jframe, dctx);
    dctx->jtables = json_object_get(jframe, "tables");
}

//---------------------------------------------------------------------
static ffe_dht_ctx_t *
ffe_mjpeg_dht_init(MJpegDecodeContext *s)
{
    ffe_dht_ctx_t *dctx = NULL;
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_DHT)) != 0
      || (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_DHT)) != 0
      || (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_DHT)) != 0 )
    {
        AVFrame *f = s->picture_ptr;
        json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_DHT];
        dctx = json_object_userdata_get(jframe);
        if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_DHT)) != 0 )
        {
            dctx->jsegment = json_dynamic_array_new(s->jctx);
            json_dynamic_array_add(dctx->jtables, dctx->jsegment);
        }
        else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_DHT)) != 0 )
        {
            dctx->jsegment = json_array_get(dctx->jtables, dctx->cur_segment++);
            dctx->cur_table = 0;
        }
        if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_DHT)) != 0 )
            dctx->saved = ffe_transplicate_bits_save(&s->ffe_xp);
    }
    return dctx;
}

/* export */

//---------------------------------------------------------------------
static int
ffe_mjpeg_dht_len(MJpegDecodeContext *s, ffe_dht_ctx_t *dctx)
{
    int len = get_bits(&s->gb, 16) - 2;

    if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_DHT)) != 0 )
    {
        size_t nb_tables = json_array_length(dctx->jsegment);
        len = 0;
        for ( size_t i = 0; i < nb_tables; i++ )
        {
            json_t *jtable = json_array_get(dctx->jsegment, i);
            json_t *jbits;

            jbits = json_object_get(jtable, "bits");
            for ( int j = 0; j < 16; j++ )
            {
                json_t *jvals = json_array_get(jbits, j);
                len += json_array_length(jvals);
            }
            len += 17;
        }
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
        json_t *jtable = json_array_get(dctx->jsegment, dctx->cur_table++);
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
                val_table[val_idx++] = jvals->array_of_ints[j];
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
        json_t *jclass = json_int_new(s->jctx, class);
        json_t *jindex = json_int_new(s->jctx, index);
        json_t *jbits = json_array_new(s->jctx, 16);
        json_kvp_t kvps[] = {
            { "class", jclass },
            { "index", jindex },
            { "bits", jbits },
            { NULL },
        };
        json_t *jtable = json_const_object_from(s->jctx, kvps);
        int val_idx = 0;

        json_dynamic_array_add(dctx->jsegment, jtable);

        for ( size_t i = 0; i < 16; i++ )
        {
            size_t len = bits_table[1+i];
            json_t *jvals = json_array_of_ints_new(s->jctx, len);
            json_set_pflags(jvals, JSON_PFLAGS_NO_LF);
            for ( int j = 0; j < len; j++ )
                jvals->array_of_ints[j] = val_table[val_idx++];
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
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_DHT)) != 0 )
        json_dynamic_array_done(s->jctx, dctx->jsegment);
    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_DHT)) != 0 )
        ffe_transplicate_bits_restore(&s->ffe_xp, dctx->saved);
}

//---------------------------------------------------------------------
static void
ffe_mjpeg_export_dht_cleanup(MJpegDecodeContext *s, AVFrame *f)
{
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_DHT)) != 0 )
    {
        json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_DHT];
        ffe_dht_ctx_t *dctx = json_object_userdata_get(jframe);
        json_dynamic_array_done(s->jctx, dctx->jtables);
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
    AVFrame *f = s->picture_ptr;

    if ( s->avctx->ffedit_import != 0 )
        memcpy(f->ffedit_sd, s->ffedit_sd, sizeof(json_t *)*FFEDIT_FEAT_LAST);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_DQT)) != 0 )
        ffe_mjpeg_export_dqt_init(s, f);
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_DQT)) != 0 )
        ffe_mjpeg_import_dqt_init(s, f);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_DHT)) != 0 )
        ffe_mjpeg_export_dht_init(s, f);
    else if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_DHT)) != 0 )
        ffe_mjpeg_import_dht_init(s, f);
}

//---------------------------------------------------------------------
static void
ffe_mjpeg_export_cleanup(MJpegDecodeContext *s)
{
    AVFrame *f = s->picture_ptr;

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_DQT)) != 0 )
        ffe_mjpeg_export_dqt_cleanup(s, f);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_DHT)) != 0 )
        ffe_mjpeg_export_dht_cleanup(s, f);
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
