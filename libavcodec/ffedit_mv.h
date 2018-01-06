/*
 * Copyright (c) 2017-2022 Ramiro Polla
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVCODEC_FFEDIT_MV_H
#define AVCODEC_FFEDIT_MV_H

#include "libavutil/json.h"

#include "avcodec.h"

// #define WIP_MV_2DARRAY

#define MV_OVERFLOW_ASSERT   0
#define MV_OVERFLOW_TRUNCATE 1
#define MV_OVERFLOW_IGNORE   2
#define MV_OVERFLOW_WARN     3
typedef struct
{
    /*
     * 0: forward
     * 1: backward
     * 2: direct (MPEG-4)
     */
    json_t *data[3];
    int import_is_mv2darray[3];
    json_t *fcode;
    json_t *bcode;
    json_t *overflow;
    int used[3];
    int overflow_action;
} ffe_mv_ctx;

typedef struct
{
    /* import */
    json_t *jmb[3];
    json_t *cur_import;
    size_t nb_blocks;

    /* export */
    int32_t *cur_export;
    int mb_y;
    int mb_x;

    int *pused;

    int overflow_action;
} ffe_mv_mb_ctx;

void ffe_mv_export_init_mb(
        ffe_mv_mb_ctx *ctx,
        AVFrame *f,
        int mb_y,
        int mb_x,
        int nb_directions,
        int nb_blocks);
void ffe_mv_import_init_mb(
        ffe_mv_mb_ctx *ctx,
        AVFrame *f,
        int mb_y,
        int mb_x,
        int nb_directions,
        int nb_blocks);
void ffe_mv_export_select(
        ffe_mv_mb_ctx *mbctx,
        AVFrame *f,
        int direction,
        int blockn);
void ffe_mv_import_select(
        ffe_mv_mb_ctx *mbctx,
        AVFrame *f,
        int direction,
        int blockn);
int32_t ffe_mv_get(
        ffe_mv_mb_ctx *mbctx,
        int x_or_y);
void ffe_mv_set(
        ffe_mv_mb_ctx *mbctx,
        int x_or_y,
        int32_t val);
void ffe_mv_export_init(
        json_ctx_t *jctx,
        AVFrame *f,
        int mb_height,
        int mb_width,
        int nb_fcodes,
        int max_nb_blocks);
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
        AVFrame *f,
        int mb_y,
        int mb_x,
        int nb_directions,
        int nb_blocks);
void ffe_mv_delta_import_init_mb(
        ffe_mv_mb_ctx *ctx,
        AVFrame *f,
        int mb_y,
        int mb_x,
        int nb_directions,
        int nb_blocks);
void ffe_mv_delta_export_select(
        ffe_mv_mb_ctx *mbctx,
        AVFrame *f,
        int direction,
        int blockn);
void ffe_mv_delta_import_select(
        ffe_mv_mb_ctx *mbctx,
        AVFrame *f,
        int direction,
        int blockn);
int32_t ffe_mv_delta_get(
        ffe_mv_mb_ctx *mbctx,
        int x_or_y);
void ffe_mv_delta_set(
        ffe_mv_mb_ctx *mbctx,
        int x_or_y,
        int32_t val);
void ffe_mv_delta_export_init(
        json_ctx_t *jctx,
        AVFrame *f,
        int mb_height,
        int mb_width,
        int nb_fcodes,
        int max_nb_blocks);
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
