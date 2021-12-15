/*
 * Copyright (C) 2022 Ramiro Polla
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

#include "libavutil/attributes.h"
#include "libavutil/cpu.h"
#include "libavutil/x86/asm.h"
#include "libavutil/x86/cpu.h"
#include "libavfilter/pixelsort.h"

void ff_rgb_to_hsv_001_sse2(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsv_010_sse2(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsv_011_sse2(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsv_100_sse2(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsv_101_sse2(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsv_110_sse2(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
static rgb_to_hsvl_func rgb_to_hsv_sse2[2][2][2] =
{
    { { NULL,                   ff_rgb_to_hsv_001_sse2 },
      { ff_rgb_to_hsv_010_sse2, ff_rgb_to_hsv_011_sse2 }, },
    { { ff_rgb_to_hsv_100_sse2, ff_rgb_to_hsv_101_sse2 },
      { ff_rgb_to_hsv_110_sse2, NULL                   }, },
};

void ff_rgb_to_hsv_001_sse4(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsv_010_sse4(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsv_011_sse4(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsv_100_sse4(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsv_101_sse4(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsv_110_sse4(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
static rgb_to_hsvl_func rgb_to_hsv_sse4[2][2][2] =
{
    { { NULL,                   ff_rgb_to_hsv_001_sse4 },
      { ff_rgb_to_hsv_010_sse4, ff_rgb_to_hsv_011_sse4 }, },
    { { ff_rgb_to_hsv_100_sse4, ff_rgb_to_hsv_101_sse4 },
      { ff_rgb_to_hsv_110_sse4, NULL                   }, },
};

void ff_rgb_to_hsv_001_avx(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsv_010_avx(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsv_011_avx(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsv_100_avx(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsv_101_avx(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsv_110_avx(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
static rgb_to_hsvl_func rgb_to_hsv_avx[2][2][2] =
{
    { { NULL,                  ff_rgb_to_hsv_001_avx },
      { ff_rgb_to_hsv_010_avx, ff_rgb_to_hsv_011_avx }, },
    { { ff_rgb_to_hsv_100_avx, ff_rgb_to_hsv_101_avx },
      { ff_rgb_to_hsv_110_avx, NULL                  }, },
};

void ff_rgb_to_hsv_001_avx2(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsv_010_avx2(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsv_011_avx2(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsv_100_avx2(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsv_101_avx2(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsv_110_avx2(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
static rgb_to_hsvl_func rgb_to_hsv_avx2[2][2][2] =
{
    { { NULL,                   ff_rgb_to_hsv_001_avx2 },
      { ff_rgb_to_hsv_010_avx2, ff_rgb_to_hsv_011_avx2 }, },
    { { ff_rgb_to_hsv_100_avx2, ff_rgb_to_hsv_101_avx2 },
      { ff_rgb_to_hsv_110_avx2, NULL                   }, },
};

void ff_rgb_to_hsl_001_sse2(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsl_010_sse2(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsl_011_sse2(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsl_100_sse2(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsl_101_sse2(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsl_110_sse2(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
static rgb_to_hsvl_func rgb_to_hsl_sse2[2][2][2] =
{
    { { NULL,                   ff_rgb_to_hsl_001_sse2 },
      { ff_rgb_to_hsl_010_sse2, ff_rgb_to_hsl_011_sse2 }, },
    { { ff_rgb_to_hsl_100_sse2, ff_rgb_to_hsl_101_sse2 },
      { ff_rgb_to_hsl_110_sse2, NULL                   }, },
};

void ff_rgb_to_hsl_001_sse4(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsl_010_sse4(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsl_011_sse4(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsl_100_sse4(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsl_101_sse4(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsl_110_sse4(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
static rgb_to_hsvl_func rgb_to_hsl_sse4[2][2][2] =
{
    { { NULL,                   ff_rgb_to_hsl_001_sse4 },
      { ff_rgb_to_hsl_010_sse4, ff_rgb_to_hsl_011_sse4 }, },
    { { ff_rgb_to_hsl_100_sse4, ff_rgb_to_hsl_101_sse4 },
      { ff_rgb_to_hsl_110_sse4, NULL                   }, },
};

void ff_rgb_to_hsl_001_avx(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsl_010_avx(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsl_011_avx(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsl_100_avx(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsl_101_avx(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsl_110_avx(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
static rgb_to_hsvl_func rgb_to_hsl_avx[2][2][2] =
{
    { { NULL,                  ff_rgb_to_hsl_001_avx },
      { ff_rgb_to_hsl_010_avx, ff_rgb_to_hsl_011_avx }, },
    { { ff_rgb_to_hsl_100_avx, ff_rgb_to_hsl_101_avx },
      { ff_rgb_to_hsl_110_avx, NULL                  }, },
};

void ff_rgb_to_hsl_001_avx2(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsl_010_avx2(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsl_011_avx2(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsl_100_avx2(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsl_101_avx2(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
void ff_rgb_to_hsl_110_avx2(struct hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int);
static rgb_to_hsvl_func rgb_to_hsl_avx2[2][2][2] =
{
    { { NULL,                   ff_rgb_to_hsl_001_avx2 },
      { ff_rgb_to_hsl_010_avx2, ff_rgb_to_hsl_011_avx2 }, },
    { { ff_rgb_to_hsl_100_avx2, ff_rgb_to_hsl_101_avx2 },
      { ff_rgb_to_hsl_110_avx2, NULL                   }, },
};

av_cold void ff_pixelsort_init_x86(PixelsortVtable *v, int is_hsv, int do_h, int do_s, int do_vl)
{
    int cpu_flags = av_get_cpu_flags();

    if ( is_hsv )
    {
        if      ( EXTERNAL_AVX2(cpu_flags) )
            v->rgb_to_hsvl = rgb_to_hsv_avx2[do_h][do_s][do_vl];
        else if ( EXTERNAL_AVX(cpu_flags) )
            v->rgb_to_hsvl = rgb_to_hsv_avx[do_h][do_s][do_vl];
        else if ( EXTERNAL_SSE4(cpu_flags) )
            v->rgb_to_hsvl = rgb_to_hsv_sse4[do_h][do_s][do_vl];
        else if ( EXTERNAL_SSE2(cpu_flags) )
            v->rgb_to_hsvl = rgb_to_hsv_sse2[do_h][do_s][do_vl];
    }
    else
    {
        if      ( EXTERNAL_AVX2(cpu_flags) )
            v->rgb_to_hsvl = rgb_to_hsl_avx2[do_h][do_s][do_vl];
        else if ( EXTERNAL_AVX(cpu_flags) )
            v->rgb_to_hsvl = rgb_to_hsl_avx[do_h][do_s][do_vl];
        else if ( EXTERNAL_SSE4(cpu_flags) )
            v->rgb_to_hsvl = rgb_to_hsl_sse4[do_h][do_s][do_vl];
        else if ( EXTERNAL_SSE2(cpu_flags) )
            v->rgb_to_hsvl = rgb_to_hsl_sse2[do_h][do_s][do_vl];
    }
}
