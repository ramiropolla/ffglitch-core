
#ifndef AVCODEC_FFEDIT_MV_H
#define AVCODEC_FFEDIT_MV_H

#include <stdatomic.h>

#include "libavutil/json.h"

#include "avcodec.h"
#include "put_bits.h"
#include "get_bits.h"

#define MV_OVERFLOW_ASSERT   0
#define MV_OVERFLOW_TRUNCATE 1
#define MV_OVERFLOW_IGNORE   2
#define MV_OVERFLOW_WARN     3
typedef struct
{
    /*
     * 0: forward
     * 1: backward
     */
    json_t *data[2];
    json_t *fcode;
    json_t *bcode;
    json_t *overflow;
    atomic_size_t count[2];
    int overflow_action;
} ffe_mv_ctx;

typedef struct
{
    json_t *jmb[2];
    json_t *cur;
    atomic_size_t *pcount;

    size_t nb_blocks;
    int overflow_action;
} ffe_mv_mb_ctx;

void ffe_mv_export_init_mb(
        ffe_mv_mb_ctx *ctx,
        json_ctx_t *jctx,
        AVFrame *f,
        int mb_y,
        int mb_x,
        int nb_directions,
        int nb_blocks);
void ffe_mv_import_init_mb(
        ffe_mv_mb_ctx *ctx,
        json_ctx_t *jctx,
        AVFrame *f,
        int mb_y,
        int mb_x,
        int nb_directions,
        int nb_blocks);
void ffe_mv_select(
        ffe_mv_mb_ctx *mbctx,
        AVFrame *f,
        int direction,
        int blockn);
int ffe_mv_get(
        ffe_mv_mb_ctx *mbctx,
        AVFrame *f,
        int x_or_y);
void ffe_mv_set(
        ffe_mv_mb_ctx *mbctx,
        json_ctx_t *jctx,
        int x_or_y,
        int val);
void ffe_mv_export_init(
        json_ctx_t *jctx,
        AVFrame *f,
        int mb_height,
        int mb_width,
        int nb_fcodes);
void ffe_mv_export_fcode(
        json_ctx_t *jctx,
        AVFrame *f,
        int f_or_b,
        int num,
        int fcode);
int ffe_mv_overflow(
        ffe_mv_mb_ctx *mbctx,
        int pred,
        int val,
        int fcode,
        int shift);
void ffe_mv_export_cleanup(json_ctx_t *jctx, AVFrame *f);
void ffe_mv_import_init(json_ctx_t *jctx, AVFrame *f);

void ffe_mv_delta_export_init_mb(
        ffe_mv_mb_ctx *ctx,
        json_ctx_t *jctx,
        AVFrame *f,
        int mb_y,
        int mb_x,
        int nb_directions,
        int nb_blocks);
void ffe_mv_delta_import_init_mb(
        ffe_mv_mb_ctx *ctx,
        json_ctx_t *jctx,
        AVFrame *f,
        int mb_y,
        int mb_x,
        int nb_directions,
        int nb_blocks);
void ffe_mv_delta_select(
        ffe_mv_mb_ctx *mbctx,
        AVFrame *f,
        int direction,
        int blockn);
int ffe_mv_delta_get(
        ffe_mv_mb_ctx *mbctx,
        AVFrame *f,
        int x_or_y);
void ffe_mv_delta_set(
        ffe_mv_mb_ctx *mbctx,
        json_ctx_t *jctx,
        int x_or_y,
        int val);
void ffe_mv_delta_export_init(
        json_ctx_t *jctx,
        AVFrame *f,
        int mb_height,
        int mb_width,
        int nb_fcodes);
void ffe_mv_delta_export_fcode(
        json_ctx_t *jctx,
        AVFrame *f,
        int f_or_b,
        int num,
        int fcode);
int ffe_mv_delta_overflow(
        ffe_mv_mb_ctx *mbctx,
        int delta,
        int fcode,
        int shift);
void ffe_mv_delta_export_cleanup(json_ctx_t *jctx, AVFrame *f);
void ffe_mv_delta_import_init(json_ctx_t *jctx, AVFrame *f);

#endif /* AVCODEC_FFEDIT_MV_H */
