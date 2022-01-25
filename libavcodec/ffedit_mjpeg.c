
/* This file is included by mjpegdec.c */

#include "ffedit_json.h"

//---------------------------------------------------------------------
// info

// TODO

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
