
/* ffedit: all bitstream readers are checked */
#undef UNCHECKED_BITSTREAM_READER
#define UNCHECKED_BITSTREAM_READER 0

#include "get_bits_common.h"

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
