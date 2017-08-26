#ifndef AVCODEC_GET_BITS_COMMON_H
#define AVCODEC_GET_BITS_COMMON_H

#include "libavutil/common.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/log.h"
#include "libavutil/avassert.h"
#include "avcodec.h"
#include "mathops.h"
#include "vlc.h"

struct PutBitContext;
typedef struct PutBitContext PutBitContext;

typedef struct GetBitContext {
    const uint8_t *buffer, *buffer_end;
    int index;
    int size_in_bits;
    int size_in_bits_plus8;
    PutBitContext *pb;
} GetBitContext;

#if defined LONG_BITSTREAM_READER
#   define MIN_CACHE_BITS 32
#else
#   define MIN_CACHE_BITS 25
#endif

#endif /* AVCODEC_GET_BITS_COMMON_H */
