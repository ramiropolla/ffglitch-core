
#ifndef AVUTIL_FFEDIT_H
#define AVUTIL_FFEDIT_H

/* ffedit features */
enum FFEditFeature {
    FFEDIT_FEAT_INFO,
    FFEDIT_FEAT_Q_DCT,
    FFEDIT_FEAT_Q_DC,
    FFEDIT_FEAT_MV,
    FFEDIT_FEAT_QSCALE,
    FFEDIT_FEAT_DQT,
    FFEDIT_FEAT_DHT,
    FFEDIT_FEAT_MB,

    FFEDIT_FEAT_LAST
};

enum FFEditFeature ffe_str_to_feat(const char *str);
const char *       ffe_feat_to_str(enum FFEditFeature feat);
const char *       ffe_feat_desc(enum FFEditFeature feat);
int                ffe_default_feat(enum FFEditFeature feat);

#endif /* AVUTIL_FFEDIT_H */
