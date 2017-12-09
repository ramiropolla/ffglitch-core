
#ifndef AVCODEC_CAS9_XP_H
#define AVCODEC_CAS9_XP_H

#include "avcodec.h"
#include "put_bits.h"
#include "get_bits.h"

typedef struct CAS9TransplicateContext {
    const AVClass *av_class;

    AVPacket *o_pkt;
    PutBitContext *o_pb;

    PutBitContext saved; /* saved */
} CAS9TransplicateContext;

int cas9_transplicate_init(
        AVCodecContext *avctx,
        CAS9TransplicateContext *xp,
        size_t pkt_size);

void cas9_transplicate_flush(
        AVCodecContext *avctx,
        CAS9TransplicateContext *xp,
        AVPacket *pkt);

void cas9_transplicate_free(CAS9TransplicateContext *xp);

PutBitContext *cas9_transplicate_pb(CAS9TransplicateContext *xp);

PutBitContext *cas9_transplicate_save(CAS9TransplicateContext *xp);

void cas9_transplicate_restore(
        CAS9TransplicateContext *xp,
        PutBitContext *saved);

#endif /* AVCODEC_CAS9_XP_H */
