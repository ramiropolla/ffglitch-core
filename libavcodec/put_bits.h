#ifndef AVCODEC_PUT_BITS_H
#define AVCODEC_PUT_BITS_H

#include "put_bits_common.h"

#ifdef BITSTREAM_WRITER_LE
# define PB_PREFIX pb_le_
#else /* BITSTREAM_WRITER_LE */
# define PB_PREFIX pb_be_
#endif /* BITSTREAM_WRITER_LE */

#include "put_bits_funcs.h"

#endif /* AVCODEC_PUT_BITS_H */
