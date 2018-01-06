
#ifndef AVCODEC_FFEDIT_MV_H
#define AVCODEC_FFEDIT_MV_H

#include "libavutil/json.h"

#include "avcodec.h"
#include "put_bits.h"
#include "get_bits.h"

typedef struct
{
    /*
     * 0: forward
     * 1: backward
     */
    json_t *data[2];
    size_t count[2];
    json_t *jmb[2];
    json_t *cur;
    size_t *pcount;

    size_t nb_blocks;
} ffe_mv_ctx;

void ffe_mv_export_init_mb(
        json_ctx_t *jctx,
        AVFrame *f,
        int mb_y,
        int mb_x,
        int nb_directions,
        int nb_blocks);
void ffe_mv_import_init_mb(
        json_ctx_t *jctx,
        AVFrame *f,
        int mb_y,
        int mb_x,
        int nb_directions,
        int nb_blocks);
void ffe_mv_select(
        AVFrame *f,
        int direction,
        int blockn);
int ffe_mv_get(
        AVFrame *f,
        int x_or_y);
void ffe_mv_set(
        AVFrame *f,
        int x_or_y,
        int val);
void ffe_mv_export_init(
        json_ctx_t *jctx,
        AVFrame *f,
        int mb_height,
        int mb_width);
void ffe_mv_export_cleanup(json_ctx_t *jctx, AVFrame *f);
void ffe_mv_import_init(json_ctx_t *jctx, AVFrame *f);

#endif /* AVCODEC_FFEDIT_MV_H */
