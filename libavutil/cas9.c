
#include <string.h>
#include "cas9.h"

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

enum CAS9Feature cas9_str_to_feat(const char *str)
{
    for ( size_t i = 0; i < CAS9_FEAT_LAST; i++ )
        if ( strcmp(feat_keys[i], str) == 0 )
            return i;
    return CAS9_FEAT_LAST;
}

const char *cas9_feat_to_str(enum CAS9Feature feat)
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

const char *cas9_feat_desc(enum CAS9Feature feat)
{
    return feat_desc[feat];
}

int cas9_default_feat(enum CAS9Feature feat)
{
    switch ( feat )
    {
        case CAS9_FEAT_Q_DCT:
        case CAS9_FEAT_MB:
            return 0;
        default:
            return 1;
    }
}
