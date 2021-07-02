/* Crappy JSON dumper
 * Copyright (c) 2018-2021 Ramiro Polla
 * MIT License
 */

#include <stdlib.h>
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
static void output_lf(sbuf *ctx, json_t *jso, int level)
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

static void output_num(sbuf *ctx, int64_t val)
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

static void output_string(sbuf *ctx, const char *str)
{
    sbuf_fputc(ctx, '"');
    sbuf_fputs(ctx, str);
    sbuf_fputc(ctx, '"');
}

static void json_print_element(sbuf *ctx, json_t *jso, int level)
{
    if ( jso == NULL )
    {
        sbuf_fputs(ctx, "null");
        return;
    }
    switch ( JSON_TYPE(jso->flags) )
    {
    case JSON_TYPE_OBJECT:
        sbuf_fputc(ctx, '{');
        for ( size_t i = 0; i < JSON_LEN(jso->flags); i++ )
        {
            if ( i != 0 )
                sbuf_fputc(ctx, ',');
            output_lf(ctx, jso, level+1);
            output_string(ctx, jso->obj->names[i]);
            sbuf_fputc(ctx, ':');
            json_print_element(ctx, jso->obj->values[i], level+1);
        }
        output_lf(ctx, jso, level);
        sbuf_fputc(ctx, '}');
        break;
    case JSON_TYPE_ARRAY:
        sbuf_fputc(ctx, '[');
        for ( size_t i = 0; i < JSON_LEN(jso->flags); i++ )
        {
            if ( i != 0 )
                sbuf_fputc(ctx, ',');
            output_lf(ctx, jso, level+1);
            json_print_element(ctx, jso->array[i], level+1);
        }
        output_lf(ctx, jso, level);
        sbuf_fputc(ctx, ']');
        break;
    case JSON_TYPE_ARRAY_OF_INTS:
        sbuf_fputc(ctx, '[');
        for ( size_t i = 0; i < JSON_LEN(jso->flags); i++ )
        {
            if ( i != 0 )
                sbuf_fputc(ctx, ',');
            output_lf(ctx, jso, level+1);
            output_num(ctx, jso->array_of_ints[i]);
        }
        output_lf(ctx, jso, level);
        sbuf_fputc(ctx, ']');
        break;
    case JSON_TYPE_STRING:
        output_string(ctx, jso->str);
        break;
    case JSON_TYPE_NUMBER:
        output_num(ctx, jso->val);
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
