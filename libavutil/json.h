/* Crappy JSON parser
 * Copyright (c) 2018-2019 Ramiro Polla
 * MIT License
 */

#ifndef LAVU_JSON_H
#define LAVU_JSON_H

#include <stdio.h>
#include <inttypes.h>

typedef struct json_t json_t;

typedef struct {
    char **names;
    json_t **values;
} json_obj_t;

#define JSON_NULL 0x8000000000000000ULL

struct json_t {
#define JSON_TYPE_OBJECT        0x00010000
#define JSON_TYPE_ARRAY         0x00020000
#define JSON_TYPE_ARRAY_OF_INTS 0x00040000
#define JSON_TYPE_STRING        0x00080000
#define JSON_TYPE_NUMBER        0x00100000
#define JSON_TYPE_BOOL          0x00200000
#define JSON_PFLAGS_NO_LF       0x01000000
#define JSON_PFLAGS_NO_SPACE    0x02000000
#define JSON_LEN(x)    (x & 0x0000FFFF)
#define JSON_TYPE(x)   (x & 0x00FF0000)
#define JSON_PFLAGS(x) (x & 0xFF000000)
    uint64_t flags;
    void *userdata;
    union {
        char *str;
        int64_t val;
        json_t **array;
        int64_t *array_of_ints;
        json_obj_t *obj;
    };
};

//---------------------------------------------------------------------
// json context

#define DATA_CHUNK (32*1024*1024) // 32Mb
#define STR_CHUNK (1024*1024) // 1Mb
#define OBJECT_CHUNK 16
#define ARRAY_CHUNK 64

typedef struct {
    void **chunks;
    uint8_t *ptr;
    size_t len;
    size_t bytes_left;
} json_allocator_t;

typedef struct {
    json_allocator_t data;
    json_allocator_t str;

    const char *error;
} json_ctx_t;

void json_ctx_start(json_ctx_t *jctx);
void *json_allocator_get(json_ctx_t *jctx, size_t len);
void *json_allocator_get0(json_ctx_t *jctx, size_t len);
void *json_allocator_dup(json_ctx_t *jctx, const void *src, size_t len);
void *json_allocator_strget(json_ctx_t *jctx, size_t len);
void *json_allocator_strdup(json_ctx_t *jctx, const void *src, size_t len);
void json_ctx_free(json_ctx_t *jctx);
json_t *alloc_json_t(json_ctx_t *jctx);

//---------------------------------------------------------------------
// parser
json_t *json_parse(json_ctx_t *jctx, char *buf);
const char *json_parse_error(json_ctx_t *jctx);

//---------------------------------------------------------------------
// dynamic
void json_userdata_set(json_t *jso, void *userdata);
void *json_userdata_get(json_t *jso);

json_t *json_array_new(json_ctx_t *jctx);
int json_array_add(json_t *jso, json_t *jval);
int json_array_alloc(json_ctx_t *jctx, json_t *jso, size_t len);
int json_array_set(json_t *jso, size_t idx, json_t *jval);
int json_array_set_int(json_ctx_t *jctx, json_t *jso, size_t idx, int64_t val);
json_t *json_array_get(json_t *jso, size_t idx);
int64_t json_array_get_int(json_t *jso, size_t idx);
size_t json_array_length(json_t *jso);
void json_array_sort(json_t *jso, int (* sort_fn)(const void *, const void *));
int json_array_done(json_ctx_t *jctx, json_t *jso);

int json_make_array_of_ints(json_ctx_t *jctx, json_t *jso, size_t len);
json_t *json_array_of_ints_new(json_ctx_t *jctx, size_t len);

json_t *json_object_new(json_ctx_t *jctx);
int json_object_del(json_t *jso, const char *str);
int json_object_add(json_t *jso, const char *str, json_t *jval);
json_t *json_object_get(json_t *jso, const char *str);
int json_object_done(json_ctx_t *jctx, json_t *jso);

json_t *json_string_new(json_ctx_t *jctx, const char *str);
const char *json_string_get(json_t *jso);

json_t *json_int_new(json_ctx_t *jctx, int64_t val);
int64_t json_int_val(json_t *jso);

//---------------------------------------------------------------------
// printer
void json_fputs(FILE *fp, json_t *jso);

void json_set_len(json_t *jso, size_t len);
void json_set_type(json_t *jso, int type);
void json_set_pflags(json_t *jso, int pflags);

int is_json_null(json_t *jso);

#endif
