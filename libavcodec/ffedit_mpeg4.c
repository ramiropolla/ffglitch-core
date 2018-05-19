
/* This file is included by mpeg4videodec.c */

#include "ffedit_json.h"

//---------------------------------------------------------------------
// info

//---------------------------------------------------------------------
static void
ffe_mpeg4_export_info(MpegEncContext *s, int ffe_mb_type, int ffe_mb_cbp)
{
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_INFO)) != 0 )
    {
        AVFrame *f = s->current_picture_ptr->f;
        json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_INFO];
        json_t *jmb_type = json_object_get(jframe, "mb_type");
        json_t *jso;
        char buf[32];
        char *ptr = buf;

        if ( ffe_mb_type == -1 )
        {
            jso = json_null_new(s->jctx);
        }
        else
        {
            *ptr++ = (ffe_mb_type & FFE_MB_TYPE_INTRA)      ? 'I' : ' ';
            *ptr++ = (ffe_mb_type & FFE_MB_TYPE_ACPRED)     ? 'a' : ' ';
            *ptr++ = (ffe_mb_type & FFE_MB_TYPE_QUANT)      ? 'q' : ' ';
            *ptr++ = (ffe_mb_type & FFE_MB_TYPE_FORWARD)    ? 'f' : ' ';
            *ptr++ = (ffe_mb_type & FFE_MB_TYPE_BACKWARD)   ? 'b' : ' ';
            *ptr++ = (ffe_mb_type & FFE_MB_TYPE_DIRECT)     ? 'd' : ' ';
            *ptr++ = (ffe_mb_type & FFE_MB_TYPE_GMC)        ? 'G' : ' ';
            *ptr++ = (ffe_mb_type & FFE_MB_TYPE_MV4)        ? '4' : ' ';
            *ptr++ = (ffe_mb_type & FFE_MB_TYPE_INTERLACED) ? 'i' : ' ';
            *ptr++ = (ffe_mb_cbp & 0x20)                    ? '1' : ' ';
            *ptr++ = (ffe_mb_cbp & 0x10)                    ? '2' : ' ';
            *ptr++ = (ffe_mb_cbp & 0x08)                    ? '3' : ' ';
            *ptr++ = (ffe_mb_cbp & 0x04)                    ? '4' : ' ';
            *ptr++ = (ffe_mb_cbp & 0x02)                    ? '5' : ' ';
            *ptr++ = (ffe_mb_cbp & 0x01)                    ? '6' : ' ';
            *ptr = '\0';

            jso = json_string_new(s->jctx, buf);
        }
        ffe_jblock_set(jmb_type, s->mb_y, s->mb_x, jso);
    }
}
