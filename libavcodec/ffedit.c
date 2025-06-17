/*
 * Copyright (c) 2017-2022 Ramiro Polla
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <string.h>
#include "ffedit.h"

static const char *const feat_keys[] = {
    "info",
    "q_dct",
    "q_dct_delta",
    "q_dc",
    "q_dc_delta",
    "mv",
    "mv_delta",
    "qscale",
    "dqt",
    "dht",
    "mb",
    "gmc",
    "headers",
    "idat",
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
    "quantized DCT coefficients (with DC delta)",
    "quantized DCT coefficients (DC only)",
    "quantized DCT coefficients (DC delta only)",
    "motion vectors",
    "motion vectors (delta only)",
    "quantization scale",
    "quantization table",
    "huffman table",
    "macroblock",
    "global motion compensation",
    "headers",
    "image data",
};

const char *ffe_feat_desc(enum FFEditFeature feat)
{
    return feat_desc[feat];
}

static enum FFEditFeature const feat_excludes[][32] = {
    // FFEDIT_FEAT_INFO
    { FFEDIT_FEAT_LAST },
    // FFEDIT_FEAT_Q_DCT
    { FFEDIT_FEAT_Q_DCT_DELTA,
      FFEDIT_FEAT_Q_DC,
      FFEDIT_FEAT_Q_DC_DELTA,
      FFEDIT_FEAT_LAST },
    // FFEDIT_FEAT_Q_DCT_DELTA
    { FFEDIT_FEAT_Q_DCT,
      FFEDIT_FEAT_Q_DC,
      FFEDIT_FEAT_Q_DC_DELTA,
      FFEDIT_FEAT_LAST },
    // FFEDIT_FEAT_Q_DC
    { FFEDIT_FEAT_Q_DCT,
      FFEDIT_FEAT_Q_DCT_DELTA,
      FFEDIT_FEAT_Q_DC_DELTA,
      FFEDIT_FEAT_LAST },
    // FFEDIT_FEAT_Q_DC_DELTA
    { FFEDIT_FEAT_Q_DCT,
      FFEDIT_FEAT_Q_DCT_DELTA,
      FFEDIT_FEAT_Q_DC,
      FFEDIT_FEAT_LAST },
    // FFEDIT_FEAT_MV
    { FFEDIT_FEAT_MV_DELTA,
      FFEDIT_FEAT_LAST },
    // FFEDIT_FEAT_MV_DELTA
    { FFEDIT_FEAT_MV,
      FFEDIT_FEAT_LAST },
    // FFEDIT_FEAT_QSCALE
    { FFEDIT_FEAT_LAST },
    // FFEDIT_FEAT_DQT
    { FFEDIT_FEAT_LAST },
    // FFEDIT_FEAT_DHT
    { FFEDIT_FEAT_LAST },
    // FFEDIT_FEAT_MB
    { FFEDIT_FEAT_Q_DCT,
      FFEDIT_FEAT_Q_DC,
      FFEDIT_FEAT_MV,
      FFEDIT_FEAT_MV_DELTA,
      FFEDIT_FEAT_QSCALE,
      FFEDIT_FEAT_DQT,
      FFEDIT_FEAT_DHT,
      FFEDIT_FEAT_LAST },
    // FFEDIT_FEAT_GMC
    { FFEDIT_FEAT_LAST },
    // FFEDIT_FEAT_HEADERS
    { FFEDIT_FEAT_LAST },
    // FFEDIT_FEAT_IDAT
    { FFEDIT_FEAT_LAST },
};

int ffe_feat_excludes(enum FFEditFeature feat1, enum FFEditFeature feat2)
{
    for ( const enum FFEditFeature *p = feat_excludes[feat1]; *p != FFEDIT_FEAT_LAST; p++ )
        if ( *p == feat2 )
            return 1;
    return 0;
}
