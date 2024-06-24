/*
 * Crappy JSON library
 *
 * Copyright (c) 2018-2023 Ramiro Polla
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef AVUTIL_JSON_H
#define AVUTIL_JSON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

typedef struct json_t json_t;

typedef struct {
    char **keys;
    json_t **values;
    void *userdata;
} json_obj_t;

#define MAX_MV2DARRAY_NBLOCKS 4
#define MV_NULL 0x80000000
typedef struct {
    int32_t *mvs[MAX_MV2DARRAY_NBLOCKS];
    uint8_t *nb_blocks_array;
    uint16_t width;
    uint16_t height;
    int max_nb_blocks;
} json_mv2darray_t;

#define JSON_NULL 0x8000000000000000ULL

struct json_t {
#define JSON_TYPE_OBJECT        0x00010000
#define JSON_TYPE_ARRAY         0x00020000
#define JSON_TYPE_ARRAY_OF_INTS 0x00040000
#define JSON_TYPE_STRING        0x00080000
#define JSON_TYPE_NUMBER        0x00100000
#define JSON_TYPE_BOOL          0x00200000
#define JSON_TYPE_MV_2DARRAY    0x00400000 /* 2d array of mvs, if mv[x][y][0] is 0x80000000, the mv is JSON_NULL */
#define JSON_PFLAGS_NO_LF       0x01000000
#define JSON_PFLAGS_NO_SPACE    0x02000000
#define JSON_PFLAGS_SPLIT8      0x04000000
#define JSON_LEN(x)    (x & 0x0000FFFF)
#define JSON_TYPE(x)   (x & 0x00FF0000)
#define JSON_PFLAGS(x) (x & 0xFF000000)
    union {
        char *str;
        int64_t val;
        json_mv2darray_t *mv2darray;
        json_t **array;
        int32_t *array_of_ints;
        json_obj_t *obj;
    };
    uint32_t flags;
};

//---------------------------------------------------------------------
// json context

#define SMALL_DATA_CHUNK (128*1024) // 128kB
#define SMALL_STR_CHUNK (4*1024) // 4kB
#ifdef _WIN32
// it seems windows is not as smart as linux/macos to manage unused
// memory, so we use smaller chunks.
#  define LARGE_DATA_CHUNK (128*1024) // 128kB
#  define LARGE_STR_CHUNK (4*1024) // 4kB
#else
#  define LARGE_DATA_CHUNK (32*1024*1024) // 32MB
#  define LARGE_STR_CHUNK (1024*1024) // 1MB
#endif

typedef struct {
    void **chunks;
    uint8_t *ptr;
    size_t len;
    size_t bytes_left;
} json_allocator_t;

typedef struct json_ctx_t json_ctx_t;

struct json_ctx_t {
    json_allocator_t data;
    json_allocator_t str;

    size_t data_chunk;
    size_t str_chunk;

    /* for multi-threading */
    json_ctx_t *next;
};

void json_ctx_start(json_ctx_t *jctx, int large);
json_ctx_t *json_ctx_start_thread(json_ctx_t *jctx, int large, int n);
void *json_allocator_get(json_ctx_t *jctx, size_t len);
void *json_allocator_get0(json_ctx_t *jctx, size_t len);
void *json_allocator_dup(json_ctx_t *jctx, const void *src, size_t len);
void *json_allocator_strget(json_ctx_t *jctx, size_t len);
void *json_allocator_strndup(json_ctx_t *jctx, const void *src, size_t len);
static inline
void *json_allocator_strdup(json_ctx_t *jctx, const void *src)
{
    return json_allocator_strndup(jctx, src, strlen(src)+1);
}
void json_ctx_free(json_ctx_t *jctx);
json_t *alloc_json_t(json_ctx_t *jctx);

typedef struct json_error_ctx_t json_error_ctx_t;

struct json_error_ctx_t {
    char *str;
    char *buf;
    char *column;

    size_t line;
    size_t offset;
};

//---------------------------------------------------------------------
// parser
json_t *json_parse(json_ctx_t *jctx, const char *buf);
void json_error_parse(json_error_ctx_t *jectx, const char *buf);
void json_error_free(json_error_ctx_t *jectx);

//---------------------------------------------------------------------
// dynamic
static inline
void json_userdata_set(json_t *jso, void *userdata)
{
    jso->obj->userdata = userdata;
}

static inline
void *json_userdata_get(json_t *jso)
{
    return jso->obj->userdata;
}

//---------------------------------------------------------------------
// array (fixed)
static inline
void json_set_len(json_t *jso, size_t len);
static inline
json_t *json_array_new_uninit(json_ctx_t *jctx, size_t len)
{
    json_t *jso = alloc_json_t(jctx);
    jso->flags = JSON_TYPE_ARRAY;
    jso->array = json_allocator_get(jctx, len * sizeof(json_t *));
    json_set_len(jso, len);
    return jso;
}
static inline
json_t *json_array_new(json_ctx_t *jctx, size_t len)
{
    json_t *jso = json_array_new_uninit(jctx, len);
    for ( size_t i = 0; i < len; i++ )
        jso->array[i] = NULL;
    return jso;
}

// array (common)
static inline
size_t json_array_length(json_t *jso)
{
    return JSON_LEN(jso->flags);
}
static inline
int json_array_set(json_t *jso, size_t idx, json_t *jval)
{
    if ( json_array_length(jso) <= idx )
        return -1;
    jso->array[idx] = jval;
    return 0;
}
static inline
json_t *json_int_new(json_ctx_t *jctx, int64_t val);
static inline
json_t *json_array_get(json_t *jso, size_t idx)
{
    return jso->array[idx];
}
static inline
int64_t json_int_val(json_t *jso);
static inline
void json_array_sort(json_t *jso, int (* sort_fn)(const void *, const void *))
{
    qsort(jso->array, json_array_length(jso), sizeof(json_t *), sort_fn);
}

// array (dynamic)
json_t *json_dynamic_array_new(json_ctx_t *jctx);
int json_dynamic_array_add(json_t *jso, json_t *jval);
int json_dynamic_array_done(json_ctx_t *jctx, json_t *jso);

static inline
int json_make_array_of_ints(json_ctx_t *jctx, json_t *jso, size_t len)
{
    jso->flags = JSON_TYPE_ARRAY_OF_INTS | len;
    jso->array_of_ints = json_allocator_get(jctx, len * sizeof(int32_t));
    return 0;
}

static inline
json_t *json_array_of_ints_new(json_ctx_t *jctx, size_t len)
{
    json_t *jso = alloc_json_t(jctx);
    json_make_array_of_ints(jctx, jso, len);
    return jso;
}

//---------------------------------------------------------------------
// mv2darray
json_t *json_mv2darray_new(
        json_ctx_t *jctx,
        int16_t width,
        int16_t height,
        int max_nb_blocks,
        int set_zero);

void json_mv2darray_done(json_ctx_t *jctx, json_t *jso);

//---------------------------------------------------------------------
// object
static inline
size_t json_object_length(json_t *jso)
{
    return JSON_LEN(jso->flags);
}
json_t *json_object_new(json_ctx_t *jctx);
int json_object_del(json_t *jso, const char *key);
int json_object_add(json_t *jso, const char *key, json_t *jval);
json_t *json_object_get(json_t *jso, const char *key);
int json_object_done(json_ctx_t *jctx, json_t *jso);

//---------------------------------------------------------------------
// string
static inline
json_t *json_string_new(json_ctx_t *jctx, const char *str)
{
    json_t *jso = alloc_json_t(jctx);
    jso->flags = JSON_TYPE_STRING;
    jso->str = json_allocator_strdup(jctx, str);
    return jso;
}

static inline
const char *json_string_get(json_t *jso)
{
    return jso->str;
}

//---------------------------------------------------------------------
// int
static inline
json_t *json_int_new(json_ctx_t *jctx, int64_t val)
{
    json_t *jso = alloc_json_t(jctx);
    jso->flags = JSON_TYPE_NUMBER;
    jso->val = val;
    return jso;
}

static inline
int64_t json_int_val(json_t *jso)
{
    return jso->val;
}

//---------------------------------------------------------------------
// null
static inline
json_t *json_null_new(json_ctx_t *jctx)
{
    return json_int_new(jctx, JSON_NULL);
}

//---------------------------------------------------------------------
// bool
static inline
json_t *json_bool_new(json_ctx_t *jctx, int val)
{
    json_t *jso = alloc_json_t(jctx);
    jso->flags = JSON_TYPE_BOOL;
    jso->val = (val != 0);
    return jso;
}

static inline
int json_bool_val(json_t *jso)
{
    return (jso->val != 0);
}

//---------------------------------------------------------------------
// printer
void json_fputs(FILE *fp, json_t *jso);

static inline
void json_set_len(json_t *jso, size_t len)
{
    int type = JSON_TYPE(jso->flags);
    int pflags = JSON_PFLAGS(jso->flags);
    jso->flags = pflags | type | len;
}

static inline
void json_set_type(json_t *jso, int type)
{
    size_t len = JSON_LEN(jso->flags);
    int pflags = JSON_PFLAGS(jso->flags);
    jso->flags = pflags | type | len;
}

static inline
void json_set_pflags(json_t *jso, int pflags)
{
    size_t len = JSON_LEN(jso->flags);
    int type = JSON_TYPE(jso->flags);
    jso->flags = pflags | type | len;
}

static inline
int is_json_null(json_t *jso)
{
    return JSON_TYPE(jso->flags) == JSON_TYPE_NUMBER
        && jso->val == JSON_NULL;
}

#endif
