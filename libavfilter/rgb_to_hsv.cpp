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

#include "pixelsort.h"

/*********************************************************************/
template<bool do_h, bool do_s, bool do_v>
void rgb_to_hsv_template(hsvl_base_t *hsv, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, int length)
{
    for ( int i = 0; i < length; i++ )
    {
        uint8_t r = src_r[i];
        uint8_t g = src_g[i];
        uint8_t b = src_b[i];
        uint8_t m = FFMIN3(r, g, b);
        uint8_t v = FFMAX3(r, g, b);
        uint8_t c = v - m;
        if ( c == 0 )
        {
            if ( do_h )
                hsv->h = 0;
            if ( do_s )
                hsv->s = 0;
        }
        else
        {
            if ( do_h )
            {
                float h;
                float invC = 1. / c;
                if ( v == r ) {
                    h =       (float) (g - b) * invC;
                } else if ( v == g ) {
                    h = 2.0 + (float) (b - r) * invC;
                } else {
                    h = 4.0 + (float) (r - g) * invC;
                }
                h = h / 6.;
                if ( h < 0 )
                    h += 1;
                hsv->h = h;
            }
            if ( do_s )
                hsv->s = (float) c / v;
        }
        if ( do_v )
            hsv->v = v;
        hsv->idx = i;
        hsv++;
    }
}

static void rgb_to_hsv_001_c(hsvl_base_t *hsv, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, int length) { rgb_to_hsv_template<false, false, true>(hsv, src_r, src_g, src_b, length); }
static void rgb_to_hsv_010_c(hsvl_base_t *hsv, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, int length) { rgb_to_hsv_template<false, true, false>(hsv, src_r, src_g, src_b, length); }
static void rgb_to_hsv_011_c(hsvl_base_t *hsv, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, int length) { rgb_to_hsv_template<false, true, true>(hsv, src_r, src_g, src_b, length); }
static void rgb_to_hsv_100_c(hsvl_base_t *hsv, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, int length) { rgb_to_hsv_template<true, false, false>(hsv, src_r, src_g, src_b, length); }
static void rgb_to_hsv_101_c(hsvl_base_t *hsv, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, int length) { rgb_to_hsv_template<true, false, true>(hsv, src_r, src_g, src_b, length); }
static void rgb_to_hsv_110_c(hsvl_base_t *hsv, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, int length) { rgb_to_hsv_template<true, true, false>(hsv, src_r, src_g, src_b, length); }
// template<bool do_h, bool do_s, bool do_v>
rgb_to_hsvl_func ff_rgb_to_hsv_c[2][2][2] =
{
    { { nullptr,          rgb_to_hsv_001_c },
      { rgb_to_hsv_010_c, rgb_to_hsv_011_c }, },
    { { rgb_to_hsv_100_c, rgb_to_hsv_101_c },
      { rgb_to_hsv_110_c, nullptr          }, },
};

template<bool do_h, bool do_s, bool do_l>
void rgb_to_hsl_template(hsvl_base_t *hsl, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, int length)
{
    for ( int i = 0; i < length; i++ )
    {
        uint8_t r = src_r[i];
        uint8_t g = src_g[i];
        uint8_t b = src_b[i];
        uint8_t m = FFMIN3(r, g, b);
        uint8_t v = FFMAX3(r, g, b);
        uint8_t c = v - m;
        uint16_t _2l;
        if ( do_s || do_l )
            _2l = (m + v);
        if ( c == 0 )
        {
            if ( do_h )
                hsl->h = 0;
            if ( do_s )
                hsl->s = 0;
        }
        else
        {
            if ( do_h )
            {
                float h;
                float invC = 1. / c;
                if ( v == r ) {
                    h =       (float) (g - b) * invC;
                } else if ( v == g ) {
                    h = 2.0 + (float) (b - r) * invC;
                } else {
                    h = 4.0 + (float) (r - g) * invC;
                }
                h = h / 6.;
                if ( h < 0 )
                    h += 1;
                hsl->h = h;
            }
            if ( do_s )
                hsl->s = (float) c / FFMIN(_2l, 510-_2l);
        }
        if ( do_l )
            hsl->_2l = _2l;
        hsl->idx = i;
        hsl++;
    }
}

static void rgb_to_hsl_001_c(hsvl_base_t *hsl, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, int length) { rgb_to_hsl_template<false, false, true>(hsl, src_r, src_g, src_b, length); }
static void rgb_to_hsl_010_c(hsvl_base_t *hsl, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, int length) { rgb_to_hsl_template<false, true, false>(hsl, src_r, src_g, src_b, length); }
static void rgb_to_hsl_011_c(hsvl_base_t *hsl, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, int length) { rgb_to_hsl_template<false, true, true>(hsl, src_r, src_g, src_b, length); }
static void rgb_to_hsl_100_c(hsvl_base_t *hsl, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, int length) { rgb_to_hsl_template<true, false, false>(hsl, src_r, src_g, src_b, length); }
static void rgb_to_hsl_101_c(hsvl_base_t *hsl, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, int length) { rgb_to_hsl_template<true, false, true>(hsl, src_r, src_g, src_b, length); }
static void rgb_to_hsl_110_c(hsvl_base_t *hsl, uint8_t *src_r, uint8_t *src_g, uint8_t *src_b, int length) { rgb_to_hsl_template<true, true, false>(hsl, src_r, src_g, src_b, length); }
// template<bool do_h, bool do_s, bool do_l>
rgb_to_hsvl_func ff_rgb_to_hsl_c[2][2][2] =
{
    { { nullptr,          rgb_to_hsl_001_c },
      { rgb_to_hsl_010_c, rgb_to_hsl_011_c }, },
    { { rgb_to_hsl_100_c, rgb_to_hsl_101_c },
      { rgb_to_hsl_110_c, nullptr          }, },
};
