
/* This file is included by mjpegdec.c */

#include "ffedit_json.h"

//---------------------------------------------------------------------
// info

// TODO

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
