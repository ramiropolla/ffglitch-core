
#ifndef AVUTIL_FFEDIT_JSON_H
#define AVUTIL_FFEDIT_JSON_H

#include "libavutil/json-c/json.h"
#include "libavutil/json-c/printbuf.h"

/* block arrays */
json_object *
ffe_jblock_new(
        int width,
        int height,
        json_object_to_json_string_fn line_func,
        void *ud);
json_object *
ffe_jmb_new(
        int mb_width,
        int mb_height,
        int nb_components,
        int *v_count,
        int *h_count,
        json_object_to_json_string_fn line_func,
        void *ud);
void
ffe_jmb_set_context(
        json_object *jso,
        int nb_components,
        int *v_count,
        int *h_count);
json_object *
ffe_jmb_get(
        json_object *jso,
        int component,
        int mb_y,
        int mb_x,
        int block);
void
ffe_jmb_set(
        json_object *jso,
        int component,
        int mb_y,
        int mb_x,
        int block,
        json_object *jval);
void
ffe_json_array_zero_fill(
        json_object *jso,
        size_t len);


/* helper printing functions */
int
ffe_int_line_to_json_string(
        json_object *jso,
        struct printbuf *pb,
        int level,
        int flags);

int
ffe_int2_line_to_json_string(
        json_object *jso,
        struct printbuf *pb,
        int level,
        int flags);

int
ffe_array_line_to_json_string(
        json_object *jso,
        struct printbuf *pb,
        int level,
        int flags);


/* simple av_free() wrapper */
void ffe_free_userdata(struct json_object *jso, void *userdata);

#endif /* AVUTIL_FFEDIT_JSON_H */
