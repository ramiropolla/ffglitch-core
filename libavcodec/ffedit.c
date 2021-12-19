
#include <string.h>
#include "ffedit.h"

static const char *const feat_keys[] = {
    "info",
    "q_dct",
    "q_dc",
    "mv",
    "mv_delta",
    "qscale",
    "dqt",
    "dht",
    "mb",
};

enum FFEditFeature ffe_str_to_feat(const char *str)
{
    for ( size_t i = 0; i < FFEDIT_FEAT_LAST; i++ )
        if ( strcmp(feat_keys[i], str) == 0 )
            return i;
    return FFEDIT_FEAT_LAST;
}

const char *ffe_feat_to_str(enum FFEditFeature feat)
{
    return feat_keys[feat];
}

static const char *const feat_desc[] = {
    "info",
    "quantized DCT coefficients",
    "quantized DCT coefficients (DC only)",
    "motion vectors",
    "motion vectors (delta only)",
    "quantization scale",
    "quantization table",
    "huffman table",
    "macroblock",
};

const char *ffe_feat_desc(enum FFEditFeature feat)
{
    return feat_desc[feat];
}

int ffe_default_feat(enum FFEditFeature feat)
{
    switch ( feat )
    {
        case FFEDIT_FEAT_Q_DCT:
        case FFEDIT_FEAT_MV_DELTA:
        case FFEDIT_FEAT_MB:
            return 0;
        default:
            return 1;
    }
}

static enum FFEditFeature const feat_excludes[][32] = {
    { FFEDIT_FEAT_LAST }, // INFO
    { FFEDIT_FEAT_LAST }, // Q_DCT
    { FFEDIT_FEAT_LAST }, // Q_DC
    { FFEDIT_FEAT_MV_DELTA,
      FFEDIT_FEAT_LAST }, // MV
    { FFEDIT_FEAT_MV,
      FFEDIT_FEAT_LAST }, // MV_DELTA
    { FFEDIT_FEAT_LAST }, // QSCALE
    { FFEDIT_FEAT_LAST }, // DQT
    { FFEDIT_FEAT_LAST }, // DHT
    { FFEDIT_FEAT_Q_DCT,  // MB
      FFEDIT_FEAT_Q_DC,
      FFEDIT_FEAT_MV,
      FFEDIT_FEAT_MV_DELTA,
      FFEDIT_FEAT_QSCALE,
      FFEDIT_FEAT_DQT,
      FFEDIT_FEAT_DHT,
      FFEDIT_FEAT_LAST },
};

int ffe_feat_excludes(enum FFEditFeature feat1, enum FFEditFeature feat2)
{
    for ( enum FFEditFeature *p = feat_excludes[feat1]; *p != FFEDIT_FEAT_LAST; p++ )
        if ( *p == feat2 )
            return 1;
    return 0;
}
