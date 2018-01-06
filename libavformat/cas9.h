
#ifndef AVFORMAT_CAS9_H
#define AVFORMAT_CAS9_H

#include "avformat.h"

/* forward declarations */
struct CAS9OutputContext;
typedef struct CAS9OutputContext CAS9OutputContext;

/*----------------------------- PACKETS -----------------------------*/
typedef struct cas9_packet {
    int64_t i_pos;
    size_t i_size;

    uint8_t *data;
    size_t o_size;
} cas9_packet;

int cas9_output_packet(
        CAS9OutputContext *c9,
        int64_t i_pos,
        size_t i_size,
        uint8_t *data,
        size_t o_size);

/*----------------------------- FIXUPS ------------------------------*/
enum CAS9FixupType {
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
    CAS9_FIXUP_OFFSET,
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
    CAS9_FIXUP_SIZE,
};

typedef struct cas9_fixup {
    int64_t pos;
    int64_t val;

    int64_t a1;
    int64_t a2;

    enum CAS9FixupType type;
} cas9_fixup;

int cas9_output_fixup(
        CAS9OutputContext *c9,
        enum CAS9FixupType type,
        int64_t pos,
        int64_t val,
        int64_t a1,
        int64_t a2);

/*----------------------------- OUTPUT ------------------------------*/
typedef struct CAS9OutputContext {
    const AVClass *av_class;

    AVFormatContext *fctx;
    AVIOContext *o_pb;

    cas9_packet *packets;
    size_t    nb_packets;

    cas9_fixup *fixups;
    size_t   nb_fixups;
} CAS9OutputContext;

int cas9_output_open(
        CAS9OutputContext **pc9,
        AVFormatContext *fctx,
        const char *filename);

int cas9_output_flush(CAS9OutputContext *c9);

void cas9_output_freep(CAS9OutputContext **pc9);

#endif /* AVFORMAT_CAS9_H */
