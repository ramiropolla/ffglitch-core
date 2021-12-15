
#ifndef AVCODEC_FFEDIT_MB_H
#define AVCODEC_FFEDIT_MB_H

#include "libavutil/json.h"

#include "avcodec.h"
#include "ffedit_xp.h"
#include "put_bits.h"
#include "get_bits.h"

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

void ffe_mb_export_init(
        json_ctx_t *jctx,
        AVFrame *f,
        int mb_height,
        int mb_width);
void ffe_mb_export_cleanup(json_ctx_t *jctx, AVFrame *f);
void ffe_mb_import_init(json_ctx_t *jctx, AVFrame *f);

void ffe_mb_export_init_mb(
        ffe_mb_mb_ctx *mbctx,
        GetBitContext *gb);
void ffe_mb_export_flush_mb(
        ffe_mb_mb_ctx *mbctx,
        json_ctx_t *jctx,
        AVFrame *f,
        GetBitContext *gb,
        int mb_y,
        int mb_x);
void ffe_mb_import_init_mb(
        ffe_mb_mb_ctx *mbctx,
        AVFrame *f,
        GetBitContext *gb,
        FFEditTransplicateContext *xp,
        int mb_y,
        int mb_x);
void ffe_mb_import_flush_mb(
        ffe_mb_mb_ctx *mbctx,
        AVFrame *f,
        GetBitContext *gb,
        FFEditTransplicateContext *xp,
        int mb_y,
        int mb_x);

#endif /* AVCODEC_FFEDIT_MB_H */
