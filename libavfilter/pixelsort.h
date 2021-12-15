/*
 * Copyright (C) 2021-2022 Ramiro Polla
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
#ifndef AVFILTER_PIXELSORT_H
#define AVFILTER_PIXELSORT_H

#include <pthread.h>

#include "internal.h"

/*********************************************************************/
struct hsvl_base_t {
    float h;            // [ 0., 1. )
    float s;            // [ 0., 1. )
    union {
        uint32_t v;     // [ 0, 255 ]
        uint32_t _2l;   // [ 0, 510 ]
    };
    uint32_t idx;
};

typedef void (*rgb_to_hsvl_func)(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);

extern rgb_to_hsvl_func ff_rgb_to_hsv_c[2][2][2];
extern rgb_to_hsvl_func ff_rgb_to_hsl_c[2][2][2];

typedef void (*pixelsort_internal_hsvl_func)(uint8_t *, uint8_t *, uint8_t *, uint8_t *, struct hsvl_base_t *, int, int);
typedef void (*pixelsort_internal_uint8_func)(uint8_t *, uint8_t *, uint8_t *, uint8_t *, int, int);

typedef struct PixelsortVtable {
    rgb_to_hsvl_func rgb_to_hsvl;
    pixelsort_internal_hsvl_func pixelsort_internal_hsvl;
    pixelsort_internal_uint8_func pixelsort_internal_uint8;
} PixelsortVtable;

void ff_pixelsort_init_x86(PixelsortVtable *v, int is_hsv, int do_h, int do_s, int do_vl);

/*********************************************************************/
typedef struct PixelSortThreadData {
    PixelsortVtable vtable;
    int mode_is_threshold;
    int mode_is_random;
    int reverse_sort;
    int sort_colorspace_is_yuv;
    int sort_colorspace_is_rgb;
    int sort_colorspace_is_hsv;
    int sort_colorspace_is_hsl;
    int start_y;
    int end_y;
    int start_x;
    int end_x;
    uint8_t *src_0;
    uint8_t *src_1;
    uint8_t *src_2;
    int linesizes[3];
    float lower_threshold;
    float upper_threshold;
    int reverse_range;
    int clength;
    int sort_by_n;
    int trigger_by_n;
    int cur_y;
    pthread_mutex_t mutex;
} PixelSortThreadData;

int ff_pixelsort_slice(AVFilterContext *ctx, void *arg, int jobnr, int nb_jobs);
void ff_pixelsort_mutex_init(PixelSortThreadData *td);
void ff_pixelsort_mutex_destroy(PixelSortThreadData *td);

#endif
