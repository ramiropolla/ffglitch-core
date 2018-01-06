/* Crappy JSON dynamic
 * Copyright (c) 2018-2019 Ramiro Polla
 * MIT License
 */

#include "json.h"

#include <stdlib.h>
#include <string.h>

//---------------------------------------------------------------------
void json_set_len(json_t *jso, size_t len)
{
    int type = JSON_TYPE(jso->flags);
    int pflags = JSON_PFLAGS(jso->flags);
    jso->flags = pflags | type | len;
}

void json_set_type(json_t *jso, int type)
{
    size_t len = JSON_LEN(jso->flags);
    int pflags = JSON_PFLAGS(jso->flags);
    jso->flags = pflags | type | len;
}

void json_set_pflags(json_t *jso, int pflags)
{
    size_t len = JSON_LEN(jso->flags);
    int type = JSON_TYPE(jso->flags);
    jso->flags = pflags | type | len;
}

//---------------------------------------------------------------------
void json_ctx_start(json_ctx_t *jctx)
{
    jctx->data.chunks = NULL;
    jctx->data.ptr = NULL;
    jctx->data.len = 0;
    jctx->data.bytes_left = 0;

    jctx->str.chunks = NULL;
    jctx->str.ptr = NULL;
    jctx->str.len = 0;
    jctx->str.bytes_left = 0;

    jctx->error = NULL;
}

static void *json_allocator_get_internal(
        json_allocator_t *jal,
        size_t chunk_size,
        size_t len)
{
    void *ptr;
    if ( jal->bytes_left < len )
    {
        size_t cur_i = jal->len++;
        jal->chunks = realloc(jal->chunks, jal->len * sizeof(char *));
        jal->chunks[cur_i] = malloc(chunk_size);
        jal->ptr = jal->chunks[cur_i];
        jal->bytes_left = chunk_size;
    }
    ptr = jal->ptr;
    jal->bytes_left -= len;
    jal->ptr += len;
    return ptr;
}

void *json_allocator_get(json_ctx_t *jctx, size_t len)
{
    return json_allocator_get_internal(&jctx->data, DATA_CHUNK, len);
}

void *json_allocator_get0(json_ctx_t *jctx, size_t len)
{
    void *ptr = json_allocator_get(jctx, len);
    memset(ptr, 0, len);
    return ptr;
}

void *json_allocator_dup(json_ctx_t *jctx, const void *src, size_t len)
{
    void *ptr = json_allocator_get(jctx, len);
    memcpy(ptr, src, len);
    return ptr;
}

void *json_allocator_strget(json_ctx_t *jctx, size_t len)
{
    return json_allocator_get_internal(&jctx->str, STR_CHUNK, len);
}

void *json_allocator_strdup(json_ctx_t *jctx, const void *src, size_t len)
{
    void *ptr = json_allocator_strget(jctx, len);
    memcpy(ptr, src, len);
    return ptr;
}

static void json_allocator_free(json_allocator_t *jal)
{
    for ( size_t i = 0; i < jal->len; i++ )
        free(jal->chunks[i]);
    free(jal->chunks);
}

void json_ctx_free(json_ctx_t *jctx)
{
    json_allocator_free(&jctx->data);
    json_allocator_free(&jctx->str);
}

json_t *alloc_json_t(json_ctx_t *jctx)
{
    return json_allocator_get(jctx, sizeof(json_t));
}

//---------------------------------------------------------------------
// Dynamic object.
// json_object_new();
// json_object_(add|del)();
// json_object_get();
// json_object_done(); // MUST be called to copy data to json_ctx_t.
// note: len contains the size of the array;
//       deleted elements have name == NULL;
//       names are strdup'd (free'd on del and done()).

json_t *json_object_new(json_ctx_t *jctx)
{
    json_t *jso = alloc_json_t(jctx);
    jso->flags = JSON_TYPE_OBJECT;
    jso->obj = json_allocator_get(jctx, sizeof(json_obj_t));
    jso->obj->names = NULL;
    jso->obj->values = NULL;
    return jso;
}

int json_object_add(json_t *jso, const char *str, json_t *jval)
{
    size_t len = JSON_LEN(jso->flags);
    size_t cur_i = len++;
    jso->obj->names = realloc(jso->obj->names, len * sizeof(char *));
    jso->obj->values = realloc(jso->obj->values, len * sizeof(json_t *));
    jso->obj->names[cur_i] = strdup(str);
    jso->obj->values[cur_i] = jval;
    json_set_len(jso, len);
    return 0;
}

int json_object_del(json_t *jso, const char *str)
{
    size_t len = JSON_LEN(jso->flags);
    for ( size_t i = 0; i < len; i++ )
    {
        char *name = jso->obj->names[i];
        if ( name != NULL && strcmp(name, str) == 0 )
        {
            free(name);
            jso->obj->names[i] = NULL;
            return 0;
        }
    }
    return -1;
}

json_t *json_object_get(json_t *jso, const char *str)
{
    size_t len = JSON_LEN(jso->flags);
    for ( size_t i = 0; i < len; i++ )
    {
        char *name = jso->obj->names[i];
        if ( name != NULL && strcmp(name, str) == 0 )
            return jso->obj->values[i];
    }
    return NULL;
}

int json_object_done(json_ctx_t *jctx, json_t *jso)
{
    size_t len = JSON_LEN(jso->flags);
    char **orig_names = jso->obj->names;
    json_t **orig_values = jso->obj->values;
    char **names = NULL;
    json_t **values = NULL;
    size_t real_len = 0;

    // Calculate real length.
    for ( size_t i = 0; i < len; i++ )
        if ( orig_names[i] != NULL )
            real_len++;

    // Populate new arrays if object is not empty.
    if ( real_len != 0 )
    {
        size_t real_i = 0;
        names = json_allocator_get(jctx, real_len * sizeof(char *));
        values = json_allocator_get(jctx, real_len * sizeof(json_t *));

        for ( size_t i = 0; i < len; i++ )
        {
            char *name = orig_names[i];
            if ( name == NULL )
                continue;
            names[real_i] = json_allocator_strdup(jctx, name, strlen(name)+1);
            free(name);
            values[real_i] = orig_values[i];
            real_i++;
        }
    }

    // Free old arrays.
    if ( orig_names != NULL )
        free(orig_names);
    if ( orig_values != NULL )
        free(orig_values);

    // Update object struct with new arrays (or NULL if empty).
    jso->obj->names = names;
    jso->obj->values = values;
    json_set_len(jso, real_len);

    return 0;
}

//---------------------------------------------------------------------
// Dynamic array.
// json_array_new();
// json_array_add();
// json_array_set();
// json_array_get();

json_t *json_array_new(json_ctx_t *jctx)
{
    json_t *jso = alloc_json_t(jctx);
    jso->flags = JSON_TYPE_ARRAY;
    jso->array = NULL;
    return jso;
}

int json_array_add(json_t *jso, json_t *jval)
{
    size_t len = JSON_LEN(jso->flags);
    size_t cur_i = len++;
    jso->array = realloc(jso->array, len * sizeof(json_t *));
    jso->array[cur_i] = jval;
    json_set_len(jso, len);
    return 0;
}

int json_array_alloc(json_ctx_t *jctx, json_t *jso, size_t len)
{
    jso->array = json_allocator_get(jctx, len * sizeof(json_t *));
    for ( size_t i = 0; i < len; i++ )
        jso->array[i] = NULL;
    json_set_len(jso, len);
    return 0;
}

int json_array_set(json_t *jso, size_t idx, json_t *jval)
{
    if ( JSON_LEN(jso->flags) <= idx )
        return -1;
    jso->array[idx] = jval;
    return 0;
}

int json_array_set_int(json_ctx_t *jctx, json_t *jso, size_t idx, int64_t val)
{
    if ( JSON_LEN(jso->flags) <= idx )
        return -1;
    if ( JSON_TYPE(jso->flags) == JSON_TYPE_ARRAY_OF_INTS )
        jso->array_of_ints[idx] = val;
    else
        jso->array[idx] = json_int_new(jctx, val);
    return 0;
}

json_t *json_array_get(json_t *jso, size_t idx)
{
    if ( JSON_LEN(jso->flags) <= idx )
        return NULL;
    return jso->array[idx];
}

int64_t json_array_get_int(json_t *jso, size_t idx)
{
    if ( JSON_LEN(jso->flags) <= idx )
        return JSON_NULL;
    if ( JSON_TYPE(jso->flags) == JSON_TYPE_ARRAY_OF_INTS )
        return jso->array_of_ints[idx];
    else
        return json_int_val(jso->array[idx]);
}

size_t json_array_length(json_t *jso)
{
    return JSON_LEN(jso->flags);
}

void json_array_sort(json_t *jso, int (* sort_fn)(const void *, const void *))
{
    qsort(jso->array, JSON_LEN(jso->flags), sizeof(json_t *), sort_fn);
}

int json_array_done(json_ctx_t *jctx, json_t *jso)
{
    json_t **orig_array = jso->array;

    if ( orig_array != NULL )
    {
        size_t len = JSON_LEN(jso->flags);
        jso->array = json_allocator_dup(jctx, orig_array, len * sizeof(json_t *));
        free(orig_array);
    }

    return 0;
}

//---------------------------------------------------------------------
// json_array_of_ints_new();

int json_make_array_of_ints(json_ctx_t *jctx, json_t *jso, size_t len)
{
    jso->flags = JSON_TYPE_ARRAY_OF_INTS | len;
    jso->array_of_ints = json_allocator_get(jctx, len * sizeof(int64_t));
    for ( size_t i = 0; i < len; i++ )
        jso->array_of_ints[i] = JSON_NULL;
    return 0;
}

json_t *json_array_of_ints_new(json_ctx_t *jctx, size_t len)
{
    json_t *jso = alloc_json_t(jctx);
    json_make_array_of_ints(jctx, jso, len);
    return jso;
}

//---------------------------------------------------------------------
json_t *json_int_new(json_ctx_t *jctx, int64_t val)
{
    json_t *jso = alloc_json_t(jctx);
    jso->flags = JSON_TYPE_NUMBER;
    jso->val = val;
    return jso;
}

int64_t json_int_val(json_t *jso)
{
    return jso->val;
}

//---------------------------------------------------------------------
json_t *json_string_new(json_ctx_t *jctx, const char *str)
{
    json_t *jso = alloc_json_t(jctx);
    jso->flags = JSON_TYPE_STRING;
    jso->str = json_allocator_strdup(jctx, str, strlen(str)+1);
    return jso;
}

const char *json_string_get(json_t *jso)
{
    return jso->str;
}

//---------------------------------------------------------------------
void json_userdata_set(json_t *jso, void *userdata)
{
    jso->userdata = userdata;
}

void *json_userdata_get(json_t *jso)
{
    return jso->userdata;
}

//---------------------------------------------------------------------
int is_json_null(json_t *jso)
{
    return JSON_TYPE(jso->flags) == JSON_TYPE_NUMBER
        && jso->val == JSON_NULL;
}
