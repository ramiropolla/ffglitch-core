
#include <string.h>
#include <stdio.h>
#include <json.h>
#include <printbuf.h>

#include "cas9_json.h"

int
cas9_int_line_to_json_string(
        json_object *jso,
        struct printbuf *pb,
        int level,
        int flags)
{
    size_t length = json_object_array_length(jso);
    const char *fmt = json_object_get_userdata(jso);

    printbuf_strappend(pb, "[ ");

    for ( size_t i = 0; i < length; i++ )
    {
        json_object *jval = json_object_array_get_idx(jso, i);
        int val = json_object_get_int(jval);
        char sbuf[21];

        if ( i != 0 )
            printbuf_strappend(pb, ", ");

        snprintf(sbuf, sizeof(sbuf), fmt, val);
        printbuf_memappend(pb, sbuf, strlen(sbuf));
    }

    return printbuf_strappend(pb, " ]");
}

int
cas9_array_line_to_json_string(
        json_object *jso,
        struct printbuf *pb,
        int level,
        int flags)
{
    size_t length = json_object_array_length(jso);

    printbuf_strappend(pb, "[ ");

    for ( size_t i = 0; i < length; i++ )
    {
        json_object *jval = json_object_array_get_idx(jso, i);
        const char *s = json_object_to_json_string_ext(jval, JSON_C_TO_STRING_PRETTY);

        if ( i != 0 )
            printbuf_strappend(pb, ", ");

        printbuf_memappend(pb, s, strlen(s));
    }

    return printbuf_strappend(pb, " ]");
}
