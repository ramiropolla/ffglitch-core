
#include <string.h>
#include "ffedit.h"

static const char *const feat_keys[] = {
    "info",
    "q_dct",
    "q_dc",
    "mv",
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
        case FFEDIT_FEAT_MB:
            return 0;
        default:
            return 1;
    }
}
