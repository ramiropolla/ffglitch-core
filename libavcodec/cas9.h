
#ifndef AVUTIL_CAS9_H
#define AVUTIL_CAS9_H

/* cas9 features */
enum CAS9Feature {
    CAS9_FEAT_INFO,
    CAS9_FEAT_Q_DCT,
    CAS9_FEAT_Q_DC,
    CAS9_FEAT_MV,
    CAS9_FEAT_QSCALE,
    CAS9_FEAT_DQT,
    CAS9_FEAT_DHT,
    CAS9_FEAT_MB,

    CAS9_FEAT_LAST
};

enum CAS9Feature cas9_str_to_feat(const char *str);
const char *     cas9_feat_to_str(enum CAS9Feature feat);
const char *     cas9_feat_desc(enum CAS9Feature feat);
int              cas9_default_feat(enum CAS9Feature feat);

#endif /* AVUTIL_CAS9_H */
