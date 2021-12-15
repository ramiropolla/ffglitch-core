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

#include <algorithm>

#include <pthread.h>

extern "C" {
#include "libavutil/lfg.h"
#include "libavutil/random_seed.h"
#include "pixelsort.h"
#include "internal.h"
}

/*********************************************************************/
void ff_pixelsort_mutex_init(PixelSortThreadData *td)
{
    pthread_mutex_init(&td->mutex, NULL);
}

static bool pixelsort_get_range(PixelSortThreadData *td, int *_start_y, int *_end_y)
{
    bool ret = false;
    pthread_mutex_lock(&td->mutex);
    int start_y = td->cur_y;
    int end_y = td->end_y;
    if ( start_y != end_y )
    {
        end_y = FFMIN(end_y, start_y + 32);
        *_start_y = start_y;
        *_end_y = end_y;
        td->cur_y = end_y;
        ret = true;
    }
    pthread_mutex_unlock(&td->mutex);
    return ret;
}

void ff_pixelsort_mutex_destroy(PixelSortThreadData *td)
{
    pthread_mutex_destroy(&td->mutex);
}

/*********************************************************************/
/* counting sort */

static av_always_inline void
calculate_histogram(uint16_t *histogram, uint8_t *src, int length)
{
    uint16_t sub_histograms[4][0x100];
    ssize_t i = 0;

    /* for some reason it's faster to keep the memsets together */
    memset(sub_histograms, 0x00, 4 * 0x100 * sizeof(uint16_t));
    memset(histogram, 0x00, 0x100 * sizeof(uint16_t));

    /* histograms are almost impossible to vectorize. the best I could
     * do was use sub-histograms to overlap loads/stores */
    for ( ; i < length - 4; i += 4 )
    {
        sub_histograms[0][src[i + 0]]++;
        sub_histograms[1][src[i + 1]]++;
        sub_histograms[2][src[i + 2]]++;
        sub_histograms[3][src[i + 3]]++;
    }
    for ( ; i < length; i++ )
        sub_histograms[0][src[i]]++;

    /* merge sub-histograms into output histogram */
#if ARCH_X86_64
    __asm__ volatile(
        "xor %[i], %[i]\n\t"

        "1:\n\t"
        "movdqa (%[in0],%[i],2), %%xmm0\n\t"
        "movdqa (%[in1],%[i],2), %%xmm1\n\t"
        "movdqa (%[in2],%[i],2), %%xmm2\n\t"
        "movdqa (%[in3],%[i],2), %%xmm3\n\t"
        "paddw %%xmm1, %%xmm0\n\t"
        "paddw %%xmm3, %%xmm2\n\t"
        "paddw %%xmm2, %%xmm0\n\t"
        "movdqa %%xmm0, (%[out],%[i],2)\n\t"

        "add $8, %[i]\n\t"

        "cmp $0x100, %[i]\n\t"
        "jb 1b\n\t"
        :: [i]"r"(i),
           [out]"r"(histogram),
           [in0]"r"(sub_histograms[0]),
           [in1]"r"(sub_histograms[1]),
           [in2]"r"(sub_histograms[2]),
           [in3]"r"(sub_histograms[3])
        : "memory"
    );
#else
    for ( i = 0; i < 0x100; i++ )
    {
        histogram[i] = sub_histograms[0][i]
                     + sub_histograms[1][i]
                     + sub_histograms[2][i]
                     + sub_histograms[3][i];
    }
#endif
}

template<int histogram_size, bool reverse_sort>
void calculate_dst_pointers(uint8_t **ptrs_0, uint8_t **ptrs_1, uint8_t **ptrs_2, uint16_t *histogram, uint8_t *dst_0, uint8_t *dst_1, uint8_t *dst_2, int length)
{
    if ( reverse_sort )
    {
        dst_0 += length - 1;
        dst_1 += length - 1;
        dst_2 += length - 1;
    }
    for ( int i = 0; i < histogram_size; i++ )
    {
        uint16_t count = histogram[i];
        if ( count == 0 )
            continue;
        ptrs_0[i] = dst_0;
        ptrs_1[i] = dst_1;
        ptrs_2[i] = dst_2;
        if ( reverse_sort )
        {
            dst_0 -= count;
            dst_1 -= count;
            dst_2 -= count;
        }
        else
        {
            dst_0 += count;
            dst_1 += count;
            dst_2 += count;
        }
    }
}

template<int sort_by_n, bool reverse_sort>
void sort_to_tmp(uint8_t **ptrs_0, uint8_t **ptrs_1, uint8_t **ptrs_2, uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, int length)
{
    for ( int i = 0; i < length; i++ )
    {
        uint8_t val[3];
        __builtin_prefetch(&src_0[i]);
        __builtin_prefetch(&src_1[i]);
        __builtin_prefetch(&src_2[i]);
        val[0] = src_0[i];
        val[1] = src_1[i];
        val[2] = src_2[i];
        __builtin_prefetch(ptrs_0[val[sort_by_n]], 1);
        __builtin_prefetch(ptrs_1[val[sort_by_n]], 1);
        __builtin_prefetch(ptrs_2[val[sort_by_n]], 1);
        if ( reverse_sort )
        {
            *ptrs_0[val[sort_by_n]]-- = val[0];
            *ptrs_1[val[sort_by_n]]-- = val[1];
            *ptrs_2[val[sort_by_n]]-- = val[2];
        }
        else
        {
            *ptrs_0[val[sort_by_n]]++ = val[0];
            *ptrs_1[val[sort_by_n]]++ = val[1];
            *ptrs_2[val[sort_by_n]]++ = val[2];
        }
    }
}

static av_always_inline void
copy_tmp_to_src(uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, uint8_t *tmp_0, uint8_t *tmp_1, uint8_t *tmp_2, int length)
{
    memcpy(src_0, tmp_0, length);
    memcpy(src_1, tmp_1, length);
    memcpy(src_2, tmp_2, length);
}

template<int sort_by_n, bool reverse_sort>
void counting_sort_uint8(uint8_t *tmp_out, uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, int length)
{
    uint8_t *tmp_0 = tmp_out;
    uint8_t *tmp_1 = tmp_0 + length;
    uint8_t *tmp_2 = tmp_1 + length;
    uint16_t histogram[0x100];
    uint8_t *ptrs_0[0x100];
    uint8_t *ptrs_1[0x100];
    uint8_t *ptrs_2[0x100];

    switch ( sort_by_n )
    {
    case 0: calculate_histogram(histogram, src_0, length); break;
    case 1: calculate_histogram(histogram, src_1, length); break;
    case 2: calculate_histogram(histogram, src_2, length); break;
    }
    calculate_dst_pointers<0x100, reverse_sort>(ptrs_0, ptrs_1, ptrs_2, histogram, tmp_0, tmp_1, tmp_2, length);
    sort_to_tmp<sort_by_n, reverse_sort>(ptrs_0, ptrs_1, ptrs_2, src_0, src_1, src_2, length);
    copy_tmp_to_src(src_0, src_1, src_2, tmp_0, tmp_1, tmp_2, length);
}

/*********************************************************************/
/* counting sort for hsvl */

template<int histogram_size>
void calculate_histogram_hsvl(uint16_t *histogram, hsvl_base_t *hsvl, int length)
{
    memset(histogram, 0x00, histogram_size * sizeof(uint16_t));
    for ( int i = 0; i < length; i++ )
        histogram[hsvl[i].v]++;
}

template<bool reverse_sort>
void sort_to_tmp_hsvl(uint8_t **ptrs_r, uint8_t **ptrs_g, uint8_t **ptrs_b, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, hsvl_base_t *hsvl, int length)
{
    for ( int i = 0; i < length; i++ )
    {
        uint16_t val;
        __builtin_prefetch(&hsvl[i]);
        val = hsvl[i].v;
        __builtin_prefetch(&src_r[i]);
        __builtin_prefetch(&src_g[i]);
        __builtin_prefetch(&src_b[i]);
        __builtin_prefetch(ptrs_r[val], 1);
        __builtin_prefetch(ptrs_g[val], 1);
        __builtin_prefetch(ptrs_b[val], 1);
        if ( reverse_sort )
        {
            *ptrs_r[val]-- = src_r[i];
            *ptrs_g[val]-- = src_g[i];
            *ptrs_b[val]-- = src_b[i];
        }
        else
        {
            *ptrs_r[val]++ = src_r[i];
            *ptrs_g[val]++ = src_g[i];
            *ptrs_b[val]++ = src_b[i];
        }
    }
}

template<int histogram_size, bool reverse_sort>
void counting_sort_hsvl(uint8_t *tmp_out, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, hsvl_base_t *hsvl, int length)
{
    uint8_t *tmp_0 = tmp_out;
    uint8_t *tmp_1 = tmp_0 + length;
    uint8_t *tmp_2 = tmp_1 + length;
    uint16_t histogram[histogram_size];
    uint8_t *ptrs_r[histogram_size];
    uint8_t *ptrs_g[histogram_size];
    uint8_t *ptrs_b[histogram_size];

    calculate_histogram_hsvl<histogram_size>(histogram, hsvl, length);
    calculate_dst_pointers<histogram_size, reverse_sort>(ptrs_r, ptrs_g, ptrs_b, histogram, tmp_0, tmp_1, tmp_2, length);
    sort_to_tmp_hsvl<reverse_sort>(ptrs_r, ptrs_g, ptrs_b, src_r, src_g, src_b, hsvl, length);
    copy_tmp_to_src(src_r, src_g, src_b, tmp_0, tmp_1, tmp_2, length);
}

/*********************************************************************/
template<int val_n, bool reverse_sort>
struct hsvl_t : public hsvl_base_t {
    bool operator < (const hsvl_t &b) const
    {
        if ( reverse_sort )
        {
            return (val_n == 0) ? (h >= b.h)
                 : (val_n == 1) ? (s >= b.s)
                 :                (v >= b.v);
        }
        else
        {
            return (val_n == 0) ? (h < b.h)
                 : (val_n == 1) ? (s < b.s)
                 :                (v < b.v);
        }
    }
    template<typename T>
    bool is_in_range(T lower, T upper)
    {
        return (val_n == 0) ? (h >= lower && h <= upper)
             : (val_n == 1) ? (s >= lower && s <= upper)
             :                (v >= lower && v <= upper);
    }
};

static av_always_inline void
copy_sorted_hsvl_to_tmp(uint8_t *tmp_0, uint8_t *tmp_1, uint8_t *tmp_2, hsvl_base_t *hsvl, uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, int length)
{
    for ( int i = 0; i < length; i++ )
    {
        uint16_t idx = hsvl[i].idx;
        tmp_0[i] = src_0[idx];
        tmp_1[i] = src_1[idx];
        tmp_2[i] = src_2[idx];
    }
}

template<int sort_by_n, bool reverse_sort>
void stable_sort_hsvl(uint8_t *tmp_out, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, hsvl_base_t *_hsvl, int start, int end)
{
    hsvl_t<sort_by_n, reverse_sort> *hsvl = (hsvl_t<sort_by_n, reverse_sort> *) _hsvl;
    const int length = end - start;

    std::stable_sort(hsvl, hsvl + length);

    uint8_t *tmp_0 = tmp_out;
    uint8_t *tmp_1 = tmp_0 + length;
    uint8_t *tmp_2 = tmp_1 + length;
    copy_sorted_hsvl_to_tmp(tmp_0, tmp_1, tmp_2, hsvl, src_r - start, src_g - start, src_b - start, length);
    copy_tmp_to_src(src_r, src_g, src_b, tmp_0, tmp_1, tmp_2, length);
}

template<int sort_by_n, int histogram_size, bool reverse_sort>
void pixelsort_internal_hsvl_template(uint8_t *tmp_out, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, hsvl_base_t *hsvl, int start, int end)
{
    const int length = end - start;

    /* already sorted */
    if ( length == 1 )
        return;

    src_r += start;
    src_g += start;
    src_b += start;
    hsvl += start;

    /* Counting sort works only for integers (8-bit v in hsv or 9-bit
     * l in hsvl), and is only used for lengths greater than some magic
     * value determined empirically. */
    if ( sort_by_n == 2 && length >= 32 )
        counting_sort_hsvl<histogram_size, reverse_sort>(tmp_out, src_r, src_g, src_b, hsvl, length);
    else
        stable_sort_hsvl<sort_by_n, reverse_sort>(tmp_out, src_r, src_g, src_b, hsvl, start, end);
}

static void pixelsort_internal_hsvl_0_100(uint8_t *tmp_out, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, hsvl_base_t *hsvl, int start, int end) { pixelsort_internal_hsvl_template<0, 0x100, false>(tmp_out, src_r, src_g, src_b, hsvl, start, end); }
static void pixelsort_internal_hsvl_1_100(uint8_t *tmp_out, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, hsvl_base_t *hsvl, int start, int end) { pixelsort_internal_hsvl_template<1, 0x100, false>(tmp_out, src_r, src_g, src_b, hsvl, start, end); }
static void pixelsort_internal_hsvl_2_100(uint8_t *tmp_out, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, hsvl_base_t *hsvl, int start, int end) { pixelsort_internal_hsvl_template<2, 0x100, false>(tmp_out, src_r, src_g, src_b, hsvl, start, end); }
static void pixelsort_internal_hsvl_0_200(uint8_t *tmp_out, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, hsvl_base_t *hsvl, int start, int end) { pixelsort_internal_hsvl_template<0, 0x200, false>(tmp_out, src_r, src_g, src_b, hsvl, start, end); }
static void pixelsort_internal_hsvl_1_200(uint8_t *tmp_out, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, hsvl_base_t *hsvl, int start, int end) { pixelsort_internal_hsvl_template<1, 0x200, false>(tmp_out, src_r, src_g, src_b, hsvl, start, end); }
static void pixelsort_internal_hsvl_2_200(uint8_t *tmp_out, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, hsvl_base_t *hsvl, int start, int end) { pixelsort_internal_hsvl_template<2, 0x200, false>(tmp_out, src_r, src_g, src_b, hsvl, start, end); }
static void pixelsort_internal_hsvl_0_100_reverse_sort(uint8_t *tmp_out, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, hsvl_base_t *hsvl, int start, int end) { pixelsort_internal_hsvl_template<0, 0x100, true>(tmp_out, src_r, src_g, src_b, hsvl, start, end); }
static void pixelsort_internal_hsvl_1_100_reverse_sort(uint8_t *tmp_out, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, hsvl_base_t *hsvl, int start, int end) { pixelsort_internal_hsvl_template<1, 0x100, true>(tmp_out, src_r, src_g, src_b, hsvl, start, end); }
static void pixelsort_internal_hsvl_2_100_reverse_sort(uint8_t *tmp_out, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, hsvl_base_t *hsvl, int start, int end) { pixelsort_internal_hsvl_template<2, 0x100, true>(tmp_out, src_r, src_g, src_b, hsvl, start, end); }
static void pixelsort_internal_hsvl_0_200_reverse_sort(uint8_t *tmp_out, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, hsvl_base_t *hsvl, int start, int end) { pixelsort_internal_hsvl_template<0, 0x200, true>(tmp_out, src_r, src_g, src_b, hsvl, start, end); }
static void pixelsort_internal_hsvl_1_200_reverse_sort(uint8_t *tmp_out, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, hsvl_base_t *hsvl, int start, int end) { pixelsort_internal_hsvl_template<1, 0x200, true>(tmp_out, src_r, src_g, src_b, hsvl, start, end); }
static void pixelsort_internal_hsvl_2_200_reverse_sort(uint8_t *tmp_out, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, hsvl_base_t *hsvl, int start, int end) { pixelsort_internal_hsvl_template<2, 0x200, true>(tmp_out, src_r, src_g, src_b, hsvl, start, end); }
// template<int sort_by_n, int histogram_size, bool reverse_sort>
static pixelsort_internal_hsvl_func pixelsort_internal_hsvl_funcs[3][2][2] =
{
    { { pixelsort_internal_hsvl_0_100, pixelsort_internal_hsvl_0_100_reverse_sort },
      { pixelsort_internal_hsvl_0_200, pixelsort_internal_hsvl_0_200_reverse_sort }, },
    { { pixelsort_internal_hsvl_1_100, pixelsort_internal_hsvl_1_100_reverse_sort },
      { pixelsort_internal_hsvl_1_200, pixelsort_internal_hsvl_1_200_reverse_sort }, },
    { { pixelsort_internal_hsvl_2_100, pixelsort_internal_hsvl_2_100_reverse_sort },
      { pixelsort_internal_hsvl_2_200, pixelsort_internal_hsvl_2_200_reverse_sort }, },
};

static av_always_inline void
copy_src_to_tmp_packed(uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, uint8_t *tmp_out, int length)
{
    ssize_t i = 0;
#if ARCH_X86_64
    ssize_t length_7 = length - 7;
    __asm__ volatile(
        "xor %[i], %[i]\n\t"
        "test %[length_7], %[length_7]\n\t"
        "jle 2f\n\t"

        "1:\n\t"
        "movq (%[src_0],%[i]), %%xmm0\n\t" // xmm0 = Y0 Y1 Y2 Y3 Y4 Y5 Y6 Y7 00 00 00 00 00 00 00 00
        "movq (%[src_1],%[i]), %%xmm1\n\t" // xmm1 = U0 U1 U2 U3 U4 U5 U6 U7 00 00 00 00 00 00 00 00
        "movq (%[src_2],%[i]), %%xmm2\n\t" // xmm2 = V0 V1 V2 V3 V4 V5 V6 V7 00 00 00 00 00 00 00 00
        "add $8, %[i]\n\t"

        "punpcklbw %%xmm1, %%xmm0\n\t" // xmm0 = Y0 U0 Y1 U1 Y2 U2 Y3 U3 Y4 U4 Y5 U5 Y6 U6 Y7 U7
        "punpcklbw %%xmm3, %%xmm2\n\t" // xmm2 = V0 XX V1 XX V2 XX V3 XX V4 XX V5 XX V6 XX V7 XX

        "movdqa    %%xmm0, %%xmm1\n\t" // xmm1 = Y0 U0 Y1 U1 Y2 U2 Y3 U3 Y4 U4 Y5 U5 Y6 U6 Y7 U7
        "movdqa    %%xmm2, %%xmm3\n\t" // xmm3 = V0 XX V1 XX V2 XX V3 XX V4 XX V5 XX V6 XX V7 XX

        "punpcklwd %%xmm3, %%xmm0\n\t" // xmm0 = Y0 U0 V0 XX Y1 U1 V1 XX Y2 U2 V2 XX Y3 U3 V3 XX
        "punpckhwd %%xmm2, %%xmm1\n\t" // xmm1 = Y4 U4 V4 XX Y5 U5 V5 XX Y6 U6 V6 XX Y7 U7 V7 XX

        "movdqa %%xmm0,   (%[tmp_out])\n\t"
        "movdqa %%xmm1, 16(%[tmp_out])\n\t"

        "add $32, %[tmp_out]\n\t"

        "cmp %[length_7], %[i]\n\t"
        "jb 1b\n\t"
        "2:\n\t"

        : [i]"+&r"(i),
          [tmp_out]"+&r"(tmp_out)
        : [src_0]"r"(src_0),
          [src_1]"r"(src_1),
          [src_2]"r"(src_2),
          [length_7]"r"(length_7)
        : "memory"
    );
#endif
    for ( ; i < length; i++ )
    {
        *tmp_out++ = src_0[i];
        *tmp_out++ = src_1[i];
        *tmp_out++ = src_2[i];
        tmp_out++;
    }
}

static av_always_inline void
copy_tmp_to_src_packed(uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, uint8_t *tmp_out, int length)
{
    ssize_t i = 0;
#if ARCH_X86_64
    ssize_t length_7 = length - 7;
    __asm__ volatile(
        "xor %[i], %[i]\n\t"
        "test %[length_7], %[length_7]\n\t"
        "jle 2f\n\t"

        "pcmpeqw  %%xmm7, %%xmm7\n\t" // xmm7 = FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF
        "psrlw        $8, %%xmm7\n\t" // xmm7 = FF 00 FF 00 FF 00 FF 00 FF 00 FF 00 FF 00 FF 00

        "1:\n\t"
        "movdqa   (%[tmp_out]), %%xmm0\n\t" // xmm0 = Y0 U0 V0 XX Y1 U1 V1 XX Y2 U2 V2 XX Y3 U3 V3 XX
        "movdqa 16(%[tmp_out]), %%xmm1\n\t" // xmm1 = Y4 U4 V4 XX Y5 U5 V5 XX Y6 U6 V6 XX Y7 U7 V7 XX
        "add  $32, %[tmp_out]\n\t"

        "pshuflw $0xD8, %%xmm0, %%xmm3\n\t" // xmm3 = Y0 U0 Y1 U1 V0 XX V1 XX Y2 U2 V2 XX Y3 U3 V3 XX
        "pshufhw $0xD8, %%xmm3, %%xmm2\n\t" // xmm2 = Y0 U0 Y1 U1 V0 XX V1 XX Y2 U2 Y3 U3 V2 XX V3 XX
        "pshufd  $0xD8, %%xmm2, %%xmm4\n\t" // xmm4 = Y0 U0 Y1 U1 Y2 U2 Y3 U3 V0 XX V1 XX V2 XX V3 XX

        "pshuflw $0xD8, %%xmm1, %%xmm3\n\t" // xmm3 = Y4 U4 Y5 U5 V4 XX V5 XX Y6 U6 V6 XX Y7 U7 V7 XX
        "pshufhw $0xD8, %%xmm3, %%xmm2\n\t" // xmm2 = Y4 U4 Y5 U5 V4 XX V5 XX Y6 U6 Y7 U7 V6 XX V7 XX
        "pshufd  $0xD8, %%xmm2, %%xmm5\n\t" // xmm5 = Y4 U4 Y5 U5 Y6 U6 Y7 U7 V4 XX V5 XX V6 XX V7 XX

        "movdqa     %%xmm4, %%xmm3\n\t" // xmm3 = Y0 U0 Y1 U1 Y2 U2 Y3 U3 V0 XX V1 XX V2 XX V3 XX
        "punpcklqdq %%xmm5, %%xmm3\n\t" // xmm3 = Y0 U0 Y1 U1 Y2 U2 Y3 U3 Y4 U4 Y5 U5 Y6 U6 Y7 U7

        "movdqa     %%xmm3, %%xmm2\n\t" // xmm2 = Y0 U0 Y1 U1 Y2 U2 Y3 U3 Y4 U4 Y5 U5 Y6 U6 Y7 U7
        "pand       %%xmm7, %%xmm2\n\t" // xmm2 = Y0 00 Y1 00 Y2 00 Y3 00 Y4 00 Y5 00 Y6 00 Y7 00
        "packuswb   %%xmm2, %%xmm2\n\t" // xmm2 = Y0 Y1 Y2 Y3 Y4 Y5 Y6 Y7 XX XX XX XX XX XX XX XX
        // xmm2 = Y

        "psrlw          $8, %%xmm3\n\t" // xmm3 = U0 00 U1 00 U2 00 U3 00 U4 00 U5 00 U6 00 U7 00
        "packuswb   %%xmm3, %%xmm3\n\t" // xmm3 = U0 U1 U2 U3 U4 U5 U6 U7 XX XX XX XX XX XX XX XX
        // xmm3 = U

        "punpckhqdq %%xmm5, %%xmm4\n\t" // xmm4 = V0 XX V1 XX V2 XX V3 XX V4 XX V5 XX V6 XX V7 XX
        "pand       %%xmm7, %%xmm4\n\t" // xmm4 = V0 00 V1 00 V2 00 V3 00 V4 00 V5 00 V6 00 V7 00
        "packuswb   %%xmm4, %%xmm4\n\t" // xmm4 = V0 V1 V2 V3 V4 V5 V6 V7 XX XX XX XX XX XX XX XX
        // xmm4 = V

        "movq       %%xmm2, (%[src_0],%[i])\n\t"
        "movq       %%xmm3, (%[src_1],%[i])\n\t"
        "movq       %%xmm4, (%[src_2],%[i])\n\t"
        "add $8, %[i]\n\t"

        "cmp %[length_7], %[i]\n\t"
        "jb 1b\n\t"
        "2:\n\t"

        : [i]"+&r"(i),
          [tmp_out]"+&r"(tmp_out)
        : [src_0]"r"(src_0),
          [src_1]"r"(src_1),
          [src_2]"r"(src_2),
          [length_7]"r"(length_7)
        : "memory"
    );
#endif
    for ( ; i < length; i++ )
    {
        src_0[i] = *tmp_out++;
        src_1[i] = *tmp_out++;
        src_2[i] = *tmp_out++;
        tmp_out++;
    }
}

struct yuv_base_t {
    uint8_t vals[3];
    uint8_t padding; /* to make the struct 32-bit */
};

template<int sort_by_n, bool reverse_sort>
struct yuv_t : public yuv_base_t {
    bool operator < (const yuv_t &b) const
    {
        if ( reverse_sort )
            return vals[sort_by_n] >= b.vals[sort_by_n];
        else
            return vals[sort_by_n] < b.vals[sort_by_n];
    }
};

template<int sort_by_n, bool reverse_sort>
void stable_sort_uint8(uint8_t *tmp_out, uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, int length)
{
    yuv_t<sort_by_n, reverse_sort> *tmp_yuv = (yuv_t<sort_by_n, reverse_sort> *) tmp_out;
    copy_src_to_tmp_packed(src_0, src_1, src_2, tmp_out, length);
    std::stable_sort(tmp_yuv, tmp_yuv + length);
    copy_tmp_to_src_packed(src_0, src_1, src_2, tmp_out, length);
}

template<int sort_by_n, bool reverse_sort>
void pixelsort_internal_uint8_template(uint8_t *tmp_out, uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, int start, int end)
{
    const int length = end - start;

    /* already sorted */
    if ( length == 1 )
        return;

    src_0 += start;
    src_1 += start;
    src_2 += start;

    /* faster sort for small lengths (determined empirically) */
    if ( length < 256 )
        stable_sort_uint8<sort_by_n, reverse_sort>(tmp_out, src_0, src_1, src_2, length);
    else
        counting_sort_uint8<sort_by_n, reverse_sort>(tmp_out, src_0, src_1, src_2, length);
}

static void pixelsort_internal_uint8_0(uint8_t *tmp_out, uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, int start, int end) { pixelsort_internal_uint8_template<0, false>(tmp_out, src_0, src_1, src_2, start, end); }
static void pixelsort_internal_uint8_1(uint8_t *tmp_out, uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, int start, int end) { pixelsort_internal_uint8_template<1, false>(tmp_out, src_0, src_1, src_2, start, end); }
static void pixelsort_internal_uint8_2(uint8_t *tmp_out, uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, int start, int end) { pixelsort_internal_uint8_template<2, false>(tmp_out, src_0, src_1, src_2, start, end); }
static void pixelsort_internal_uint8_0_reverse_sort(uint8_t *tmp_out, uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, int start, int end) { pixelsort_internal_uint8_template<0, true>(tmp_out, src_0, src_1, src_2, start, end); }
static void pixelsort_internal_uint8_1_reverse_sort(uint8_t *tmp_out, uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, int start, int end) { pixelsort_internal_uint8_template<1, true>(tmp_out, src_0, src_1, src_2, start, end); }
static void pixelsort_internal_uint8_2_reverse_sort(uint8_t *tmp_out, uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, int start, int end) { pixelsort_internal_uint8_template<2, true>(tmp_out, src_0, src_1, src_2, start, end); }
// template<int sort_by_n, bool reverse_sort>
static pixelsort_internal_uint8_func pixelsort_internal_uint8_funcs[3][2] =
{
    { pixelsort_internal_uint8_0, pixelsort_internal_uint8_0_reverse_sort },
    { pixelsort_internal_uint8_1, pixelsort_internal_uint8_1_reverse_sort },
    { pixelsort_internal_uint8_2, pixelsort_internal_uint8_2_reverse_sort },
};

/*********************************************************************/
typedef struct {
    int x;
    int start;
    int end;
} range_t;

/*********************************************************************/
template<bool reverse_range>
bool threshold_range_next_uint8(range_t *range, int end_x, uint8_t *src, int lower, int upper)
{
    int x = range->x;
    while ( x < end_x && (src[x] >= lower && src[x] <= upper) == reverse_range )
        x++;
    range->start = x++;
    if ( x > end_x )
        return false;
    while ( x < end_x && (src[x] >= lower && src[x] <= upper) != reverse_range )
        x++;
    range->end = x++;
    range->x = x;
    return true;
}

template<int trigger_by_n, bool reverse_range, typename T>
bool threshold_range_next_hsvl(range_t *range, int end_x, hsvl_base_t *_hsvl, T lower, T upper)
{
    hsvl_t<trigger_by_n, false> *hsvl = (hsvl_t<trigger_by_n, false> *) _hsvl;
    int x = range->x;
    while ( x < end_x && hsvl[x].is_in_range(lower, upper) == reverse_range )
        x++;
    range->start = x++;
    if ( x > end_x )
        return false;
    while ( x < end_x && hsvl[x].is_in_range(lower, upper) != reverse_range )
        x++;
    range->end = x++;
    range->x = x;
    return true;
}

template<bool is_hsvl, int trigger_by_n, bool reverse_range>
void pixelsort_threshold_template(PixelSortThreadData *td, uint8_t *tmp_out, hsvl_base_t *hsvl, uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, int *linesizes, int start_x, int end_x, float lower, float upper)
{
    rgb_to_hsvl_func av_unused rgb_to_hsvl = td->vtable.rgb_to_hsvl;
    pixelsort_internal_hsvl_func av_unused pixelsort_internal_hsvl = td->vtable.pixelsort_internal_hsvl;
    pixelsort_internal_uint8_func av_unused pixelsort_internal_uint8 = td->vtable.pixelsort_internal_uint8;
    const int length = (end_x - start_x);
    int linesize_0 = linesizes[0];
    int linesize_1 = linesizes[1];
    int linesize_2 = linesizes[2];
    int start_y;
    int end_y;
    uint32_t av_unused i_lower = lower;
    uint32_t av_unused i_upper = upper;
    while ( pixelsort_get_range(td, &start_y, &end_y) )
    {
        for ( int y = start_y; y < end_y; y++ )
        {
            uint8_t *line_src_0 = src_0 + y * linesize_0;
            uint8_t *line_src_1 = src_1 + y * linesize_1;
            uint8_t *line_src_2 = src_2 + y * linesize_2;
            if ( is_hsvl )
            {
                uint8_t *src_r = line_src_2 + start_x;
                uint8_t *src_g = line_src_0 + start_x;
                uint8_t *src_b = line_src_1 + start_x;
                range_t range = { 0 };
                /* NOTE: we only do the rgb->hsvl conversion on the
                 *       input range we actually need. Therefore, the
                 *       range_next function starts at 0 and ends at
                 *       length (instead of start_x and end_x). */
                rgb_to_hsvl(hsvl, src_r, src_g, src_b, length);
                if ( trigger_by_n == 2 )
                {
                    while ( threshold_range_next_hsvl<trigger_by_n, reverse_range>(&range, length, hsvl, i_lower, i_upper) )
                        pixelsort_internal_hsvl(tmp_out, src_r, src_g, src_b, hsvl, range.start, range.end);
                }
                else
                {
                    while ( threshold_range_next_hsvl<trigger_by_n, reverse_range>(&range, length, hsvl, lower, upper) )
                        pixelsort_internal_hsvl(tmp_out, src_r, src_g, src_b, hsvl, range.start, range.end);
                }
            }
            else
            {
                range_t range = { start_x };
                uint8_t *src;
                switch ( trigger_by_n )
                {
                case 0: src = line_src_0; break;
                case 1: src = line_src_1; break;
                case 2: src = line_src_2; break;
                }
                while ( threshold_range_next_uint8<reverse_range>(&range, end_x, src, lower, upper) )
                    pixelsort_internal_uint8(tmp_out, line_src_0, line_src_1, line_src_2, range.start, range.end);
            }
        }
    }
}
static void pixelsort_threshold_hsvl_0(PixelSortThreadData *td, uint8_t *tmp_out, hsvl_base_t *hsvl, uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, int *linesizes, int start_x, int end_x, float lower, float upper) { pixelsort_threshold_template<true, 0, false>(td, tmp_out, hsvl, src_0, src_1, src_2, linesizes, start_x, end_x, lower, upper); }
static void pixelsort_threshold_hsvl_1(PixelSortThreadData *td, uint8_t *tmp_out, hsvl_base_t *hsvl, uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, int *linesizes, int start_x, int end_x, float lower, float upper) { pixelsort_threshold_template<true, 1, false>(td, tmp_out, hsvl, src_0, src_1, src_2, linesizes, start_x, end_x, lower, upper); }
static void pixelsort_threshold_hsvl_2(PixelSortThreadData *td, uint8_t *tmp_out, hsvl_base_t *hsvl, uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, int *linesizes, int start_x, int end_x, float lower, float upper) { pixelsort_threshold_template<true, 2, false>(td, tmp_out, hsvl, src_0, src_1, src_2, linesizes, start_x, end_x, lower, upper); }
static void pixelsort_threshold_uint8_0(PixelSortThreadData *td, uint8_t *tmp_out, hsvl_base_t *hsvl, uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, int *linesizes, int start_x, int end_x, float lower, float upper) { pixelsort_threshold_template<false, 0, false>(td, tmp_out, nullptr, src_0, src_1, src_2, linesizes, start_x, end_x, lower, upper); }
static void pixelsort_threshold_uint8_1(PixelSortThreadData *td, uint8_t *tmp_out, hsvl_base_t *hsvl, uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, int *linesizes, int start_x, int end_x, float lower, float upper) { pixelsort_threshold_template<false, 1, false>(td, tmp_out, nullptr, src_0, src_1, src_2, linesizes, start_x, end_x, lower, upper); }
static void pixelsort_threshold_uint8_2(PixelSortThreadData *td, uint8_t *tmp_out, hsvl_base_t *hsvl, uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, int *linesizes, int start_x, int end_x, float lower, float upper) { pixelsort_threshold_template<false, 2, false>(td, tmp_out, nullptr, src_0, src_1, src_2, linesizes, start_x, end_x, lower, upper); }
static void pixelsort_threshold_hsvl_0_reverse_range(PixelSortThreadData *td, uint8_t *tmp_out, hsvl_base_t *hsvl, uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, int *linesizes, int start_x, int end_x, float lower, float upper) { pixelsort_threshold_template<true, 0, true>(td, tmp_out, hsvl, src_0, src_1, src_2, linesizes, start_x, end_x, lower, upper); }
static void pixelsort_threshold_hsvl_1_reverse_range(PixelSortThreadData *td, uint8_t *tmp_out, hsvl_base_t *hsvl, uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, int *linesizes, int start_x, int end_x, float lower, float upper) { pixelsort_threshold_template<true, 1, true>(td, tmp_out, hsvl, src_0, src_1, src_2, linesizes, start_x, end_x, lower, upper); }
static void pixelsort_threshold_hsvl_2_reverse_range(PixelSortThreadData *td, uint8_t *tmp_out, hsvl_base_t *hsvl, uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, int *linesizes, int start_x, int end_x, float lower, float upper) { pixelsort_threshold_template<true, 2, true>(td, tmp_out, hsvl, src_0, src_1, src_2, linesizes, start_x, end_x, lower, upper); }
static void pixelsort_threshold_uint8_0_reverse_range(PixelSortThreadData *td, uint8_t *tmp_out, hsvl_base_t *hsvl, uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, int *linesizes, int start_x, int end_x, float lower, float upper) { pixelsort_threshold_template<false, 0, true>(td, tmp_out, nullptr, src_0, src_1, src_2, linesizes, start_x, end_x, lower, upper); }
static void pixelsort_threshold_uint8_1_reverse_range(PixelSortThreadData *td, uint8_t *tmp_out, hsvl_base_t *hsvl, uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, int *linesizes, int start_x, int end_x, float lower, float upper) { pixelsort_threshold_template<false, 1, true>(td, tmp_out, nullptr, src_0, src_1, src_2, linesizes, start_x, end_x, lower, upper); }
static void pixelsort_threshold_uint8_2_reverse_range(PixelSortThreadData *td, uint8_t *tmp_out, hsvl_base_t *hsvl, uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, int *linesizes, int start_x, int end_x, float lower, float upper) { pixelsort_threshold_template<false, 2, true>(td, tmp_out, nullptr, src_0, src_1, src_2, linesizes, start_x, end_x, lower, upper); }

typedef void (*pixelsort_threshold_func)(PixelSortThreadData *, uint8_t *, hsvl_base_t *, uint8_t *, uint8_t *, uint8_t *, int *, int, int, float, float);
static pixelsort_threshold_func pixelsort_threshold_hsvl_funcs[3][2] =
{
    { pixelsort_threshold_hsvl_0, pixelsort_threshold_hsvl_0_reverse_range },
    { pixelsort_threshold_hsvl_1, pixelsort_threshold_hsvl_1_reverse_range },
    { pixelsort_threshold_hsvl_2, pixelsort_threshold_hsvl_2_reverse_range },
};
static pixelsort_threshold_func pixelsort_threshold_uint8_funcs[3][2] =
{
    { pixelsort_threshold_uint8_0, pixelsort_threshold_uint8_0_reverse_range },
    { pixelsort_threshold_uint8_1, pixelsort_threshold_uint8_1_reverse_range },
    { pixelsort_threshold_uint8_2, pixelsort_threshold_uint8_2_reverse_range },
};

/*********************************************************************/
static av_always_inline
int random_range_next(range_t *range, int end_x, AVLFG *lfg, int clength)
{
    int x = range->x;
    x += av_lfg_get(lfg) % clength;
    range->start = x;
    x += av_lfg_get(lfg) % clength;
    range->end = x;
    range->x = x;
    if ( range->end >= end_x )
        return false;
    if ( range->start == range->end )
        return false;
    return true;
}

template<bool is_hsvl>
void pixelsort_random_template(PixelSortThreadData *td, uint8_t *tmp_out, hsvl_base_t *hsvl, AVLFG *lfg, uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, int *linesizes, int start_x, int end_x, int clength)
{
    rgb_to_hsvl_func av_unused rgb_to_hsvl = td->vtable.rgb_to_hsvl;
    pixelsort_internal_hsvl_func av_unused pixelsort_internal_hsvl = td->vtable.pixelsort_internal_hsvl;
    pixelsort_internal_uint8_func av_unused pixelsort_internal_uint8 = td->vtable.pixelsort_internal_uint8;
    const int length = (end_x - start_x);
    int linesize_0 = linesizes[0];
    int linesize_1 = linesizes[1];
    int linesize_2 = linesizes[2];
    int start_y;
    int end_y;
    while ( pixelsort_get_range(td, &start_y, &end_y) )
    {
        for ( int y = start_y; y < end_y; y++ )
        {
            uint8_t *line_src_0 = src_0 + y * linesize_0;
            uint8_t *line_src_1 = src_1 + y * linesize_1;
            uint8_t *line_src_2 = src_2 + y * linesize_2;
            if ( is_hsvl )
            {
                uint8_t *src_r = line_src_2 + start_x;
                uint8_t *src_g = line_src_0 + start_x;
                uint8_t *src_b = line_src_1 + start_x;
                range_t range = { 0 };
                /* NOTE: we only do the rgb->hsvl conversion on the
                 *       input range we actually need. Therefore, the
                 *       range_next function starts at 0 and ends at
                 *       length (instead of start_x and end_x). */
                rgb_to_hsvl(hsvl, src_r, src_g, src_b, length);
                while ( random_range_next(&range, length, lfg, clength) )
                    pixelsort_internal_hsvl(tmp_out, src_r, src_g, src_b, hsvl, range.start, range.end);
            }
            else
            {
                range_t range = { start_x };
                while ( random_range_next(&range, end_x, lfg, clength) )
                    pixelsort_internal_uint8(tmp_out, line_src_0, line_src_1, line_src_2, range.start, range.end);
            }
        }
    }
}

static void pixelsort_random_hsvl(PixelSortThreadData *td, uint8_t *tmp_out, hsvl_base_t *hsvl, AVLFG *lfg, uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, int *linesizes, int start_x, int end_x, int clength) { pixelsort_random_template<true>(td, tmp_out, hsvl, lfg, src_0, src_1, src_2, linesizes, start_x, end_x, clength); }
static void pixelsort_random_uint8(PixelSortThreadData *td, uint8_t *tmp_out, hsvl_base_t *hsvl, AVLFG *lfg, uint8_t *src_0, uint8_t *src_1, uint8_t *src_2, int *linesizes, int start_x, int end_x, int clength) { pixelsort_random_template<false>(td, tmp_out, hsvl, lfg, src_0, src_1, src_2, linesizes, start_x, end_x, clength); }

typedef void (*pixelsort_random_func)(PixelSortThreadData *, uint8_t *, hsvl_base_t *, AVLFG *, uint8_t *, uint8_t *, uint8_t *, int *, int, int, int);

/*********************************************************************/
int ff_pixelsort_slice(AVFilterContext *ctx, void *arg, int jobnr, int nb_jobs)
{
    PixelSortThreadData *td = (PixelSortThreadData *) arg;
    int start_x = td->start_x;
    int end_x = td->end_x;
    uint8_t *src_0 = td->src_0;
    uint8_t *src_1 = td->src_1;
    uint8_t *src_2 = td->src_2;
    int *linesizes = td->linesizes;
    float lower = td->lower_threshold;
    float upper = td->upper_threshold;
    int clength = td->clength;
    int sort_by_n = td->sort_by_n;
    int trigger_by_n = td->trigger_by_n;
    pixelsort_threshold_func pixelsort_threshold;
    pixelsort_random_func pixelsort_random;
    bool is_hsvl = (td->sort_colorspace_is_hsv || td->sort_colorspace_is_hsl);

    /* setup function pointers */
    if ( is_hsvl )
    {
        /* rgb_to_hsvl */
        bool do_h  = (td->trigger_by_n == 0) || (td->sort_by_n == 0);
        bool do_s  = (td->trigger_by_n == 1) || (td->sort_by_n == 1);
        bool do_vl = (td->trigger_by_n == 2) || (td->sort_by_n == 2);
        if ( td->sort_colorspace_is_hsv )
            td->vtable.rgb_to_hsvl = ff_rgb_to_hsv_c[do_h][do_s][do_vl];
        else
            td->vtable.rgb_to_hsvl = ff_rgb_to_hsl_c[do_h][do_s][do_vl];
        if ( ARCH_X86 )
            ff_pixelsort_init_x86(&td->vtable, td->sort_colorspace_is_hsv, do_h, do_s, do_vl);

        /* pixelsort_internal */
        td->vtable.pixelsort_internal_hsvl = pixelsort_internal_hsvl_funcs[sort_by_n][td->sort_colorspace_is_hsl][td->reverse_sort];
        /* pixelsort_threshold */
        if ( td->mode_is_threshold )
        {
            pixelsort_threshold = pixelsort_threshold_hsvl_funcs[trigger_by_n][td->reverse_range];
        }
        else if ( td->mode_is_random )
        {
            pixelsort_random = pixelsort_random_hsvl;
        }
    }
    else
    {
        /* pixelsort_internal */
        td->vtable.pixelsort_internal_uint8 = pixelsort_internal_uint8_funcs[sort_by_n][td->reverse_sort];
        /* pixelsort_threshold */
        if ( td->mode_is_threshold )
        {
            pixelsort_threshold = pixelsort_threshold_uint8_funcs[trigger_by_n][td->reverse_range];
        }
        else if ( td->mode_is_random )
        {
            pixelsort_random = pixelsort_random_uint8;
        }
    }

    uint8_t *tmp_out = (uint8_t *) av_malloc((end_x - start_x + 32) * sizeof(yuv_base_t));
    hsvl_base_t *hsvl = nullptr;

    if ( is_hsvl )
        hsvl = (hsvl_base_t *) av_malloc((end_x - start_x + 32) * sizeof(hsvl_base_t));

    if ( td->mode_is_threshold )
    {
        pixelsort_threshold(td, tmp_out, hsvl, src_0, src_1, src_2, linesizes, start_x, end_x, lower, upper);
    }
    else if ( td->mode_is_random )
    {
        AVLFG lfg;
        av_lfg_init(&lfg, av_get_random_seed());
        pixelsort_random(td, tmp_out, hsvl, &lfg, src_0, src_1, src_2, linesizes, start_x, end_x, clength);
    }

    av_free(hsvl);
    av_free(tmp_out);

    return 0;
}
