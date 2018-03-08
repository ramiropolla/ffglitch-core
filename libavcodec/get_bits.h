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

#include "get_bits_funcs.h"

#endif /* AVCODEC_GET_BITS_H */
