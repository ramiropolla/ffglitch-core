
/* This file is included by mpeg12dec.c */

#include "ffedit_json.h"

#include "ffedit_mpegvideo.c"

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
    const char *pict_type = s->pict_type == AV_PICTURE_TYPE_I ? "I"
                          : s->pict_type == AV_PICTURE_TYPE_P ? "P"
                          :                                     "B";
    int is_interlaced = (s->picture_structure != PICT_FRAME);

    jso = json_string_new(s->jctx, pict_type);
    json_object_add(jframe, "pict_type", jso);

    jso = json_bool_new(s->jctx, is_interlaced);
    json_object_add(jframe, "interlaced", jso);

    if ( is_interlaced )
    {
        const char *field = (s->picture_structure == PICT_TOP_FIELD)
                          ? "top"
                          : "bottom";
        jso = json_string_new(s->jctx, field);
        json_object_add(jframe, "field", jso);
    }

    jso = ffe_jblock_new(s->jctx,
                         s->mb_width, s->mb_height,
                         JSON_PFLAGS_NO_LF);
    json_object_add(jframe, "mb_type", jso);

    f->ffedit_sd[FFEDIT_FEAT_INFO] = jframe;
}

/* export */

static void
ffe_mpeg12_export_info(MpegEncContext *s, int ffe_mb_type)
{
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_INFO)) != 0 )
    {
        AVFrame *f = s->current_picture_ptr->f;
        json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_INFO];
        json_t *jmb_type = json_object_get(jframe, "mb_type");
        json_t *jso;
        char buf[10];
        char *ptr = buf;

        *ptr++ = (ffe_mb_type & FFE_MB_TYPE_INTRA)    ? 'I' : ' ';
        *ptr++ = (ffe_mb_type & FFE_MB_TYPE_QUANT)    ? 'q' : ' ';
        *ptr++ = (ffe_mb_type & FFE_MB_TYPE_CBP)      ? 'c' : ' ';
        *ptr++ = (ffe_mb_type & FFE_MB_TYPE_FORWARD)  ? 'f' : ' ';
        *ptr++ = (ffe_mb_type & FFE_MB_TYPE_BACKWARD) ? 'b' : ' ';
        *ptr = '\0';

        jso = json_string_new(s->jctx, buf);
        ffe_jblock_set(jmb_type, s->mb_y, s->mb_x, jso);
    }
}

/* cleanup */

static void
ffe_mpeg12_export_info_cleanup(MpegEncContext *s, AVFrame *f)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_INFO];
    json_object_done(s->jctx, jframe);
}

/*-------------------------------------------------------------------*/
/* common                                                            */
/*-------------------------------------------------------------------*/

static void
ffe_mpeg12_prepare_frame(AVCodecContext *avctx, MpegEncContext *s, AVPacket *avpkt)
{
    if ( s->avctx->ffedit_import != 0 )
    {
        memcpy(s->ffedit_sd, avpkt->ffedit_sd, sizeof(json_t *)*FFEDIT_FEAT_LAST);
        s->jctx = avpkt->jctx;
    }
}

static void
ffe_mpeg12_init(MpegEncContext *s)
{
    AVFrame *f = s->current_picture_ptr->f;

    ffe_mpegvideo_jctx_init(s);
    if ( s->avctx->ffedit_import != 0 )
        memcpy(f->ffedit_sd, s->ffedit_sd, sizeof(json_t *)*FFEDIT_FEAT_LAST);

    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_INFO)) != 0 )
        ffe_mpeg12_export_info_init(s, f);
}

static void
ffe_mpeg12_export_cleanup(MpegEncContext *s, AVFrame *f)
{
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_INFO)) != 0 )
        ffe_mpeg12_export_info_cleanup(s, f);
}
