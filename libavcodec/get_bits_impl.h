/*
 * copyright (c) 2004 Michael Niedermayer <michaelni@gmx.at>
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
 * bitstream reader API header.
 */

#include "get_bits_funcs.h"
#include "get_bits_defines.h"

int get_bits_count(const GetBitContext *s)
{
    return s->index;
}

/**
 * Skips the specified number of bits.
 * @param n the number of bits to skip,
 *          For the UNCHECKED_BITSTREAM_READER this must not cause the distance
 *          from the start to overflow int32_t. Staying within the bitstream + padding
 *          is sufficient, too.
 */
void skip_bits_long(GetBitContext *s, int n)
{
    get_bits_long(s, n);
}

/**
 * Read MPEG-1 dc-style VLC (sign bit + mantissa with no MSB).
 * if MSB not set it is negative
 * @param n length in bits
 */
int get_xbits(GetBitContext *s, int n)
{
    register int sign;
    register int32_t cache;
    int tmp;
    OPEN_READER(re, s);
    GB_DEBUG_START(s);
    av_assert2(n>0 && n<=25);
    UPDATE_CACHE(re, s);
    cache = GET_CACHE(re, s);
    sign  = ~cache >> 31;
    tmp = SHOW_UBITS(re, s, n);
    if ( s->pb != NULL )
        put_bits(s->pb, n, tmp);
    LAST_SKIP_BITS(re, s, n);
    CLOSE_READER(re, s);
    GB_DEBUG_END(s, tmp);
    return (NEG_USR32(sign ^ cache, n) ^ sign) - sign;
}

int get_xbits_le(GetBitContext *s, int n)
{
    register int sign;
    register int32_t cache;
    int tmp;
    OPEN_READER(re, s);
    GB_DEBUG_START(s);
    av_assert2(n>0 && n<=25);
    UPDATE_CACHE_LE(re, s);
    cache = GET_CACHE(re, s);
    sign  = sign_extend(~cache, n) >> 31;
    tmp = SHOW_UBITS_LE(re, s, n);
    if ( s->pb != NULL )
        put_bits(s->pb, n, tmp);
    LAST_SKIP_BITS(re, s, n);
    CLOSE_READER(re, s);
    GB_DEBUG_END(s, tmp);
    return (zero_extend(sign ^ cache, n) ^ sign) - sign;
}

int get_sbits(GetBitContext *s, int n)
{
    register int tmp;
    OPEN_READER(re, s);
    GB_DEBUG_START(s);
    av_assert2(n>0 && n<=25);
    UPDATE_CACHE(re, s);
    tmp = SHOW_UBITS(re, s, n);
    if ( s->pb != NULL )
        put_bits(s->pb, n, tmp);
    tmp = sign_extend(tmp, n);
    LAST_SKIP_BITS(re, s, n);
    CLOSE_READER(re, s);
    GB_DEBUG_END(s, tmp);
    return tmp;
}

/**
 * Read 1-25 bits.
 */
unsigned int get_bits(GetBitContext *s, int n)
{
    register int tmp;
    OPEN_READER(re, s);
    GB_DEBUG_START(s);
    av_assert2(n>0 && n<=25);
    UPDATE_CACHE(re, s);
    tmp = SHOW_UBITS(re, s, n);
    if ( s->pb != NULL )
        put_bits(s->pb, n, tmp);
    LAST_SKIP_BITS(re, s, n);
    CLOSE_READER(re, s);
    GB_DEBUG_END(s, tmp);
    return tmp;
}

/**
 * Read 0-25 bits.
 */
int get_bitsz(GetBitContext *s, int n)
{
    return n ? get_bits(s, n) : 0;
}

unsigned int get_bits_le(GetBitContext *s, int n)
{
    register int tmp;
    OPEN_READER(re, s);
    GB_DEBUG_START(s);
    av_assert2(n>0 && n<=25);
    UPDATE_CACHE_LE(re, s);
    tmp = SHOW_UBITS_LE(re, s, n);
    if ( s->pb != NULL )
        put_bits(s->pb, n, tmp);
    LAST_SKIP_BITS(re, s, n);
    CLOSE_READER(re, s);
    GB_DEBUG_END(s, tmp);
    return tmp;
}

/**
 * Show 1-25 bits.
 */
unsigned int show_bits(GetBitContext *s, int n)
{
    register int tmp;
    OPEN_READER_NOSIZE(re, s);
    GB_DEBUG_START(s);
    av_assert2(n>0 && n<=25);
    UPDATE_CACHE(re, s);
    tmp = SHOW_UBITS(re, s, n);
    GB_DEBUG_END_SHOW(s, tmp, n);
    return tmp;
}

void skip_bits(GetBitContext *s, int n)
{
    get_bits(s, n);
}

unsigned int get_bits1(GetBitContext *s)
{
    return get_bits(s, 1);
}

unsigned int show_bits1(GetBitContext *s)
{
    return show_bits(s, 1);
}

void skip_bits1(GetBitContext *s)
{
    skip_bits(s, 1);
}

/**
 * Read 0-32 bits.
 */
unsigned int get_bits_long(GetBitContext *s, int n)
{
    av_assert2(n>=0 && n<=32);
    if (!n) {
        return 0;
    } else if (n <= MIN_CACHE_BITS) {
        return get_bits(s, n);
    } else {
#ifdef BITSTREAM_READER_LE
        unsigned ret = get_bits(s, 16);
        return ret | (get_bits(s, n - 16) << 16);
#else
        unsigned ret = get_bits(s, 16) << (n - 16);
        return ret | get_bits(s, n - 16);
#endif
    }
}

/**
 * Read 0-64 bits.
 */
uint64_t get_bits64(GetBitContext *s, int n)
{
    if (n <= 32) {
        return get_bits_long(s, n);
    } else {
#ifdef BITSTREAM_READER_LE
        uint64_t ret = get_bits_long(s, 32);
        return ret | (uint64_t) get_bits_long(s, n - 32) << 32;
#else
        uint64_t ret = (uint64_t) get_bits_long(s, n - 32) << 32;
        return ret | get_bits_long(s, 32);
#endif
    }
}

/**
 * Read 0-32 bits as a signed integer.
 */
int get_sbits_long(GetBitContext *s, int n)
{
    // sign_extend(x, 0) is undefined
    if (!n)
        return 0;

    return sign_extend(get_bits_long(s, n), n);
}

/**
 * Show 0-32 bits.
 */
unsigned int show_bits_long(GetBitContext *s, int n)
{
    if (n <= MIN_CACHE_BITS) {
        return show_bits(s, n);
    } else {
        GetBitContext gb = *s;
        return get_bits_long(&gb, n);
    }
}

int check_marker(void *logctx, GetBitContext *s, const char *msg)
{
    int bit = get_bits1(s);
    if (!bit)
        av_log(logctx, AV_LOG_INFO, "Marker bit missing at %d of %d %s\n",
               get_bits_count(s) - 1, s->size_in_bits, msg);

    return bit;
}

/**
 * Initialize GetBitContext.
 * @param buffer bitstream buffer, must be AV_INPUT_BUFFER_PADDING_SIZE bytes
 *        larger than the actual read bits because some optimized bitstream
 *        readers read 32 or 64 bit at once and could read over the end
 * @param bit_size the size of the buffer in bits
 * @return 0 on success, AVERROR_INVALIDDATA if the buffer_size would overflow.
 */
int init_get_bits(GetBitContext *s, const uint8_t *buffer, int bit_size)
{
    int buffer_size;
    int ret = 0;

    if (bit_size >= INT_MAX - FFMAX(7, AV_INPUT_BUFFER_PADDING_SIZE*8) || bit_size < 0 || !buffer) {
        bit_size    = 0;
        buffer      = NULL;
        ret         = AVERROR_INVALIDDATA;
    }

    buffer_size = (bit_size + 7) >> 3;

    s->buffer             = buffer;
    s->size_in_bits       = bit_size;
    s->size_in_bits_plus8 = bit_size + 8;
    s->buffer_end         = buffer + buffer_size;
    s->index              = 0;
    s->pb                 = NULL;

    return ret;
}

/**
 * Initialize GetBitContext.
 * @param buffer bitstream buffer, must be AV_INPUT_BUFFER_PADDING_SIZE bytes
 *        larger than the actual read bits because some optimized bitstream
 *        readers read 32 or 64 bit at once and could read over the end
 * @param byte_size the size of the buffer in bytes
 * @return 0 on success, AVERROR_INVALIDDATA if the buffer_size would overflow.
 */
int init_get_bits8(GetBitContext *s, const uint8_t *buffer, int byte_size)
{
    if (byte_size > INT_MAX / 8 || byte_size < 0)
        byte_size = -1;
    return init_get_bits(s, buffer, byte_size * 8);
}

const uint8_t *align_get_bits(GetBitContext *s)
{
    int n = -get_bits_count(s) & 7;
    if (n)
        skip_bits(s, n);
    return s->buffer + (s->index >> 3);
}

/**
 * Parse a vlc code.
 * @param bits is the number of bits which will be read at once, must be
 *             identical to nb_bits in init_vlc()
 * @param max_depth is the number of times bits bits must be read to completely
 *                  read the longest vlc code
 *                  = (max_vlc_length + bits - 1) / bits
 * @returns the code parsed or -1 if no vlc matches
 */
int get_vlc2(GetBitContext *s, VLC_TYPE (*table)[2], int bits, int max_depth)
{
    int code;
#ifdef FFEDIT_GB_DEBUG
    int dbg_cache;
#endif

    OPEN_READER(re, s);
    GB_DEBUG_START(s);
    UPDATE_CACHE(re, s);
#ifdef FFEDIT_GB_DEBUG
    dbg_cache = re_cache;
#endif

    GET_VLC(code, re, s, table, bits, max_depth);

    CLOSE_READER(re, s);

    GB_DEBUG_END_CACHE(s, dbg_cache);

    return code;
}

void get_rl_vlc2(
        int *plevel,
        int *prun,
        GetBitContext *s,
        RL_VLC_ELEM *table,
        int bits,
        int max_depth,
        int need_update)
{
    int level, run;
#ifdef FFEDIT_GB_DEBUG
    int dbg_cache;
#endif

    OPEN_READER(re, s);
    GB_DEBUG_START(s);
    UPDATE_CACHE(re, s);
#ifdef FFEDIT_GB_DEBUG
    dbg_cache = re_cache;
#endif

    GET_RL_VLC(level, run, re, s, table, bits, max_depth, need_update);

    CLOSE_READER(re, s);

    *plevel = level;
    *prun = run;

    GB_DEBUG_END_CACHE(s, dbg_cache);
}

void get_cfhd_rl_vlc(
        int *plevel,
        int *prun,
        GetBitContext *s,
        CFHD_RL_VLC_ELEM *table,
        int bits,
        int max_depth,
        int need_update)
{
    int level, run;

    OPEN_READER(re, s);
    UPDATE_CACHE(re, s);

    GET_RL_VLC(level, run, re, s, table, bits, max_depth, need_update);

    CLOSE_READER(re, s);

    *plevel = level;
    *prun = run;
}

int decode012(GetBitContext *gb)
{
    int n;
    n = get_bits1(gb);
    if (n == 0)
        return 0;
    else
        return get_bits1(gb) + 1;
}

int decode210(GetBitContext *gb)
{
    if (get_bits1(gb))
        return 0;
    else
        return 2 - get_bits1(gb);
}

int get_bits_left(GetBitContext *gb)
{
    return gb->size_in_bits - get_bits_count(gb);
}

int skip_1stop_8data_bits(GetBitContext *gb)
{
    if (get_bits_left(gb) <= 0)
        return AVERROR_INVALIDDATA;

    while (get_bits1(gb)) {
        skip_bits(gb, 8);
        if (get_bits_left(gb) <= 0)
            return AVERROR_INVALIDDATA;
    }

    return 0;
}

#include "get_bits_undef.h"

#if CONFIG_FFEDIT_XP_DEBUG
int AV_JOIN(dbg_, get_bits_count)(const GetBitContext *s, const char *file, int line, const char *func);
int AV_JOIN(dbg_, get_bits_count)(const GetBitContext *s, const char *file, int line, const char *func)
{
    return get_bits_count(s);
}

void AV_JOIN(dbg_, skip_bits_long)(GetBitContext *s, int n, const char *file, int line, const char *func);
void AV_JOIN(dbg_, skip_bits_long)(GetBitContext *s, int n, const char *file, int line, const char *func)
{
    GB_DEBUG_LOG("[%s][%d][%s] %s(%d)\n", file, line, func, __func__, n);
    return skip_bits_long(s, n);
}

int AV_JOIN(dbg_, get_xbits)(GetBitContext *s, int n, const char *file, int line, const char *func);
int AV_JOIN(dbg_, get_xbits)(GetBitContext *s, int n, const char *file, int line, const char *func)
{
    GB_DEBUG_LOG("[%s][%d][%s] %s(%d)\n", file, line, func, __func__, n);
    return get_xbits(s, n);
}

int AV_JOIN(dbg_, get_xbits_le)(GetBitContext *s, int n, const char *file, int line, const char *func);
int AV_JOIN(dbg_, get_xbits_le)(GetBitContext *s, int n, const char *file, int line, const char *func)
{
    GB_DEBUG_LOG("[%s][%d][%s] %s(%d)\n", file, line, func, __func__, n);
    return get_xbits_le(s, n);
}

int AV_JOIN(dbg_, get_sbits)(GetBitContext *s, int n, const char *file, int line, const char *func);
int AV_JOIN(dbg_, get_sbits)(GetBitContext *s, int n, const char *file, int line, const char *func)
{
    GB_DEBUG_LOG("[%s][%d][%s] %s(%d)\n", file, line, func, __func__, n);
    return get_sbits(s, n);
}

unsigned int AV_JOIN(dbg_, get_bits)(GetBitContext *s, int n, const char *file, int line, const char *func);
unsigned int AV_JOIN(dbg_, get_bits)(GetBitContext *s, int n, const char *file, int line, const char *func)
{
    GB_DEBUG_LOG("[%s][%d][%s] %s(%d)\n", file, line, func, __func__, n);
    return get_bits(s, n);
}

int AV_JOIN(dbg_, get_bitsz)(GetBitContext *s, int n, const char *file, int line, const char *func);
int AV_JOIN(dbg_, get_bitsz)(GetBitContext *s, int n, const char *file, int line, const char *func)
{
    GB_DEBUG_LOG("[%s][%d][%s] %s(%d)\n", file, line, func, __func__, n);
    return get_bitsz(s, n);
}

unsigned int AV_JOIN(dbg_, get_bits_le)(GetBitContext *s, int n, const char *file, int line, const char *func);
unsigned int AV_JOIN(dbg_, get_bits_le)(GetBitContext *s, int n, const char *file, int line, const char *func)
{
    GB_DEBUG_LOG("[%s][%d][%s] %s(%d)\n", file, line, func, __func__, n);
    return get_bits_le(s, n);
}

unsigned int AV_JOIN(dbg_, show_bits)(GetBitContext *s, int n, const char *file, int line, const char *func);
unsigned int AV_JOIN(dbg_, show_bits)(GetBitContext *s, int n, const char *file, int line, const char *func)
{
    GB_DEBUG_LOG("[%s][%d][%s] %s(%d)\n", file, line, func, __func__, n);
    return show_bits(s, n);
}

void AV_JOIN(dbg_, skip_bits)(GetBitContext *s, int n, const char *file, int line, const char *func);
void AV_JOIN(dbg_, skip_bits)(GetBitContext *s, int n, const char *file, int line, const char *func)
{
    GB_DEBUG_LOG("[%s][%d][%s] %s(%d)\n", file, line, func, __func__, n);
    return skip_bits(s, n);
}

unsigned int AV_JOIN(dbg_, get_bits1)(GetBitContext *s, const char *file, int line, const char *func);
unsigned int AV_JOIN(dbg_, get_bits1)(GetBitContext *s, const char *file, int line, const char *func)
{
    GB_DEBUG_LOG("[%s][%d][%s] %s()\n", file, line, func, __func__);
    return get_bits1(s);
}

unsigned int AV_JOIN(dbg_, show_bits1)(GetBitContext *s, const char *file, int line, const char *func);
unsigned int AV_JOIN(dbg_, show_bits1)(GetBitContext *s, const char *file, int line, const char *func)
{
    GB_DEBUG_LOG("[%s][%d][%s] %s()\n", file, line, func, __func__);
    return show_bits1(s);
}

void AV_JOIN(dbg_, skip_bits1)(GetBitContext *s, const char *file, int line, const char *func);
void AV_JOIN(dbg_, skip_bits1)(GetBitContext *s, const char *file, int line, const char *func)
{
    GB_DEBUG_LOG("[%s][%d][%s] %s()\n", file, line, func, __func__);
    return skip_bits1(s);
}

unsigned int AV_JOIN(dbg_, get_bits_long)(GetBitContext *s, int n, const char *file, int line, const char *func);
unsigned int AV_JOIN(dbg_, get_bits_long)(GetBitContext *s, int n, const char *file, int line, const char *func)
{
    GB_DEBUG_LOG("[%s][%d][%s] %s(%d)\n", file, line, func, __func__, n);
    return get_bits_long(s, n);
}

uint64_t AV_JOIN(dbg_, get_bits64)(GetBitContext *s, int n, const char *file, int line, const char *func);
uint64_t AV_JOIN(dbg_, get_bits64)(GetBitContext *s, int n, const char *file, int line, const char *func)
{
    GB_DEBUG_LOG("[%s][%d][%s] %s(%d)\n", file, line, func, __func__, n);
    return get_bits64(s, n);
}

int AV_JOIN(dbg_, get_sbits_long)(GetBitContext *s, int n, const char *file, int line, const char *func);
int AV_JOIN(dbg_, get_sbits_long)(GetBitContext *s, int n, const char *file, int line, const char *func)
{
    GB_DEBUG_LOG("[%s][%d][%s] %s(%d)\n", file, line, func, __func__, n);
    return get_sbits_long(s, n);
}

unsigned int AV_JOIN(dbg_, show_bits_long)(GetBitContext *s, int n, const char *file, int line, const char *func);
unsigned int AV_JOIN(dbg_, show_bits_long)(GetBitContext *s, int n, const char *file, int line, const char *func)
{
    GB_DEBUG_LOG("[%s][%d][%s] %s(%d)\n", file, line, func, __func__, n);
    return show_bits_long(s, n);
}

int AV_JOIN(dbg_, check_marker)(void *logctx, GetBitContext *s, const char *msg, const char *file, int line, const char *func);
int AV_JOIN(dbg_, check_marker)(void *logctx, GetBitContext *s, const char *msg, const char *file, int line, const char *func)
{
    GB_DEBUG_LOG("[%s][%d][%s] %s()\n", file, line, func, __func__);
    return check_marker(logctx, s, msg);
}

int AV_JOIN(dbg_, init_get_bits)(GetBitContext *s, const uint8_t *buffer, int bit_size, const char *file, int line, const char *func);
int AV_JOIN(dbg_, init_get_bits)(GetBitContext *s, const uint8_t *buffer, int bit_size, const char *file, int line, const char *func)
{
    GB_DEBUG_LOG("[%s][%d][%s] %s()\n", file, line, func, __func__);
    return init_get_bits(s, buffer, bit_size);
}

int AV_JOIN(dbg_, init_get_bits8)(GetBitContext *s, const uint8_t *buffer, int byte_size, const char *file, int line, const char *func);
int AV_JOIN(dbg_, init_get_bits8)(GetBitContext *s, const uint8_t *buffer, int byte_size, const char *file, int line, const char *func)
{
    GB_DEBUG_LOG("[%s][%d][%s] %s()\n", file, line, func, __func__);
    return init_get_bits8(s, buffer, byte_size);
}

const uint8_t *AV_JOIN(dbg_, align_get_bits)(GetBitContext *s, const char *file, int line, const char *func);
const uint8_t *AV_JOIN(dbg_, align_get_bits)(GetBitContext *s, const char *file, int line, const char *func)
{
    GB_DEBUG_LOG("[%s][%d][%s] %s()\n", file, line, func, __func__);
    return align_get_bits(s);
}

int AV_JOIN(dbg_, get_vlc2)(GetBitContext *s, VLC_TYPE (*table)[2], int bits, int max_depth, const char *file, int line, const char *func);
int AV_JOIN(dbg_, get_vlc2)(GetBitContext *s, VLC_TYPE (*table)[2], int bits, int max_depth, const char *file, int line, const char *func)
{
    GB_DEBUG_LOG("[%s][%d][%s] %s()\n", file, line, func, __func__);
    return get_vlc2(s, table, bits, max_depth);
}

void AV_JOIN(dbg_, get_rl_vlc2)(
        int *plevel,
        int *prun,
        GetBitContext *s,
        RL_VLC_ELEM *table,
        int bits,
        int max_depth,
        int need_update,
        const char *file,
        int line,
        const char *func);
void AV_JOIN(dbg_, get_rl_vlc2)(
        int *plevel,
        int *prun,
        GetBitContext *s,
        RL_VLC_ELEM *table,
        int bits,
        int max_depth,
        int need_update,
        const char *file,
        int line,
        const char *func)
{
    GB_DEBUG_LOG("[%s][%d][%s] %s()\n", file, line, func, __func__);
    return get_rl_vlc2(plevel, prun, s, table, bits, max_depth, need_update);
}

void AV_JOIN(dbg_, get_cfhd_rl_vlc)(
        int *plevel,
        int *prun,
        GetBitContext *s,
        CFHD_RL_VLC_ELEM *table,
        int bits,
        int max_depth,
        int need_update,
        const char *file,
        int line,
        const char *func);
void AV_JOIN(dbg_, get_cfhd_rl_vlc)(
        int *plevel,
        int *prun,
        GetBitContext *s,
        CFHD_RL_VLC_ELEM *table,
        int bits,
        int max_depth,
        int need_update,
        const char *file,
        int line,
        const char *func)
{
    GB_DEBUG_LOG("[%s][%d][%s] %s()\n", file, line, func, __func__);
    return get_cfhd_rl_vlc(plevel, prun, s, table, bits, max_depth, need_update);
}

int AV_JOIN(dbg_, decode012)(GetBitContext *gb, const char *file, int line, const char *func);
int AV_JOIN(dbg_, decode012)(GetBitContext *gb, const char *file, int line, const char *func)
{
    GB_DEBUG_LOG("[%s][%d][%s] %s()\n", file, line, func, __func__);
    return decode012(gb);
}

int AV_JOIN(dbg_, decode210)(GetBitContext *gb, const char *file, int line, const char *func);
int AV_JOIN(dbg_, decode210)(GetBitContext *gb, const char *file, int line, const char *func)
{
    GB_DEBUG_LOG("[%s][%d][%s] %s()\n", file, line, func, __func__);
    return decode210(gb);
}

int AV_JOIN(dbg_, get_bits_left)(GetBitContext *gb, const char *file, int line, const char *func);
int AV_JOIN(dbg_, get_bits_left)(GetBitContext *gb, const char *file, int line, const char *func)
{
    return get_bits_left(gb);
}

int AV_JOIN(dbg_, skip_1stop_8data_bits)(GetBitContext *gb, const char *file, int line, const char *func);
int AV_JOIN(dbg_, skip_1stop_8data_bits)(GetBitContext *gb, const char *file, int line, const char *func)
{
    GB_DEBUG_LOG("[%s][%d][%s] %s()\n", file, line, func, __func__);
    return skip_1stop_8data_bits(gb);
}
#endif
