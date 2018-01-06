
#ifndef AVUTIL_CAS9_JSON_H
#define AVUTIL_CAS9_JSON_H

#include <json.h>
#include <printbuf.h>

/* block arrays */
json_object *
cas9_jblock_new(
        int width,
        int height,
        json_object_to_json_string_fn line_func,
        void *ud);
json_object *
cas9_jmb_new(
        int mb_width,
        int mb_height,
        int nb_components,
        int *v_count,
        int *h_count,
        json_object_to_json_string_fn line_func,
        void *ud);
void
cas9_jmb_set_context(
        json_object *jso,
        int nb_components,
        int *v_count,
        int *h_count);
json_object *
cas9_jmb_get(
        json_object *jso,
        int component,
        int mb_y,
        int mb_x,
        int block);
void
cas9_jmb_set(
        json_object *jso,
        int component,
        int mb_y,
        int mb_x,
        int block,
        json_object *jval);
void
cas9_json_array_zero_fill(
        json_object *jso,
        size_t len);


/* helper printing functions */
int
cas9_int_line_to_json_string(
        json_object *jso,
        struct printbuf *pb,
        int level,
        int flags);

int
cas9_int2_line_to_json_string(
        json_object *jso,
        struct printbuf *pb,
        int level,
        int flags);

int
cas9_array_line_to_json_string(
        json_object *jso,
        struct printbuf *pb,
        int level,
        int flags);


/* simple av_free() wrapper */
void cas9_free_userdata(struct json_object *jso, void *userdata);

#endif /* AVUTIL_CAS9_JSON_H */
