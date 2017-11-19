
/* ffedit: all bitstream readers are checked */
#undef UNCHECKED_BITSTREAM_READER
#define UNCHECKED_BITSTREAM_READER 0

#include "get_bits_common.h"
#include "put_bits.h"

//#define FFEDIT_GB_DEBUG

#ifdef FFEDIT_GB_DEBUG
static void gb_debug(
        const GetBitContext *s,
        int val,
        int orig_count,
        int new_count,
        int is_cache)
{
    int diff = new_count - orig_count;
    char buf[33];
    char *ptr = buf;
    int pbcount = 0;
    // TODO check >32 (skip)
    if ( diff == 0 || diff > 32 )
        return;
    if ( is_cache )
        val >>= 32 - diff;
    for ( int i = 0; i < 32 - diff; i++ )
        *ptr++ = ' ';
    for ( int i = 0; i < diff; i++ )
    {
        int bit = val & (1<<(diff-i-1));
        *ptr++ = !!bit + '0';
    }
    *ptr = '\0';
    if ( s->pb != NULL )
        pbcount = put_bits_count(s->pb);
    av_log(NULL, AV_LOG_ERROR, "%-32s [gb:%6d] [pb:%6d] (%2d)\n",
           buf, orig_count, pbcount - diff, diff);
}
#define GB_DEBUG_START(s) int orig_count = get_bits_count(s)
#define GB_DEBUG_END(s, val) gb_debug(s, val, orig_count, get_bits_count(s), 0)
#define GB_DEBUG_END_SHOW(s, val, n) gb_debug(s, val, orig_count, orig_count + n, 0)
#define GB_DEBUG_END_CACHE(s, val) gb_debug(s, val, orig_count, get_bits_count(s), 1)
#else
#define GB_DEBUG_START(s)
#define GB_DEBUG_END(s, val)
#define GB_DEBUG_END_SHOW(s, val, n)
#define GB_DEBUG_END_CACHE(s, val)
#endif

#define GB_PREFIX gb_be_
// #define BITSTREAM_READER_LE
// #define LONG_BITSTREAM_READER
#include "get_bits_impl.h"

#undef GB_PREFIX
#define GB_PREFIX gb_le_
#define BITSTREAM_READER_LE
// #define LONG_BITSTREAM_READER
#include "get_bits_impl.h"

#undef GB_PREFIX
#define GB_PREFIX gb_be_long_
// #define BITSTREAM_READER_LE
#define LONG_BITSTREAM_READER
#include "get_bits_impl.h"

#undef GB_PREFIX
#define GB_PREFIX gb_le_long_
#define BITSTREAM_READER_LE
#define LONG_BITSTREAM_READER
#include "get_bits_impl.h"
