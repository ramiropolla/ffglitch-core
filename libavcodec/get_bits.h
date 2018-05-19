#ifndef AVCODEC_GET_BITS_H
#define AVCODEC_GET_BITS_H

#include "get_bits_common.h"

#ifdef BITSTREAM_READER_LE
# ifdef LONG_BITSTREAM_READER
#  define GB_PREFIX gb_le_long_
# else /* LONG_BITSTREAM_READER */
#  define GB_PREFIX gb_le_
# endif /* LONG_BITSTREAM_READER */
#else /* BITSTREAM_READER_LE */
# ifdef LONG_BITSTREAM_READER
#  define GB_PREFIX gb_be_long_
# else /* LONG_BITSTREAM_READER */
#  define GB_PREFIX gb_be_
# endif /* LONG_BITSTREAM_READER */
#endif /* BITSTREAM_READER_LE */

#if CONFIG_FFEDIT_XP_DEBUG
#include "get_bits_funcs_dbg.h"
#else
#include "get_bits_funcs.h"
#endif

#if CONFIG_FFEDIT_XP_DEBUG
#undef get_bits_count
#undef get_xbits
#undef get_xbits_le
#undef get_sbits
#undef get_bits
#undef get_bitsz
#undef get_bits_le
#undef show_bits
#undef skip_bits
#undef get_bits1
#undef show_bits1
#undef skip_bits1
#undef get_bits_long
#undef skip_bits_long
#undef get_bits64
#undef get_sbits_long
#undef show_bits_long
#undef check_marker
#undef init_get_bits
#undef init_get_bits8
#undef init_get_bits8_le
#undef align_get_bits
#undef get_vlc2
#undef get_rl_vlc2
#undef get_cfhd_rl_vlc
#undef decode012
#undef decode210
#undef get_bits_left
#undef skip_1stop_8data_bits

#define get_bits_count(s)                    AV_JOIN3(dbg_, GB_PREFIX, get_bits_count)(s, __FILE__, __LINE__, __func__)
#define get_xbits(s, n)                      AV_JOIN3(dbg_, GB_PREFIX, get_xbits)(s, n, __FILE__, __LINE__, __func__)
#define get_xbits_le(s, n)                   AV_JOIN3(dbg_, GB_PREFIX, get_xbits_le)(s, n, __FILE__, __LINE__, __func__)
#define get_sbits(s, n)                      AV_JOIN3(dbg_, GB_PREFIX, get_sbits)(s, n, __FILE__, __LINE__, __func__)
#define get_bits(s, n)                       AV_JOIN3(dbg_, GB_PREFIX, get_bits)(s, n, __FILE__, __LINE__, __func__)
#define get_bitsz(s, n)                      AV_JOIN3(dbg_, GB_PREFIX, get_bitsz)(s, n, __FILE__, __LINE__, __func__)
#define get_bits_le(s, n)                    AV_JOIN3(dbg_, GB_PREFIX, get_bits_le)(s, n, __FILE__, __LINE__, __func__)
#define show_bits(s, n)                      AV_JOIN3(dbg_, GB_PREFIX, show_bits)(s, n, __FILE__, __LINE__, __func__)
#define skip_bits(s, n)                      AV_JOIN3(dbg_, GB_PREFIX, skip_bits)(s, n, __FILE__, __LINE__, __func__)
#define get_bits1(s)                         AV_JOIN3(dbg_, GB_PREFIX, get_bits1)(s, __FILE__, __LINE__, __func__)
#define show_bits1(s)                        AV_JOIN3(dbg_, GB_PREFIX, show_bits1)(s, __FILE__, __LINE__, __func__)
#define skip_bits1(s)                        AV_JOIN3(dbg_, GB_PREFIX, skip_bits1)(s, __FILE__, __LINE__, __func__)
#define get_bits_long(s, n)                  AV_JOIN3(dbg_, GB_PREFIX, get_bits_long)(s, n, __FILE__, __LINE__, __func__)
#define skip_bits_long(s, n)                 AV_JOIN3(dbg_, GB_PREFIX, skip_bits_long)(s, n, __FILE__, __LINE__, __func__)
#define get_bits64(s, n)                     AV_JOIN3(dbg_, GB_PREFIX, get_bits64)(s, n, __FILE__, __LINE__, __func__)
#define get_sbits_long(s, n)                 AV_JOIN3(dbg_, GB_PREFIX, get_sbits_long)(s, n, __FILE__, __LINE__, __func__)
#define show_bits_long(s, n)                 AV_JOIN3(dbg_, GB_PREFIX, show_bits_long)(s, n, __FILE__, __LINE__, __func__)
#define check_marker(l, s, m)                AV_JOIN3(dbg_, GB_PREFIX, check_marker)(l, s, m, __FILE__, __LINE__, __func__)
#define init_get_bits(s, b, sz)              AV_JOIN3(dbg_, GB_PREFIX, init_get_bits)(s, b, sz, __FILE__, __LINE__, __func__)
#define init_get_bits8(s, b, sz)             AV_JOIN3(dbg_, GB_PREFIX, init_get_bits8)(s, b, sz, __FILE__, __LINE__, __func__)
#define init_get_bits8_le(s, b, sz)          AV_JOIN3(dbg_, GB_PREFIX, init_get_bits8_le)(s, b, sz, __FILE__, __LINE__, __func__)
#define align_get_bits(s)                    AV_JOIN3(dbg_, GB_PREFIX, align_get_bits)(s, __FILE__, __LINE__, __func__)
#define get_vlc2(s, t, b, m)                 AV_JOIN3(dbg_, GB_PREFIX, get_vlc2)(s, t, b, m, __FILE__, __LINE__, __func__)
#define get_rl_vlc2(l, r, s, t, b, m, n)     AV_JOIN3(dbg_, GB_PREFIX, get_rl_vlc2)(l, r, s, t, b, m, n, __FILE__, __LINE__, __func__)
#define get_cfhd_rl_vlc(l, r, s, t, b, m, n) AV_JOIN3(dbg_, GB_PREFIX, get_cfhd_rl_vlc)(l, r, s, t, b, m, n, __FILE__, __LINE__, __func__)
#define decode012(s)                         AV_JOIN3(dbg_, GB_PREFIX, decode012)(s, __FILE__, __LINE__, __func__)
#define decode210(s)                         AV_JOIN3(dbg_, GB_PREFIX, decode210)(s, __FILE__, __LINE__, __func__)
#define get_bits_left(s)                     AV_JOIN3(dbg_, GB_PREFIX, get_bits_left)(s, __FILE__, __LINE__, __func__)
#define skip_1stop_8data_bits(s)             AV_JOIN3(dbg_, GB_PREFIX, skip_1stop_8data_bits)(s, __FILE__, __LINE__, __func__)
#endif

#endif /* AVCODEC_GET_BITS_H */
