
#ifndef AVCODEC_FFEDIT_XP_H
#define AVCODEC_FFEDIT_XP_H

#include "avcodec.h"
#include "put_bits.h"
#include "get_bits.h"

typedef struct FFEditTransplicateContext {
    const AVClass *av_class;

    AVPacket *o_pkt;
    PutBitContext *o_pb;

    PutBitContext saved; /* saved */
} FFEditTransplicateContext;

int ffe_transplicate_init(
        AVCodecContext *avctx,
        FFEditTransplicateContext *xp,
        size_t pkt_size);

void ffe_transplicate_flush(
        AVCodecContext *avctx,
        FFEditTransplicateContext *xp,
        AVPacket *pkt);

void ffe_transplicate_free(FFEditTransplicateContext *xp);

PutBitContext *ffe_transplicate_pb(FFEditTransplicateContext *xp);

PutBitContext *ffe_transplicate_save(FFEditTransplicateContext *xp);

void ffe_transplicate_restore(
        FFEditTransplicateContext *xp,
        PutBitContext *saved);

#endif /* AVCODEC_FFEDIT_XP_H */
