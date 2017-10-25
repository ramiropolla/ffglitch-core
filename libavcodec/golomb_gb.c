/*
 * exp golomb vlc stuff
 * Copyright (c) 2003 Michael Niedermayer <michaelni@gmx.at>
 * Copyright (c) 2004 Alex Beregszaszi
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

/**
 * @file
 * @brief
 *     exp golomb vlc stuff
 * @author Michael Niedermayer <michaelni@gmx.at> and Alex Beregszaszi
 */

#include "get_bits_common.h"
#define GB_PREFIX gb_be_
#include "get_bits_funcs.h"
#include "get_bits_defines.h"

#include "golomb_common.h"
#define GOLOMB_FUNCS_GB
#include "golomb_funcs.h"

/**
 * Read an unsigned Exp-Golomb code in the range 0 to 8190.
 */
int get_ue_golomb(GetBitContext *gb)
{
    unsigned int buf;

    OPEN_READER(re, gb);
    UPDATE_CACHE(re, gb);
    buf = GET_CACHE(re, gb);

    if (buf >= (1 << 27)) {
        buf >>= 32 - 9;
        LAST_SKIP_BITS(re, gb, ff_golomb_vlc_len[buf]);
        CLOSE_READER(re, gb);

        return ff_ue_golomb_vlc_code[buf];
    } else {
        int log = 2 * av_log2(buf) - 31;
        LAST_SKIP_BITS(re, gb, 32 - log);
        CLOSE_READER(re, gb);
        if (log < 7) {
            av_log(NULL, AV_LOG_ERROR, "Invalid UE golomb code\n");
            return AVERROR_INVALIDDATA;
        }
        buf >>= log;
        buf--;

        return buf;
    }
}

/**
 * Read an unsigned Exp-Golomb code in the range 0 to UINT32_MAX-1.
 */
unsigned get_ue_golomb_long(GetBitContext *gb)
{
    unsigned buf, log;

    buf = show_bits_long(gb, 32);
    log = 31 - av_log2(buf);
    skip_bits_long(gb, log);

    return get_bits_long(gb, log + 1) - 1;
}

/**
 * read unsigned exp golomb code, constraint to a max of 31.
 * the return value is undefined if the stored value exceeds 31.
 */
int get_ue_golomb_31(GetBitContext *gb)
{
    unsigned int buf;

    OPEN_READER(re, gb);
    UPDATE_CACHE(re, gb);
    buf = GET_CACHE(re, gb);

    buf >>= 32 - 9;
    LAST_SKIP_BITS(re, gb, ff_golomb_vlc_len[buf]);
    CLOSE_READER(re, gb);

    return ff_ue_golomb_vlc_code[buf];
}

unsigned get_interleaved_ue_golomb(GetBitContext *gb)
{
    uint32_t buf;

    OPEN_READER(re, gb);
    UPDATE_CACHE(re, gb);
    buf = GET_CACHE(re, gb);

    if (buf & 0xAA800000) {
        buf >>= 32 - 8;
        LAST_SKIP_BITS(re, gb, ff_interleaved_golomb_vlc_len[buf]);
        CLOSE_READER(re, gb);

        return ff_interleaved_ue_golomb_vlc_code[buf];
    } else {
        unsigned ret = 1;

        do {
            buf >>= 32 - 8;
            LAST_SKIP_BITS(re, gb,
                           FFMIN(ff_interleaved_golomb_vlc_len[buf], 8));

            if (ff_interleaved_golomb_vlc_len[buf] != 9) {
                ret <<= (ff_interleaved_golomb_vlc_len[buf] - 1) >> 1;
                ret  |= ff_interleaved_dirac_golomb_vlc_code[buf];
                break;
            }
            ret = (ret << 4) | ff_interleaved_dirac_golomb_vlc_code[buf];
            UPDATE_CACHE(re, gb);
            buf = GET_CACHE(re, gb);
        } while (ret<0x8000000U && BITS_AVAILABLE(re, gb));

        CLOSE_READER(re, gb);
        return ret - 1;
    }
}

/**
 * read unsigned truncated exp golomb code.
 */
int get_te0_golomb(GetBitContext *gb, int range)
{
    av_assert2(range >= 1);

    if (range == 1)
        return 0;
    else if (range == 2)
        return get_bits1(gb) ^ 1;
    else
        return get_ue_golomb(gb);
}

/**
 * read unsigned truncated exp golomb code.
 */
int get_te_golomb(GetBitContext *gb, int range)
{
    av_assert2(range >= 1);

    if (range == 2)
        return get_bits1(gb) ^ 1;
    else
        return get_ue_golomb(gb);
}

/**
 * read signed exp golomb code.
 */
int get_se_golomb(GetBitContext *gb)
{
    unsigned int buf;

    OPEN_READER(re, gb);
    UPDATE_CACHE(re, gb);
    buf = GET_CACHE(re, gb);

    if (buf >= (1 << 27)) {
        buf >>= 32 - 9;
        LAST_SKIP_BITS(re, gb, ff_golomb_vlc_len[buf]);
        CLOSE_READER(re, gb);

        return ff_se_golomb_vlc_code[buf];
    } else {
        int log = av_log2(buf), sign;
        LAST_SKIP_BITS(re, gb, 31 - log);
        UPDATE_CACHE(re, gb);
        buf = GET_CACHE(re, gb);

        buf >>= log;

        LAST_SKIP_BITS(re, gb, 32 - log);
        CLOSE_READER(re, gb);

        sign = -(buf & 1);
        buf  = ((buf >> 1) ^ sign) - sign;

        return buf;
    }
}

int get_se_golomb_long(GetBitContext *gb)
{
    unsigned int buf = get_ue_golomb_long(gb);
    int sign = (buf & 1) - 1;
    return ((buf >> 1) ^ sign) + 1;
}

int get_interleaved_se_golomb(GetBitContext *gb)
{
    unsigned int buf;

    OPEN_READER(re, gb);
    UPDATE_CACHE(re, gb);
    buf = GET_CACHE(re, gb);

    if (buf & 0xAA800000) {
        buf >>= 32 - 8;
        LAST_SKIP_BITS(re, gb, ff_interleaved_golomb_vlc_len[buf]);
        CLOSE_READER(re, gb);

        return ff_interleaved_se_golomb_vlc_code[buf];
    } else {
        int log;
        LAST_SKIP_BITS(re, gb, 8);
        UPDATE_CACHE(re, gb);
        buf |= 1 | (GET_CACHE(re, gb) >> 8);

        if ((buf & 0xAAAAAAAA) == 0)
            return INVALID_VLC;

        for (log = 31; (buf & 0x80000000) == 0; log--)
            buf = (buf << 2) - ((buf << log) >> (log - 1)) + (buf >> 30);

        LAST_SKIP_BITS(re, gb, 63 - 2 * log - 8);
        CLOSE_READER(re, gb);

        return (signed) (((((buf << log) >> log) - 1) ^ -(buf & 0x1)) + 1) >> 1;
    }
}

int dirac_get_se_golomb(GetBitContext *gb)
{
    uint32_t ret = get_interleaved_ue_golomb(gb);

    if (ret) {
        int sign = -get_bits1(gb);
        ret = (ret ^ sign) - sign;
    }

    return ret;
}

/**
 * read unsigned golomb rice code (ffv1).
 */
int get_ur_golomb(GetBitContext *gb, int k, int limit, int esc_len)
{
    unsigned int buf;
    int log;

    OPEN_READER(re, gb);
    UPDATE_CACHE(re, gb);
    buf = GET_CACHE(re, gb);

    log = av_log2(buf);

    if (log > 31 - limit) {
        buf >>= log - k;
        buf  += (30U - log) << k;
        LAST_SKIP_BITS(re, gb, 32 + k - log);
        CLOSE_READER(re, gb);

        return buf;
    } else {
        LAST_SKIP_BITS(re, gb, limit);
        UPDATE_CACHE(re, gb);

        buf = SHOW_UBITS(re, gb, esc_len);

        LAST_SKIP_BITS(re, gb, esc_len);
        CLOSE_READER(re, gb);

        return buf + limit - 1;
    }
}

/**
 * read unsigned golomb rice code (jpegls).
 */
int get_ur_golomb_jpegls(GetBitContext *gb, int k, int limit, int esc_len)
{
    unsigned int buf;
    int log;

    OPEN_READER(re, gb);
    UPDATE_CACHE(re, gb);
    buf = GET_CACHE(re, gb);

    log = av_log2(buf);

    av_assert2(k <= 31);

    if (log - k >= 32 - MIN_CACHE_BITS + (MIN_CACHE_BITS == 32) &&
        32 - log < limit) {
        buf >>= log - k;
        buf  += (30U - log) << k;
        LAST_SKIP_BITS(re, gb, 32 + k - log);
        CLOSE_READER(re, gb);

        return buf;
    } else {
        int i;
        for (i = 0; i < limit && SHOW_UBITS(re, gb, 1) == 0; i++) {
            if (gb->size_in_bits <= re_index) {
                CLOSE_READER(re, gb);
                return -1;
            }
            LAST_SKIP_BITS(re, gb, 1);
            UPDATE_CACHE(re, gb);
        }
        SKIP_BITS(re, gb, 1);

        if (i < limit - 1) {
            if (k) {
                if (k > MIN_CACHE_BITS - 1) {
                    buf = SHOW_UBITS(re, gb, 16) << (k-16);
                    LAST_SKIP_BITS(re, gb, 16);
                    UPDATE_CACHE(re, gb);
                    buf |= SHOW_UBITS(re, gb, k-16);
                    LAST_SKIP_BITS(re, gb, k-16);
                } else {
                    buf = SHOW_UBITS(re, gb, k);
                    LAST_SKIP_BITS(re, gb, k);
                }
            } else {
                buf = 0;
            }

            buf += ((SUINT)i << k);
        } else if (i == limit - 1) {
            buf = SHOW_UBITS(re, gb, esc_len);
            LAST_SKIP_BITS(re, gb, esc_len);

            buf ++;
        } else {
            buf = -1;
        }
        CLOSE_READER(re, gb);
        return buf;
    }
}

/**
 * read signed golomb rice code (ffv1).
 */
int get_sr_golomb(GetBitContext *gb, int k, int limit, int esc_len)
{
    unsigned v = get_ur_golomb(gb, k, limit, esc_len);
    return (v >> 1) ^ -(v & 1);
}

/**
 * read signed golomb rice code (flac).
 */
int get_sr_golomb_flac(GetBitContext *gb, int k, int limit, int esc_len)
{
    unsigned v = get_ur_golomb_jpegls(gb, k, limit, esc_len);
    return (v >> 1) ^ -(v & 1);
}

/**
 * read unsigned golomb rice code (shorten).
 */
unsigned int get_ur_golomb_shorten(GetBitContext *gb, int k)
{
    return get_ur_golomb_jpegls(gb, k, INT_MAX, 0);
}

/**
 * read signed golomb rice code (shorten).
 */
int get_sr_golomb_shorten(GetBitContext *gb, int k)
{
    int uvar = get_ur_golomb_jpegls(gb, k + 1, INT_MAX, 0);
    return (uvar >> 1) ^ -(uvar & 1);
}

#ifdef TRACE

int get_ue(GetBitContext *s, const char *file, const char *func, int line)
{
    int show = show_bits(s, 24);
    int pos  = get_bits_count(s);
    int i    = get_ue_golomb(s);
    int len  = get_bits_count(s) - pos;
    int bits = show >> (24 - len);

    av_log(NULL, AV_LOG_DEBUG, "%5d %2d %3d ue  @%5d in %s %s:%d\n",
           bits, len, i, pos, file, func, line);

    return i;
}

int get_se(GetBitContext *s, const char *file, const char *func, int line)
{
    int show = show_bits(s, 24);
    int pos  = get_bits_count(s);
    int i    = get_se_golomb(s);
    int len  = get_bits_count(s) - pos;
    int bits = show >> (24 - len);

    av_log(NULL, AV_LOG_DEBUG, "%5d %2d %3d se  @%5d in %s %s:%d\n",
           bits, len, i, pos, file, func, line);

    return i;
}

int get_te(GetBitContext *s, int r, char *file, const char *func, int line)
{
    int show = show_bits(s, 24);
    int pos  = get_bits_count(s);
    int i    = get_te0_golomb(s, r);
    int len  = get_bits_count(s) - pos;
    int bits = show >> (24 - len);

    av_log(NULL, AV_LOG_DEBUG, "%5d %2d %3d te  @%5d in %s %s:%d\n",
           bits, len, i, pos, file, func, line);

    return i;
}

#define get_ue_golomb(a) get_ue(a, __FILE__, __func__, __LINE__)
#define get_se_golomb(a) get_se(a, __FILE__, __func__, __LINE__)
#define get_te_golomb(a, r)  get_te(a, r, __FILE__, __func__, __LINE__)
#define get_te0_golomb(a, r) get_te(a, r, __FILE__, __func__, __LINE__)

#endif /* TRACE */
