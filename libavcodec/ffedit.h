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

#ifndef AVCODEC_FFEDIT_H
#define AVCODEC_FFEDIT_H

#include "libavutil/log.h"

/* ffedit features */
enum FFEditFeature {
    FFEDIT_FEAT_INFO,
    FFEDIT_FEAT_Q_DCT,
    FFEDIT_FEAT_Q_DCT_DELTA,
    FFEDIT_FEAT_Q_DC,
    FFEDIT_FEAT_Q_DC_DELTA,
    FFEDIT_FEAT_MV,
    FFEDIT_FEAT_MV_DELTA,
    FFEDIT_FEAT_QSCALE,
    FFEDIT_FEAT_DQT,
    FFEDIT_FEAT_DHT,
    FFEDIT_FEAT_MB,
    FFEDIT_FEAT_GMC,
    FFEDIT_FEAT_HEADERS,
    FFEDIT_FEAT_IDAT,

    FFEDIT_FEAT_LAST
};

enum FFEditFeature ffe_str_to_feat(const char *str);
const char *       ffe_feat_to_str(enum FFEditFeature feat);
const char *       ffe_feat_desc(enum FFEditFeature feat);
int                ffe_feat_excludes(enum FFEditFeature feat1, enum FFEditFeature feat2);

#endif /* AVUTIL_FFEDIT_H */
