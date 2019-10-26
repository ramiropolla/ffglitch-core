#ifndef AVCODEC_PUT_BITS_COMMON_H
#define AVCODEC_PUT_BITS_COMMON_H

#include <stdint.h>
#include <stddef.h>

#include "libavutil/intreadwrite.h"
#include "libavutil/avassert.h"

typedef struct PutBitContext {
    uint32_t bit_buf;
    int bit_left;
    uint8_t *buf, *buf_ptr, *buf_end;
    int size_in_bits;
} PutBitContext;

#ifdef BITSTREAM_WRITER_LE
#define avpriv_align_put_bits align_put_bits_unsupported_here
#define avpriv_put_string ff_put_string_unsupported_here
#define avpriv_copy_bits avpriv_copy_bits_unsupported_here
#else
/**
 * Pad the bitstream with zeros up to the next byte boundary.
 */
void avpriv_align_put_bits(PutBitContext *s);

/**
 * Put the string string in the bitstream.
 *
 * @param terminate_string 0-terminates the written string if value is 1
 */
void avpriv_put_string(PutBitContext *pb, const char *string,
                       int terminate_string);

/**
 * Copy the content of src to the bitstream.
 *
 * @param length the number of bits of src to copy
 */
void avpriv_copy_bits(PutBitContext *pb, const uint8_t *src, int length);
#endif

#endif /* AVCODEC_PUT_BITS_COMMON_H */
