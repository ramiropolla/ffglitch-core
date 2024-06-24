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

#include "json.h"

//---------------------------------------------------------------------
void json_ctx_start(json_ctx_t *jctx, int large)
{
    jctx->data.chunks = NULL;
    jctx->data.ptr = NULL;
    jctx->data.len = 0;
    jctx->data.bytes_left = 0;

    jctx->str.chunks = NULL;
    jctx->str.ptr = NULL;
    jctx->str.len = 0;
    jctx->str.bytes_left = 0;

    jctx->next = NULL;

    jctx->data_chunk = large ? LARGE_DATA_CHUNK : SMALL_DATA_CHUNK;
    jctx->str_chunk = large ? LARGE_STR_CHUNK : SMALL_STR_CHUNK;
}

//---------------------------------------------------------------------
json_ctx_t *json_ctx_start_thread(json_ctx_t *jctx, int large, int n)
{
    json_ctx_t *prev_jctx = jctx;
    json_ctx_t *cur_jctx = jctx;

    // walk up to current jctx ptr
    for ( int i = 0; i < n; i++ )
    {
        prev_jctx = cur_jctx;
        cur_jctx = cur_jctx->next;
    }

    // create it if needed
    if ( cur_jctx == NULL )
    {
        cur_jctx = json_allocator_get0(prev_jctx, sizeof(json_ctx_t));
        json_ctx_start(cur_jctx, large);
        prev_jctx->next = cur_jctx;
    }

    return cur_jctx;
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
        if ( chunk_size < len )
            chunk_size = len;
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
    return json_allocator_get_internal(&jctx->data, jctx->data_chunk, len);
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
    return json_allocator_get_internal(&jctx->str, jctx->str_chunk, len);
}

void *json_allocator_strndup(json_ctx_t *jctx, const void *src, size_t len)
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
    if ( jctx == NULL )
        return;
    while ( jctx->next != NULL )
    {
        json_ctx_t *prev_jctx = jctx;
        json_ctx_t *cur_jctx = jctx->next;
        while ( cur_jctx->next != NULL )
        {
            prev_jctx = cur_jctx;
            cur_jctx = cur_jctx->next;
        }
        json_ctx_free(cur_jctx);
        prev_jctx->next = NULL;
    }
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
//       keys are strdup'd (free'd on del and done()).

json_t *json_object_new(json_ctx_t *jctx)
{
    json_t *jso = alloc_json_t(jctx);
    jso->flags = JSON_TYPE_OBJECT;
    jso->obj = json_allocator_get(jctx, sizeof(json_obj_t));
    jso->obj->keys = NULL;
    jso->obj->values = NULL;
    return jso;
}

int json_object_add(json_t *jso, const char *key, json_t *jval)
{
    size_t len = json_object_length(jso);
    size_t cur_i = len++;
    jso->obj->keys = realloc(jso->obj->keys, len * sizeof(char *));
    jso->obj->values = realloc(jso->obj->values, len * sizeof(json_t *));
    jso->obj->keys[cur_i] = strdup(key);
    jso->obj->values[cur_i] = jval;
    json_set_len(jso, len);
    return 0;
}

int json_object_del(json_t *jso, const char *key)
{
    size_t len = json_object_length(jso);
    for ( size_t i = 0; i < len; i++ )
    {
        char *cur_key = jso->obj->keys[i];
        if ( cur_key != NULL && strcmp(cur_key, key) == 0 )
        {
            free(cur_key);
            jso->obj->keys[i] = NULL;
            return 0;
        }
    }
    return -1;
}

json_t *json_object_get(json_t *jso, const char *key)
{
    size_t len = json_object_length(jso);
    for ( size_t i = 0; i < len; i++ )
    {
        char *cur_key = jso->obj->keys[i];
        if ( cur_key != NULL && strcmp(cur_key, key) == 0 )
            return jso->obj->values[i];
    }
    return NULL;
}

int json_object_done(json_ctx_t *jctx, json_t *jso)
{
    size_t len = json_object_length(jso);
    char **orig_keys = jso->obj->keys;
    json_t **orig_values = jso->obj->values;
    char **keys = NULL;
    json_t **values = NULL;
    size_t real_len = 0;

    // Calculate real length.
    for ( size_t i = 0; i < len; i++ )
        if ( orig_keys[i] != NULL )
            real_len++;

    // Populate new arrays if object is not empty.
    if ( real_len != 0 )
    {
        size_t real_i = 0;
        keys = json_allocator_get(jctx, real_len * sizeof(char *));
        values = json_allocator_get(jctx, real_len * sizeof(json_t *));

        for ( size_t i = 0; i < len; i++ )
        {
            char *cur_key = orig_keys[i];
            if ( cur_key == NULL )
                continue;
            keys[real_i] = json_allocator_strdup(jctx, cur_key);
            free(cur_key);
            values[real_i] = orig_values[i];
            real_i++;
        }
    }

    // Free old arrays.
    if ( orig_keys != NULL )
        free(orig_keys);
    if ( orig_values != NULL )
        free(orig_values);

    // Update object struct with new arrays (or NULL if empty).
    jso->obj->keys = keys;
    jso->obj->values = values;
    json_set_len(jso, real_len);

    return 0;
}

//---------------------------------------------------------------------
// array (dynamic)
json_t *json_dynamic_array_new(json_ctx_t *jctx)
{
    json_t *jso = alloc_json_t(jctx);
    jso->flags = JSON_TYPE_ARRAY;
    jso->array = NULL;
    return jso;
}

int json_dynamic_array_add(json_t *jso, json_t *jval)
{
    size_t len = json_array_length(jso);
    size_t cur_i = len++;
    jso->array = realloc(jso->array, len * sizeof(json_t *));
    jso->array[cur_i] = jval;
    json_set_len(jso, len);
    return 0;
}

int json_dynamic_array_done(json_ctx_t *jctx, json_t *jso)
{
    json_t **orig_array = jso->array;
    if ( orig_array != NULL )
    {
        size_t len = json_array_length(jso);
        jso->array = json_allocator_dup(jctx, orig_array, len * sizeof(json_t *));
        free(orig_array);
    }
    return 0;
}

//---------------------------------------------------------------------
// mv2darray
json_t *json_mv2darray_new(
        json_ctx_t *jctx,
        int16_t width,
        int16_t height,
        int max_nb_blocks,
        int set_zero)
{
    json_t *jso = alloc_json_t(jctx);
    json_mv2darray_t *jmv2d = json_allocator_get0(jctx, sizeof(json_mv2darray_t));
    for ( size_t i = 0; i < max_nb_blocks; i++ )
    {
        int32_t *mvs = json_allocator_get(jctx, (sizeof(int32_t) * width * height) << 1);
        if ( set_zero == 1 )
        {
            memset(mvs, 0, (sizeof(int32_t) * width * height) << 1);
        }
        else if ( set_zero == -1 )
        {
            for ( size_t j = 0; j < (width * height * 2); j++ )
                mvs[j] = MV_NULL;
        }
        jmv2d->mvs[i] = mvs;
    }
    jmv2d->nb_blocks_array = json_allocator_get0(jctx, width * height);
    jmv2d->width = width;
    jmv2d->height = height;
    jmv2d->max_nb_blocks = max_nb_blocks;
    jso->mv2darray = jmv2d;
    jso->flags = JSON_TYPE_MV_2DARRAY;
    return jso;
}

void json_mv2darray_done(json_ctx_t *jctx, json_t *jso)
{
    json_mv2darray_t *jmv2d = jso->mv2darray;
    const uint8_t *nb_blocks_array = jmv2d->nb_blocks_array;
    const size_t width = jmv2d->width;
    const size_t height = jmv2d->height;
    int max_nb_blocks = 0;
    for ( size_t i = 0; i < height; i++ )
        for ( size_t j = 0; j < width; j++ )
            if ( max_nb_blocks < nb_blocks_array[i * width + j] )
                max_nb_blocks = nb_blocks_array[i * width + j];
    jmv2d->max_nb_blocks = max_nb_blocks;
}
