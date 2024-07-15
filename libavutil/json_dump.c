/*
 * Crappy JSON dumper
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

#include <stdarg.h>

#include "json.h"

//---------------------------------------------------------------------
#define CHUNK_SIZE 4 * 1024 * 1024
typedef struct sbuf
{
    FILE *fp;
    char *data;
    size_t offset;
} sbuf;

static inline void sbuf_init(sbuf *ctx, FILE *fp)
{
    ctx->fp = fp;
    ctx->data = malloc(CHUNK_SIZE);
    ctx->offset = 0;
}

static inline void sbuf_flush(sbuf *ctx)
{
    if ( ctx->offset != 0 )
    {
        fwrite(ctx->data, ctx->offset, 1, ctx->fp);
        ctx->offset = 0;
    }
}

static inline void sbuf_fputc(sbuf *ctx, char c)
{
    if ( ctx->offset == CHUNK_SIZE )
        sbuf_flush(ctx);
    ctx->data[ctx->offset++] = c;
}

static inline void sbuf_fputs(sbuf *ctx, const char *str)
{
    while ( *str != '\0' )
        sbuf_fputc(ctx, *str++);
}

static inline void sbuf_fprintf(sbuf *ctx, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    sbuf_flush(ctx);
    vfprintf(ctx->fp, format, args);
    va_end(args);
}

static inline void sbuf_spaces(sbuf *ctx, size_t num)
{
    if ( ctx->offset + num >= CHUNK_SIZE )
        sbuf_flush(ctx);
    while ( num-- )
        ctx->data[ctx->offset++] = ' ';
}

static inline void sbuf_free(sbuf *ctx)
{
    sbuf_flush(ctx);
    free(ctx->data);
}

//---------------------------------------------------------------------
static inline void output_lf(sbuf *ctx, json_t *jso, int level)
{
    if ( (jso->flags & JSON_PFLAGS_NO_LF) == 0 )
    {
        sbuf_fputc(ctx, '\n');
        sbuf_spaces(ctx, level * 2);
    }
    else if ( (jso->flags & JSON_PFLAGS_NO_SPACE) == 0 )
    {
        sbuf_fputc(ctx, ' ');
    }
}

static inline void output_lf_array(sbuf *ctx, json_t *jso, int level, int i)
{
    if ( (jso->flags & JSON_PFLAGS_SPLIT8) != 0 )
    {
        if ( (i & 7) == 0 )
            output_lf(ctx, jso, level);
        else if ( i != 0 )
            sbuf_fputc(ctx, ' ');
    }
    else
    {
        output_lf(ctx, jso, level);
    }
}

static void output_num_32(sbuf *ctx, int32_t val)
{
    int is_neg = (val < 0);
    uint32_t abs_val = (uint32_t) (is_neg ? -val : val);
    if ( abs_val < 10 )
    {
        uint8_t u8 = (uint8_t) abs_val;
        if ( is_neg )
            sbuf_fputc(ctx, '-');
        sbuf_fputc(ctx, '0' + u8);
    }
    else if ( abs_val < 100 )
    {
        uint8_t u8 = (uint8_t) abs_val;
        if ( is_neg )
            sbuf_fputc(ctx, '-');
        sbuf_fputc(ctx, '0' + u8 / 10);
        sbuf_fputc(ctx, '0' + u8 % 10);
    }
    else
    {
        sbuf_fprintf(ctx, "%" PRId32, val);
    }
}

static void output_num_64(sbuf *ctx, int64_t val)
{
    if ( val == JSON_NULL )
    {
        sbuf_fputs(ctx, "null");
    }
    else
    {
        int is_neg = (val < 0);
        uint64_t abs_val = (uint64_t) (is_neg ? -val : val);
        if ( abs_val < 10 )
        {
            uint8_t u8 = (uint8_t) abs_val;
            if ( is_neg )
                sbuf_fputc(ctx, '-');
            sbuf_fputc(ctx, '0' + u8);
        }
        else if ( abs_val < 100 )
        {
            uint8_t u8 = (uint8_t) abs_val;
            if ( is_neg )
                sbuf_fputc(ctx, '-');
            sbuf_fputc(ctx, '0' + u8 / 10);
            sbuf_fputc(ctx, '0' + u8 % 10);
        }
        else
        {
            sbuf_fprintf(ctx, "%" PRId64, val);
        }
    }
}

static inline void output_char(sbuf *ctx, char c)
{
    switch ( c )
    {
        case '"':  sbuf_fputs(ctx, "\\\""); break;
        case '\\': sbuf_fputs(ctx, "\\\\"); break;
        case '/':  sbuf_fputs(ctx, "\\/");  break;
        case '\b': sbuf_fputs(ctx, "\\b");  break;
        case '\f': sbuf_fputs(ctx, "\\f");  break;
        case '\n': sbuf_fputs(ctx, "\\n");  break;
        case '\r': sbuf_fputs(ctx, "\\r");  break;
        case '\t': sbuf_fputs(ctx, "\\t");  break;
        // TODO unicode
        default:   sbuf_fputc(ctx, c);      break;
    }
}

static void output_string(sbuf *ctx, const char *str)
{
    sbuf_fputc(ctx, '"');
    while ( *str != '\0' )
        output_char(ctx, *str++);
    sbuf_fputc(ctx, '"');
}

static void output_string_len(sbuf *ctx, const char *str, size_t length)
{
    sbuf_fputc(ctx, '"');
    while ( length-- )
        output_char(ctx, *str++);
    sbuf_fputc(ctx, '"');
}

static void output_mv(sbuf *ctx, int32_t *mv)
{
    sbuf_fputc(ctx, '[');
    output_num_32(ctx, mv[0]);
    sbuf_fputc(ctx, ',');
    output_num_32(ctx, mv[1]);
    sbuf_fputc(ctx, ']');
}

static void output_mv_or_null(sbuf *ctx, int32_t *mv)
{
    if ( mv[0] == MV_NULL )
    {
        sbuf_fputs(ctx, "null");
        return;
    }
    output_mv(ctx, mv);
}

static void json_print_element(sbuf *ctx, json_t *jso, int level)
{
    size_t len;
    if ( jso == NULL )
    {
        sbuf_fputs(ctx, "null");
        return;
    }
    switch ( JSON_TYPE(jso->flags) )
    {
    case JSON_TYPE_OBJECT:
        sbuf_fputc(ctx, '{');
        len = json_object_length(jso);
        for ( size_t i = 0; i < len; i++ )
        {
            if ( i != 0 )
                sbuf_fputc(ctx, ',');
            output_lf(ctx, jso, level+1);
            output_string(ctx, jso->obj->kvps[i].key);
            sbuf_fputc(ctx, ':');
            json_print_element(ctx, jso->obj->kvps[i].value, level+1);
        }
        output_lf(ctx, jso, level);
        sbuf_fputc(ctx, '}');
        break;
    case JSON_TYPE_ARRAY:
        sbuf_fputc(ctx, '[');
        len = json_array_length(jso);
        for ( size_t i = 0; i < len; i++ )
        {
            if ( i != 0 )
                sbuf_fputc(ctx, ',');
            output_lf_array(ctx, jso, level+1, i);
            json_print_element(ctx, jso->array[i], level+1);
        }
        output_lf(ctx, jso, level);
        sbuf_fputc(ctx, ']');
        break;
    case JSON_TYPE_ARRAY_OF_INTS:
        sbuf_fputc(ctx, '[');
        len = json_array_length(jso);
        for ( size_t i = 0; i < len; i++ )
        {
            if ( i != 0 )
                sbuf_fputc(ctx, ',');
            output_lf_array(ctx, jso, level+1, i);
            output_num_32(ctx, jso->array_of_ints[i]);
        }
        output_lf(ctx, jso, level);
        sbuf_fputc(ctx, ']');
        break;
    case JSON_TYPE_MV_2DARRAY:
        {
            json_mv2darray_t *mv2d = jso->mv2darray;
            const size_t width = mv2d->width;
            const size_t height = mv2d->height;
            sbuf_fputc(ctx, '[');
            if ( mv2d->max_nb_blocks == 1 )
            {
                for ( size_t i = 0; i < height; i++ )
                {
                    if ( i != 0 )
                        sbuf_fputc(ctx, ',');
                    output_lf(ctx, jso, level+1);
                    sbuf_fputc(ctx, '[');
                    for ( size_t j = 0; j < width; j++ )
                    {
                        size_t idx = i * width + j;
                        int32_t *mv = &mv2d->mvs[0][idx << 1];
                        if ( j != 0 )
                            sbuf_fputc(ctx, ',');
                        sbuf_fputc(ctx, ' ');
                        output_mv_or_null(ctx, mv);
                    }
                    sbuf_fputc(ctx, ' ');
                    sbuf_fputc(ctx, ']');
                }
            }
            else
            {
                const uint8_t *nb_blocks_array = mv2d->nb_blocks_array;
                for ( size_t i = 0; i < height; i++ )
                {
                    if ( i != 0 )
                        sbuf_fputc(ctx, ',');
                    output_lf(ctx, jso, level+1);
                    sbuf_fputc(ctx, '[');
                    for ( size_t j = 0; j < width; j++ )
                    {
                        size_t idx = i * width + j;
                        uint8_t nb_blocks = nb_blocks_array[idx];
                        if ( j != 0 )
                            sbuf_fputc(ctx, ',');
                        sbuf_fputc(ctx, ' ');
                        if ( nb_blocks == 0 )
                        {
                            sbuf_fputs(ctx, "null");
                        }
                        else if ( nb_blocks == 1 )
                        {
                            int32_t *mv = &mv2d->mvs[0][idx << 1];
                            output_mv(ctx, mv);
                        }
                        else
                        {
                            sbuf_fputc(ctx, '[');
                            for ( size_t k = 0; k < nb_blocks; k++ )
                            {
                                int32_t *mv = &mv2d->mvs[k][idx << 1];
                                if ( k != 0 )
                                    sbuf_fputc(ctx, ',');
                                output_mv(ctx, mv);
                            }
                            sbuf_fputc(ctx, ']');
                        }
                    }
                    sbuf_fputc(ctx, ' ');
                    sbuf_fputc(ctx, ']');
                }
            }
            output_lf(ctx, jso, level);
            sbuf_fputc(ctx, ']');
        }
        break;
    case JSON_TYPE_STRING:
        output_string_len(ctx, json_string_get(jso), json_string_length(jso));
        break;
    case JSON_TYPE_NUMBER:
        output_num_64(ctx, jso->val);
        break;
    case JSON_TYPE_BOOL:
        sbuf_fputs(ctx, (jso->val == 0) ? "false" : "true");
        break;
    }
}

void json_fputs(FILE *fp, json_t *jso)
{
    sbuf ctx;
    sbuf_init(&ctx, fp);
    json_print_element(&ctx, jso, 0);
    sbuf_fputc(&ctx, '\n');
    sbuf_free(&ctx);
}
