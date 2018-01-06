
#ifndef AVCODEC_CAS9_MV_H
#define AVCODEC_CAS9_MV_H

#include <json.h>

#include "avcodec.h"
#include "put_bits.h"
#include "get_bits.h"

typedef struct
{
    /*
     * 0: forward
     * 1: backward
     */
    json_object *data[2];
    size_t count[2];
    json_object *jmb[2];
    json_object *cur;
    size_t *pcount;

    size_t nb_blocks;
} cas9_mv_ctx;

void cas9_mv_export_init_mb(
        AVFrame *f,
        int mb_y,
        int mb_x,
        int nb_directions,
        int nb_blocks);
void cas9_mv_import_init_mb(
        AVFrame *f,
        int mb_y,
        int mb_x,
        int nb_directions,
        int nb_blocks);
void cas9_mv_select(
        AVFrame *f,
        int direction,
        int blockn);
int cas9_mv_get(
        AVFrame *f,
        int x_or_y);
void cas9_mv_set(
        AVFrame *f,
        int x_or_y,
        int val);
void cas9_mv_export_init(
        AVFrame *f,
        int mb_height,
        int mb_width);
void cas9_mv_export_cleanup(AVFrame *f);
void cas9_mv_import_init(AVFrame *f);

#endif /* AVCODEC_CAS9_MV_H */
