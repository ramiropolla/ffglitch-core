
#ifndef AVUTIL_FFEDIT_JSON_H
#define AVUTIL_FFEDIT_JSON_H

#include "libavutil/json.h"

/* block arrays */
json_t *
ffe_jblock_new(
        json_ctx_t *jctx,
        int width,
        int height,
        int pflags);
json_t *
ffe_jmb_new(
        json_ctx_t *jctx,
        int mb_width,
        int mb_height,
        int nb_components,
        int *v_count,
        int *h_count,
        int pflags);
void
ffe_jmb_set_context(
        json_t *jso,
        int nb_components,
        int *v_count,
        int *h_count);
json_t *
ffe_jmb_get(
        json_t *jso,
        int component,
        int mb_y,
        int mb_x,
        int block);
void
ffe_jmb_set(
        json_t *jso,
        int component,
        int mb_y,
        int mb_x,
        int block,
        json_t *jval);
void
ffe_json_array_zero_fill(
        json_ctx_t *jctx,
        json_t *jso,
        size_t len);


#endif /* AVUTIL_FFEDIT_JSON_H */
