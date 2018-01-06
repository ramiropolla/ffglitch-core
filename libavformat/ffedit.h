
#ifndef AVFORMAT_FFEDIT_H
#define AVFORMAT_FFEDIT_H

#include "avformat.h"

/* forward declarations */
struct FFEditOutputContext;
typedef struct FFEditOutputContext FFEditOutputContext;

/*----------------------------- PACKETS -----------------------------*/
typedef struct FFEditPacket {
    int64_t i_pos;
    size_t i_size;

    uint8_t *data;
    size_t o_size;

    int alignment;
} FFEditPacket;

int ffe_output_packet(
        FFEditOutputContext *ectx,
        int64_t i_pos,
        size_t i_size,
        uint8_t *data,
        size_t o_size);

int ffe_output_padding(
        FFEditOutputContext *ectx,
        int64_t i_pos,
        size_t i_size,
        uint8_t val,
        int alignment);

/*----------------------------- FIXUPS ------------------------------*/
enum FFEditFixupType {
    /*
     * fixup to an (absolute) offset in the file
     *
     * a1: points_to_here
     * a2: (unused)
     *
     *    .pos  .points_to_here
     * .------------------------.
     * |  |     |               |
     * '--|-----|---------------'
     *    '-->--'
     *    .points_to_here   .pos
     * .------------------------.
     * |  |                 |   |
     * '--|-----------------|---'
     *    '--------<--------'
     */
    FFEDIT_FIXUP_OFFSET,
    /*
     * fixup to a field with size information
     * - range_start points to the first byte of the range
     * - range_end points to the last byte of the range
     *
     * a1: range_start
     * a2: range_end
     *
     *    .pos .range_start
     * .-----------------------.
     * |  |    |<---->|        |
     * '-----------------------'
     *                .range_end
     */
    FFEDIT_FIXUP_SIZE,
};

typedef struct FFEditFixup {
    int64_t pos;
    int64_t val;

    int64_t a1;
    int64_t a2;

    enum FFEditFixupType type;
} FFEditFixup;

int ffe_output_fixup(
        FFEditOutputContext *ectx,
        enum FFEditFixupType type,
        int64_t pos,
        int64_t val,
        int64_t a1,
        int64_t a2);

/*----------------------------- OUTPUT ------------------------------*/
typedef struct FFEditOutputContext {
    const AVClass *av_class;

    AVFormatContext *fctx;
    AVIOContext *o_pb;

    FFEditPacket *packets;
    size_t     nb_packets;

    FFEditFixup *fixups;
    size_t    nb_fixups;
} FFEditOutputContext;

int ffe_output_open(
        FFEditOutputContext **pectx,
        AVFormatContext *fctx,
        const char *filename);

int ffe_output_flush(FFEditOutputContext *ectx);

void ffe_output_freep(FFEditOutputContext **pectx);

#endif /* AVFORMAT_FFEDIT_H */
