
/* This file is included by mpeg4videodec.c */

#include "ffedit_json.h"

//---------------------------------------------------------------------
// info

//---------------------------------------------------------------------
static void
ffe_mpeg4_export_info(MpegEncContext *s, int mb_type)
{
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_INFO)) != 0 )
    {
        AVFrame *f = s->current_picture_ptr->f;
        json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_INFO];
        json_t *jmb_type = json_object_get(jframe, "mb_type");
        json_t *jso;
        char buf[16];
        char *ptr = buf;

        *ptr++ = (mb_type & MB_TYPE_INTRA)      ? 'I'
               : (mb_type & MB_TYPE_DIRECT2)    ? 'D'
               :                                  ' ';
        *ptr++ = (mb_type & MB_TYPE_CBP)        ? 'c' : ' ';
        *ptr++ = (mb_type & MB_TYPE_QUANT)      ? 'q' : ' ';
        *ptr++ = (mb_type & MB_TYPE_16x16)      ? 'F'
               : (mb_type & MB_TYPE_16x8)       ? 'H'
               : (mb_type & MB_TYPE_8x8)        ? '4'
               :                                  ' ';
        *ptr++ = (mb_type & MB_TYPE_GMC)        ? 'G' : ' ';
        *ptr++ = (mb_type & MB_TYPE_L0)         ? 'f' : ' ';
        *ptr++ = (mb_type & MB_TYPE_L1)         ? 'b' : ' ';
        *ptr++ = (mb_type & MB_TYPE_ACPRED)     ? 'a' : ' ';
        *ptr++ = (mb_type & MB_TYPE_INTERLACED) ? 'i' : ' ';
        *ptr++ = (mb_type & MB_TYPE_SKIP)       ? 'S' : ' ';
        *ptr = '\0';

        jso = json_string_new(f->jctx, buf);
        ffe_jmb_set(jmb_type, 0, s->mb_y, s->mb_x, 0, jso);
    }
}
