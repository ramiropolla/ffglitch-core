
#include "put_bits_common.h"
#include "get_bits.h"

#define PB_PREFIX pb_be_
// #define BITSTREAM_WRITER_LE
#include "put_bits_impl.h"

#undef PB_PREFIX
#define PB_PREFIX pb_le_
#define BITSTREAM_WRITER_LE
#include "put_bits_impl.h"
