
#ifndef AVUTIL_CAS9_JSON_H
#define AVUTIL_CAS9_JSON_H

/* helper printing functions */
int
cas9_int_line_to_json_string(
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

#endif /* AVUTIL_CAS9_JSON_H */
