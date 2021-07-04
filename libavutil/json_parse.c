/* Crappy JSON parser
 * Copyright (c) 2018-2021 Ramiro Polla
 * MIT License
 */

#include "json.h"

#include <stdlib.h>
#include <string.h>

//---------------------------------------------------------------------
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

//---------------------------------------------------------------------
#define MEM_CHUNK 4096
struct json_parse_mem_t {
    void *ptr;
    void *ptr2;
    size_t alloc;
    size_t len;
};
typedef struct json_parse_mem_t json_parse_mem_t;

static inline void grow_parse_mem(json_parse_mem_t *__mem, void **ptr, void **ptr2, size_t sz_ptr, size_t sz_ptr2, size_t len)
{
    uint8_t *__ptr = __mem->ptr;
    uint8_t *__ptr2 = __mem->ptr2;
    size_t __alloc = __mem->alloc;
    size_t __idx = __mem->len;
    __mem->len += len;
    if ( __mem->len > __alloc )
    {
        if ( len == 1 )
            __alloc += MEM_CHUNK;
        else
            while ( __mem->len > __alloc )
                __alloc += MEM_CHUNK;
        __ptr = realloc(__ptr, __alloc * sz_ptr);
        __mem->ptr = __ptr;
        if ( ptr2 != NULL )
        {
            __ptr2 = realloc(__ptr2, __alloc * sz_ptr2);
            __mem->ptr2 = __ptr2;
        }
        __mem->alloc = __alloc;
    }
    __ptr += __idx * sz_ptr;
    *ptr = __ptr;
    if ( ptr2 != NULL )
    {
        __ptr2 += __idx * sz_ptr2;
        *ptr2 = __ptr2;
    }
}

struct json_parse_ctx_t {
    json_ctx_t jctx;

    json_parse_mem_t array;
    json_parse_mem_t array_of_ints;
    json_parse_mem_t object;

    size_t str_alloc;
    size_t str_len;
    char *str;
};
typedef struct json_parse_ctx_t json_parse_ctx_t;

//---------------------------------------------------------------------
static const char *json_parse_element(json_parse_ctx_t *jpctx, json_t *jso, const char *buf);

static const char *skip_whitespace(const char *buf)
{
    while ( *buf == ' '
         || *buf == '\t'
         || *buf == '\r'
         || *buf == '\n' )
    {
        buf++;
    }
    return buf;
}

static const char *parse_string(json_parse_ctx_t *jpctx, const char *buf)
{
    char *__ptr = jpctx->str;
    size_t __alloc = jpctx->str_alloc;
    size_t len = 0;
    while ( *buf != '"' )
    {
        if ( unlikely((len + 3) > __alloc) )
        {
            __alloc += MEM_CHUNK;
            __ptr = realloc(__ptr, __alloc);
        }
        if ( *buf == '\0' )
        {
            return NULL;
        }
        else if ( *buf == '\\' )
        {
            buf++;
            switch ( *buf )
            {
                case '"':
                case '\\':
                case '/': __ptr[len++] = *buf; break;
                case 'b': __ptr[len++] = '\b'; break;
                case 'f': __ptr[len++] = '\f'; break;
                case 'n': __ptr[len++] = '\n'; break;
                case 'r': __ptr[len++] = '\r'; break;
                case 't': __ptr[len++] = '\t'; break;
                case 'u':
                    // Copy unicode as-is.
                    __ptr[len++] = '\\';
                    __ptr[len++] = *buf++;
                    break;
                default:
                    return NULL;
            }
            buf++;
        }
        else
        {
            __ptr[len++] = *buf++;
        }
    }
    __ptr[len] = '\0';
    jpctx->str = __ptr;
    jpctx->str_len = len;
    jpctx->str_alloc = __alloc;
    buf++;
    return buf;
}

static const char *json_parse_string(json_parse_ctx_t *jpctx, json_t *jso, const char *buf)
{
    jso->flags = JSON_TYPE_STRING;
    buf = parse_string(jpctx, buf);
    if ( buf != NULL )
        jso->str = json_allocator_strdup(&jpctx->jctx, jpctx->str, jpctx->str_len+1);
    return buf;
}

static inline const char *parse_number(json_parse_ctx_t *jpctx, int64_t *pout, const char *buf, uint64_t uval, int is_negative)
{
    while ( *buf >= '0' && *buf <= '9' )
        uval = (uval * 10) + *buf++ - '0';

    if ( unlikely(*buf == '.') )
        return NULL;

    *pout = is_negative ? -uval : uval;

    return buf;
}

static const char *json_parse_number(json_parse_ctx_t *jpctx, json_t *jso, uint64_t uval, const char *buf)
{
    jso->flags = JSON_TYPE_NUMBER;
    return parse_number(jpctx, &jso->val, buf, uval, 0);
}

static const char *json_parse_negative_number(json_parse_ctx_t *jpctx, json_t *jso, const char *buf)
{
    jso->flags = JSON_TYPE_NUMBER;
    return parse_number(jpctx, &jso->val, buf, 0, 1);
}

static const char *json_parse_null(json_parse_ctx_t *jpctx, json_t *jso, const char *buf)
{
    jso->flags = JSON_TYPE_NUMBER;
    jso->val = JSON_NULL;
    return buf + 4;
}

static const char *json_parse_object(json_parse_ctx_t *jpctx, json_t *jso, const char *buf)
{
    char **names;
    json_t **values;
    size_t orig_object_len;
    size_t len;
    buf = skip_whitespace(buf);
    if ( unlikely(*buf == '}') )
    {
        // Empty object.
        jso->flags = JSON_TYPE_OBJECT;
        jso->obj = json_allocator_get(&jpctx->jctx, sizeof(json_obj_t));
        jso->obj->names = NULL;
        jso->obj->values = NULL;
        buf++;
        return buf;
    }
    orig_object_len = jpctx->object.len;
    while ( 42 )
    {
        json_t *jval = alloc_json_t(&jpctx->jctx);
        char *name;

        if ( unlikely(*buf != '"') )
            return NULL;
        buf = parse_string(jpctx, buf+1);
        if ( unlikely(buf == NULL) )
            return NULL;
        name = json_allocator_strdup(&jpctx->jctx, jpctx->str, jpctx->str_len+1);
        buf = skip_whitespace(buf);
        if ( unlikely(*buf != ':') )
            return NULL;
        buf = skip_whitespace(buf+1);
        buf = json_parse_element(jpctx, jval, buf);
        if ( unlikely(buf == NULL) )
            return NULL;

        grow_parse_mem(&jpctx->object, (void *) &names, (void *) &values, sizeof(char *), sizeof(json_t *), 1);
        *names = name;
        *values = jval;

        buf = skip_whitespace(buf);
        if ( likely(*buf == '}') )
        {
            buf = skip_whitespace(buf+1);
            break;
        }
        else if ( likely(*buf == ',') )
        {
            buf = skip_whitespace(buf+1);
            continue;
        }
        else
        {
            return NULL;
        }
    }
    names = (char **) jpctx->object.ptr + orig_object_len;
    values = (json_t **) jpctx->object.ptr2 + orig_object_len;
    len = jpctx->object.len - orig_object_len;
    jso->flags = JSON_TYPE_OBJECT | len;
    jso->obj = json_allocator_get(&jpctx->jctx, sizeof(json_obj_t));
    jso->obj->names = json_allocator_dup(&jpctx->jctx, names, len * sizeof(char *));
    jso->obj->values = json_allocator_dup(&jpctx->jctx, values, len * sizeof(json_t *));
    jpctx->object.len = orig_object_len;
    return buf;
}

static const char *json_parse_array(json_parse_ctx_t *jpctx, json_t *jso, const char *buf)
{
    int64_t *array_of_ints;
    json_t **array;
    size_t orig_array_of_ints_len;
    size_t orig_array_len;
    json_t *jval;
    size_t len = 0;
    buf = skip_whitespace(buf);
    if ( unlikely(*buf == ']') )
    {
        // Empty array.
        jso->flags = JSON_TYPE_ARRAY;
        jso->array = NULL;
        buf++;
        return buf;
    }

#define PARSE_ARRAY_ELEMENT(ptr) do {           \
    buf = json_parse_element(jpctx, ptr, buf);  \
    if ( unlikely(buf == NULL) )                \
        return NULL;                            \
} while ( 0 )

#define PARSE_ARRAY_END() {                     \
    buf = skip_whitespace(buf);                 \
    if ( likely(*buf == ']') )                  \
    {                                           \
        buf = skip_whitespace(buf+1);           \
        break;                                  \
    }                                           \
    else if ( likely(*buf == ',') )             \
    {                                           \
        buf = skip_whitespace(buf+1);           \
        continue;                               \
    }                                           \
    else                                        \
    {                                           \
        return NULL;                            \
    }                                           \
} do { } while ( 0 )

    // Start with an array of ints.
    orig_array_of_ints_len = jpctx->array_of_ints.len;
    while ( 42 )
    {
        json_t jint;
        PARSE_ARRAY_ELEMENT(&jint);

        // Convert to normal array if any element is not a number.
        if ( JSON_TYPE(jint.flags) != JSON_TYPE_NUMBER )
        {
            jval = json_allocator_dup(&jpctx->jctx, &jint, sizeof(json_t));
            goto switch_to_normal_array;
        }

        grow_parse_mem(&jpctx->array_of_ints, (void *) &array_of_ints, NULL, sizeof(int64_t), 0, 1);
        *array_of_ints = jint.val;

        PARSE_ARRAY_END();
    }
    array_of_ints = (int64_t *) jpctx->array_of_ints.ptr + orig_array_of_ints_len;
    len = jpctx->array_of_ints.len - orig_array_of_ints_len;
    jso->flags = JSON_TYPE_ARRAY_OF_INTS | len;
    jso->array_of_ints = json_allocator_dup(&jpctx->jctx, array_of_ints, len * sizeof(int64_t));
    jpctx->array_of_ints.len = orig_array_of_ints_len;
    return buf;

switch_to_normal_array:
    // Normal array.
    len = jpctx->array_of_ints.len - orig_array_of_ints_len;
    orig_array_len = jpctx->array.len;
    if ( len > 0 )
    {
        json_t jint;
        grow_parse_mem(&jpctx->array, (void *) &array, NULL, sizeof(json_t *), 0, len);
        array_of_ints = (int64_t *) jpctx->array_of_ints.ptr + orig_array_of_ints_len;
        jint.flags = JSON_TYPE_NUMBER;
        for ( size_t i = 0; i < len; i++ )
        {
            jint.val = *array_of_ints++;
            *array++ = json_allocator_dup(&jpctx->jctx, &jint, sizeof(json_t));
        }
        jpctx->array_of_ints.len = orig_array_of_ints_len;
    }
    goto headstart;
    while ( 42 )
    {
        jval = alloc_json_t(&jpctx->jctx);
        PARSE_ARRAY_ELEMENT(jval);

headstart:
        grow_parse_mem(&jpctx->array, (void *) &array, NULL, sizeof(json_t *), 0, 1);
        *array = jval;

        PARSE_ARRAY_END();
    }
    array = (json_t **) jpctx->array.ptr + orig_array_len;
    len = jpctx->array.len - orig_array_len;
    jso->flags = JSON_TYPE_ARRAY | len;
    jso->array = json_allocator_dup(&jpctx->jctx, array, len * sizeof(json_t *));
    jpctx->array.len = orig_array_len;
    return buf;
}

static const char *json_parse_element(json_parse_ctx_t *jpctx, json_t *jso, const char *buf)
{
    switch ( *buf )
    {
        case '{':
            return json_parse_object(jpctx, jso, buf+1);
        case '[':
            return json_parse_array(jpctx, jso, buf+1);
        case '"':
            return json_parse_string(jpctx, jso, buf+1);
        case '-':
            return json_parse_negative_number(jpctx, jso, buf+1);
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return json_parse_number(jpctx, jso, *buf - '0', buf+1);
        case 'n':
            if ( buf[1] == 'u' && buf[2] == 'l' && buf[3] == 'l' )
                return json_parse_null(jpctx, jso, buf);
            break;
    }
    return NULL;
}

json_t *json_parse(json_ctx_t *jctx, const char *buf)
{
    json_t *jso = alloc_json_t(jctx);

    json_parse_ctx_t jpctx;
    jpctx.jctx = *jctx;
    jpctx.array.ptr = NULL;
    jpctx.array.alloc = 0;
    jpctx.array.len = 0;
    jpctx.array_of_ints.ptr = NULL;
    jpctx.array_of_ints.alloc = 0;
    jpctx.array_of_ints.len = 0;
    jpctx.object.ptr = NULL;
    jpctx.object.ptr2 = NULL;
    jpctx.object.alloc = 0;
    jpctx.object.len = 0;
    jpctx.str_alloc = MEM_CHUNK;
    jpctx.str = malloc(jpctx.str_alloc);
    buf = skip_whitespace(buf);
    buf = json_parse_element(&jpctx, jso, buf);
    *jctx = jpctx.jctx;
    free(jpctx.array.ptr);
    free(jpctx.array_of_ints.ptr);
    free(jpctx.object.ptr);
    free(jpctx.object.ptr2);
    free(jpctx.str);

    if ( buf == NULL )
        return NULL;
    buf = skip_whitespace(buf);
    if ( *buf != '\0' )
        return NULL;
    return jso;
}

//---------------------------------------------------------------------
typedef struct json_parse_error_ctx_t json_parse_error_ctx_t;

struct json_parse_error_ctx_t {
    const char *buf;
    const char *str;

    const char *line_start;
    size_t line_num;
};

static const char *json_error_element(json_parse_error_ctx_t *jpectx, const char *buf);

static void *set_error(json_parse_error_ctx_t *jpectx, const char *buf, const char *str)
{
    jpectx->buf = buf;
    jpectx->str = str;
    return NULL;
}

static const char *error_skip_whitespace(json_parse_error_ctx_t *jpectx, const char *buf)
{
    while ( *buf == ' '
         || *buf == '\t'
         || *buf == '\r'
         || *buf == '\n' )
    {
        int is_newline = (*buf == '\n');
        buf++;
        if ( is_newline )
        {
            jpectx->line_start = buf;
            jpectx->line_num++;
        }
    }
    return buf;
}

static const char *json_error_string(json_parse_error_ctx_t *jpectx, const char *buf)
{
    while ( *buf != '"' )
    {
        if ( *buf == '\0' )
        {
            return set_error(jpectx, buf, "unexpected end of file inside string");
        }
        else if ( *buf == '\\' )
        {
            buf++;
            switch ( *buf )
            {
                case '"':
                case '\\':
                case '/':
                case 'b':
                case 'f':
                case 'n':
                case 'r':
                case 't':
                    break;
                case 'u':
                    buf++;
                    break;
                default:
                    return set_error(jpectx, buf, "unexpected escaped character in string");
            }
            buf++;
        }
        else
        {
            buf++;
        }
    }
    buf++;
    return buf;
}

static const char *json_error_number(json_parse_error_ctx_t *jpectx, const char *buf)
{
    while ( *buf >= '0' && *buf <= '9' )
        buf++;

    if ( unlikely(*buf == '.') )
        return set_error(jpectx, buf, "floating point numbers are not supported");

    return buf;
}

static const char *json_error_null(json_parse_error_ctx_t *jpectx, const char *buf)
{
    return buf + 4;
}

static const char *json_error_object(json_parse_error_ctx_t *jpectx, const char *buf)
{
    buf = error_skip_whitespace(jpectx, buf);
    if ( unlikely(*buf == '}') )
        return buf+1;
    while ( 42 )
    {
        if ( unlikely(*buf != '"') )
            return set_error(jpectx, buf, "unexpected token (expected '\"')");
        buf = json_error_string(jpectx, buf+1);
        if ( unlikely(buf == NULL) )
            return NULL;
        buf = error_skip_whitespace(jpectx, buf);
        if ( unlikely(*buf != ':') )
            return set_error(jpectx, buf, "unexpected token (expected ':')");
        buf = error_skip_whitespace(jpectx, buf+1);
        buf = json_error_element(jpectx, buf);
        if ( unlikely(buf == NULL) )
            return NULL;
        buf = error_skip_whitespace(jpectx, buf);
        if ( likely(*buf == '}') )
        {
            buf = error_skip_whitespace(jpectx, buf+1);
            break;
        }
        else if ( likely(*buf == ',') )
        {
            buf = error_skip_whitespace(jpectx, buf+1);
            continue;
        }
        else
        {
            return set_error(jpectx, buf, "unexpected token (expected '}' or ',')");
        }
    }
    return buf;
}

static const char *json_error_array(json_parse_error_ctx_t *jpectx, const char *buf)
{
    buf = error_skip_whitespace(jpectx, buf);
    if ( unlikely(*buf == ']') )
        return buf+1;
    while ( 42 )
    {
        buf = json_error_element(jpectx, buf);
        if ( unlikely(buf == NULL) )
            return NULL;
        buf = error_skip_whitespace(jpectx, buf);
        if ( likely(*buf == ']') )
        {
            buf = error_skip_whitespace(jpectx, buf+1);
            break;
        }
        else if ( likely(*buf == ',') )
        {
            buf = error_skip_whitespace(jpectx, buf+1);
            continue;
        }
        else
        {
            return set_error(jpectx, buf, "unexpected token (expected ']' or ',')");
        }
    }
    return buf;
}

static const char *json_error_element(json_parse_error_ctx_t *jpectx, const char *buf)
{
    switch ( *buf )
    {
        case '{':
            return json_error_object(jpectx, buf+1);
        case '[':
            return json_error_array(jpectx, buf+1);
        case '"':
            return json_error_string(jpectx, buf+1);
        case '-':
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return json_error_number(jpectx, buf+1);
        case 'n':
            if ( buf[1] == 'u' && buf[2] == 'l' && buf[3] == 'l' )
                return json_error_null(jpectx, buf);
            break;
        case '\0':
            return set_error(jpectx, buf, "unexpected end of file");
    }
    return set_error(jpectx, buf, "unexpected token (expected '{', '[', '\"', '-', '0' to '9', or \"null\")");
}

static void fill_error_strings(json_error_ctx_t *jectx, json_parse_error_ctx_t *jpectx)
{
#define TOTAL_LINE_LEN 72
#define ELLIPSIS_LEN 6
#define CTX_LEN 3
    const char *line_start = jpectx->line_start;
    const char *buf = jpectx->buf;
    const char *ptr_in = NULL;

    int ellipsis_head = 0;
    char *ptr_out = malloc(TOTAL_LINE_LEN+1);
    char *col_out = malloc(TOTAL_LINE_LEN+1);
    int ellipsis_tail = 0;

    size_t offset = buf - line_start;
    size_t len = 0;

    // populate jectx from jpectx
    jectx->str = strdup(jpectx->str);
    jectx->buf = ptr_out;
    jectx->column = col_out;
    jectx->line = jpectx->line_num;
    jectx->offset = offset;

    // calculate length of string after buf (up to TOTAL_LINE_LEN)
    while ( len < TOTAL_LINE_LEN )
    {
        char c = buf[len];
        if ( c == '\n' )
            break;
        len++;
        if ( c == '\0' )
            break;
    }

    // determine ellipsis
    if ( (offset + len) < TOTAL_LINE_LEN )
    {
        // xxx
        ellipsis_head = 0;
        ptr_in = line_start;
        len = offset + len;
        ellipsis_tail = 0;
    }
    else if ( (offset + CTX_LEN + ELLIPSIS_LEN) < TOTAL_LINE_LEN )
    {
        // xxx [...]
        ellipsis_head = 0;
        ptr_in = line_start;
        len = TOTAL_LINE_LEN - ELLIPSIS_LEN;
        ellipsis_tail = 1;
    }
    else if ( (ELLIPSIS_LEN + CTX_LEN + len) < TOTAL_LINE_LEN )
    {
        // [...] xxx
        ellipsis_head = 1;
        ptr_in = buf + len - (TOTAL_LINE_LEN - ELLIPSIS_LEN);
        len = TOTAL_LINE_LEN - ELLIPSIS_LEN;
        ellipsis_tail = 0;
    }
    else
    {
        // [...] xxx [...]
        ellipsis_head = 1;
        ptr_in = buf - ((TOTAL_LINE_LEN - ELLIPSIS_LEN - ELLIPSIS_LEN) / 2);
        len = TOTAL_LINE_LEN - ELLIPSIS_LEN - ELLIPSIS_LEN;
        ellipsis_tail = 1;
    }

    // print
    if ( ellipsis_head )
    {
        strcpy(ptr_out, "[...] ");
        ptr_out += strlen(ptr_out);
        strcpy(col_out, "[...] ");
        col_out += strlen(col_out);
    }
    while ( len-- )
    {
        char c = *ptr_in;
        if ( c == '\t' || c == '\r' )
            *ptr_out++ = ' ';
        else
            *ptr_out++ = c;
        if ( ptr_in == buf )
            *col_out++ = '^';
        else
            *col_out++ = ' ';
        ptr_in++;
    }
    if ( ellipsis_tail )
    {
        strcpy(ptr_out, " [...]");
        ptr_out += strlen(ptr_out);
        strcpy(col_out, " [...]");
        col_out += strlen(col_out);
    }
    *ptr_out = '\0';
    *col_out = '\0';
#undef TOTAL_LINE_LEN
#undef ELLIPSIS_LEN
#undef CTX_LEN
}

// parse everything again, but this time keep track of line numbers and
// other valuable info for error reporting.
void json_error_parse(json_error_ctx_t *jectx, const char *buf)
{
    json_parse_error_ctx_t jpectx;
    jpectx.buf = NULL;
    jpectx.str = NULL;
    jpectx.line_start = buf;
    jpectx.line_num = 1;

    buf = error_skip_whitespace(&jpectx, buf);
    buf = json_error_element(&jpectx, buf);
    if ( buf != NULL )
    {
        buf = error_skip_whitespace(&jpectx, buf);
        if ( *buf == '\0' )
        {
            jectx->str = strdup("error looking for error (what?)");
            jectx->buf = NULL;
            jectx->column = NULL;
            return;
        }
        set_error(&jpectx, buf, "unexpected token (garbage found after end of parsing)");
    }

    fill_error_strings(jectx, &jpectx);
}

void json_error_free(json_error_ctx_t *jectx)
{
    free(jectx->str);
    free(jectx->buf);
    free(jectx->column);
}
