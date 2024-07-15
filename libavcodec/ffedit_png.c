
/* This file is included by pngdec.c */

#include "ffedit.h"
#include "ffedit_json.h"
#include "libavutil/json.h"

/*-------------------------------------------------------------------*/
/* zstream (common)                                                  */
/*-------------------------------------------------------------------*/

#define IOBUF_SIZE 4096

/*-------------------------------------------------------------------*/
static int ffe_zstream_write(
        z_stream *const zstream,
        PutByteContext *pb,
        const uint8_t *buf,
        int length)
{
    uint8_t iobuf[IOBUF_SIZE];

    zstream->avail_in = length;
    zstream->next_in = buf;
    while ( zstream->avail_in > 0 )
    {
        size_t out_length;
        int ret;

        zstream->avail_out = IOBUF_SIZE;
        zstream->next_out = iobuf;
        ret = deflate(zstream, Z_NO_FLUSH);
        if ( ret != Z_OK )
            return -1;

        out_length = (IOBUF_SIZE - zstream->avail_out);
        if ( out_length != 0 )
            bytestream2_put_buffer(pb, iobuf, out_length);
    }

    return 0;
}

/*-------------------------------------------------------------------*/
static int ffe_zstream_flush(
        z_stream *const zstream,
        PutByteContext *pb,
        int flush,
        int stop_at)
{
    uint8_t iobuf[IOBUF_SIZE];
    int keep_going = 1;

    zstream->avail_in = 0;
    zstream->next_in = NULL;
    while ( keep_going )
    {
        size_t out_length;
        int ret;

        zstream->avail_out = IOBUF_SIZE;
        zstream->next_out = iobuf;
        ret = deflate(zstream, flush);
        if ( ret != Z_OK && ret != stop_at )
            return -1;

        out_length = (IOBUF_SIZE - zstream->avail_out);
        if ( out_length != 0 )
            bytestream2_put_buffer(pb, iobuf, out_length);

        keep_going = (ret == Z_OK);
    }

    return 0;
}

/*-------------------------------------------------------------------*/
static int ffe_zstream_write_buf(
        PutByteContext *pb,
        const uint8_t *buf,
        int length)
{
    int fret = -1;
    FFZStream z;
    z_stream *const zstream = &z.zstream;
    int ret = ff_deflate_init(&z, Z_DEFAULT_COMPRESSION, NULL);
    if ( ret < 0 )
        return -1;

    deflateReset(zstream);
    ret = ffe_zstream_write(zstream, pb, buf, length);
    if ( ret < 0 )
        goto the_end;
    ret = ffe_zstream_flush(zstream, pb, Z_FINISH, Z_STREAM_END);
    if ( ret < 0 )
        goto the_end;

    fret = 0;

the_end:
    ff_deflate_end(&z);
    return fret;
}

/*-------------------------------------------------------------------*/
/* FFEDIT_FEAT_HEADERS                                               */
/*-------------------------------------------------------------------*/

static const char *multiple_fields[] = {
    "iTXt",
    "tEXt",
    "zTXt",
    "sPLT",
};

/*-------------------------------------------------------------------*/

enum PNGFieldType {
    PNG_FIELD_TYPE_NONE,
    PNG_FIELD_TYPE_BYTE,
    PNG_FIELD_TYPE_BE16,
    PNG_FIELD_TYPE_BE16U,
    PNG_FIELD_TYPE_BE32,
    PNG_FIELD_TYPE_BE32U,
    PNG_FIELD_TYPE_STRING,
    PNG_FIELD_TYPE_ITEXT,
    PNG_FIELD_TYPE_TEXT,
    PNG_FIELD_TYPE_ZTEXT,
    PNG_FIELD_TYPE_ZDATA,
};

typedef struct {
    enum PNGFieldType type;
    const char *name;
} png_field_desc_t;

/*-------------------------------------------------------------------*/
static const png_field_desc_t ihdr_fields[] = {
    { PNG_FIELD_TYPE_BE32,   "width" },
    { PNG_FIELD_TYPE_BE32,   "height" },
    { PNG_FIELD_TYPE_BYTE,   "bit_depth" },
    { PNG_FIELD_TYPE_BYTE,   "color_type" },
    { PNG_FIELD_TYPE_BYTE,   "compression_type" },
    { PNG_FIELD_TYPE_BYTE,   "filter_type" },
    { PNG_FIELD_TYPE_BYTE,   "interlace_type" },
    { PNG_FIELD_TYPE_NONE }
};

/*-------------------------------------------------------------------*/
static const png_field_desc_t phys_fields[] = {
    { PNG_FIELD_TYPE_BE32,   "pixels_per_unit_x" },
    { PNG_FIELD_TYPE_BE32,   "pixels_per_unit_y" },
    { PNG_FIELD_TYPE_BYTE,   "unit_specifier" },
    { PNG_FIELD_TYPE_NONE }
};

/*-------------------------------------------------------------------*/
static const png_field_desc_t srgb_fields[] = {
    { PNG_FIELD_TYPE_BYTE,   "rendering_intent" },
    { PNG_FIELD_TYPE_NONE }
};

/*-------------------------------------------------------------------*/
static const png_field_desc_t chrm_fields[] = {
    { PNG_FIELD_TYPE_BE32,   "white_point_x" },
    { PNG_FIELD_TYPE_BE32,   "white_point_y" },
    { PNG_FIELD_TYPE_BE32,   "red_x" },
    { PNG_FIELD_TYPE_BE32,   "red_y" },
    { PNG_FIELD_TYPE_BE32,   "green_x" },
    { PNG_FIELD_TYPE_BE32,   "green_y" },
    { PNG_FIELD_TYPE_BE32,   "blue_x" },
    { PNG_FIELD_TYPE_BE32,   "blue_y" },
    { PNG_FIELD_TYPE_NONE }
};

/*-------------------------------------------------------------------*/
static const png_field_desc_t gama_fields[] = {
    { PNG_FIELD_TYPE_BE32,   "image_gamma" },
    { PNG_FIELD_TYPE_NONE }
};

/*-------------------------------------------------------------------*/
static const png_field_desc_t ster_fields[] = {
    { PNG_FIELD_TYPE_BYTE,   "mode" },
    { PNG_FIELD_TYPE_NONE }
};

/*-------------------------------------------------------------------*/
static const png_field_desc_t clli_fields[] = {
    { PNG_FIELD_TYPE_BE32U,  "max_cll" },
    { PNG_FIELD_TYPE_BE32U,  "max_fall" },
    { PNG_FIELD_TYPE_NONE }
};

/*-------------------------------------------------------------------*/
static const png_field_desc_t mdcv_fields[] = {
    { PNG_FIELD_TYPE_BE16U,  "red_x" },
    { PNG_FIELD_TYPE_BE16U,  "red_y" },
    { PNG_FIELD_TYPE_BE16U,  "green_x" },
    { PNG_FIELD_TYPE_BE16U,  "green_y" },
    { PNG_FIELD_TYPE_BE16U,  "blue_x" },
    { PNG_FIELD_TYPE_BE16U,  "blue_y" },
    { PNG_FIELD_TYPE_BE16U,  "white_point_x" },
    { PNG_FIELD_TYPE_BE16U,  "white_point_y" },
    { PNG_FIELD_TYPE_BE32U,  "max_luminance" },
    { PNG_FIELD_TYPE_BE32U,  "min_luminance" },
    { PNG_FIELD_TYPE_NONE }
};

/*-------------------------------------------------------------------*/
static const png_field_desc_t cicp_fields[] = {
    { PNG_FIELD_TYPE_BYTE,   "colour_primaries" },
    { PNG_FIELD_TYPE_BYTE,   "transfer_function" },
    { PNG_FIELD_TYPE_BYTE,   "matrix_coeffs" },
    { PNG_FIELD_TYPE_BYTE,   "video_full_range" },
    { PNG_FIELD_TYPE_NONE }
};

/*-------------------------------------------------------------------*/
static const png_field_desc_t byte_y_fields[] = {
    { PNG_FIELD_TYPE_BYTE,   "grey" },
    { PNG_FIELD_TYPE_NONE }
};

/*-------------------------------------------------------------------*/
static const png_field_desc_t byte_rgb_fields[] = {
    { PNG_FIELD_TYPE_BYTE,   "red" },
    { PNG_FIELD_TYPE_BYTE,   "green" },
    { PNG_FIELD_TYPE_BYTE,   "blue" },
    { PNG_FIELD_TYPE_NONE }
};

/*-------------------------------------------------------------------*/
static const png_field_desc_t byte_ya_fields[] = {
    { PNG_FIELD_TYPE_BYTE,   "grey" },
    { PNG_FIELD_TYPE_BYTE,   "alpha" },
    { PNG_FIELD_TYPE_NONE }
};

/*-------------------------------------------------------------------*/
static const png_field_desc_t byte_rgba_fields[] = {
    { PNG_FIELD_TYPE_BYTE,   "red" },
    { PNG_FIELD_TYPE_BYTE,   "green" },
    { PNG_FIELD_TYPE_BYTE,   "blue" },
    { PNG_FIELD_TYPE_BYTE,   "alpha" },
    { PNG_FIELD_TYPE_NONE }
};

/*-------------------------------------------------------------------*/
static const png_field_desc_t be16_y_fields[] = {
    { PNG_FIELD_TYPE_BE16,   "grey" },
    { PNG_FIELD_TYPE_NONE }
};

/*-------------------------------------------------------------------*/
static const png_field_desc_t be16_rbg_fields[] = {
    { PNG_FIELD_TYPE_BE16,   "red" },
    { PNG_FIELD_TYPE_BE16,   "blue" },
    { PNG_FIELD_TYPE_BE16,   "green" },
    { PNG_FIELD_TYPE_NONE }
};

/*-------------------------------------------------------------------*/
static const png_field_desc_t be16_rgb_fields[] = {
    { PNG_FIELD_TYPE_BE16,   "red" },
    { PNG_FIELD_TYPE_BE16,   "green" },
    { PNG_FIELD_TYPE_BE16,   "blue" },
    { PNG_FIELD_TYPE_NONE }
};

/*-------------------------------------------------------------------*/
static const png_field_desc_t time_fields[] = {
    { PNG_FIELD_TYPE_BE16,   "year" },
    { PNG_FIELD_TYPE_BYTE,   "month" },
    { PNG_FIELD_TYPE_BYTE,   "day" },
    { PNG_FIELD_TYPE_BYTE,   "hour" },
    { PNG_FIELD_TYPE_BYTE,   "minute" },
    { PNG_FIELD_TYPE_BYTE,   "second" },
    { PNG_FIELD_TYPE_NONE }
};

/*-------------------------------------------------------------------*/
static const png_field_desc_t itxt_fields[] = {
    { PNG_FIELD_TYPE_STRING, "keyword" },
    { PNG_FIELD_TYPE_BYTE,   "flag" },
    { PNG_FIELD_TYPE_BYTE,   "method" },
    { PNG_FIELD_TYPE_STRING, "language" },
    { PNG_FIELD_TYPE_STRING, "translated" },
    { PNG_FIELD_TYPE_ITEXT,  "text" },
    { PNG_FIELD_TYPE_NONE }
};

/*-------------------------------------------------------------------*/
static const png_field_desc_t text_fields[] = {
    { PNG_FIELD_TYPE_STRING, "keyword" },
    { PNG_FIELD_TYPE_TEXT,   "text" },
    { PNG_FIELD_TYPE_NONE }
};

/*-------------------------------------------------------------------*/
static const png_field_desc_t ztxt_fields[] = {
    { PNG_FIELD_TYPE_STRING, "keyword" },
    { PNG_FIELD_TYPE_BYTE,   "method" },
    { PNG_FIELD_TYPE_ZTEXT,  "text" },
    { PNG_FIELD_TYPE_NONE }
};

/*-------------------------------------------------------------------*/
static const png_field_desc_t iccp_fields[] = {
    { PNG_FIELD_TYPE_STRING, "name" },
    { PNG_FIELD_TYPE_BYTE,   "method" },
    { PNG_FIELD_TYPE_ZDATA,  "profile" },
    { PNG_FIELD_TYPE_NONE }
};

/*-------------------------------------------------------------------*/
static const png_field_desc_t fctl_fields[] = {
    { PNG_FIELD_TYPE_BE32,   "sequence_number" },
    { PNG_FIELD_TYPE_BE32,   "width" },
    { PNG_FIELD_TYPE_BE32,   "height" },
    { PNG_FIELD_TYPE_BE32,   "x_offset" },
    { PNG_FIELD_TYPE_BE32,   "y_offset" },
    { PNG_FIELD_TYPE_BE16,   "delay_num" },
    { PNG_FIELD_TYPE_BE16,   "delay_den" },
    { PNG_FIELD_TYPE_BYTE,   "dispose_op" },
    { PNG_FIELD_TYPE_BYTE,   "blend_op" },
    { PNG_FIELD_TYPE_NONE }
};

/*-------------------------------------------------------------------*/
static const png_field_desc_t actl_fields[] = {
    { PNG_FIELD_TYPE_BE32,   "num_frames" },
    { PNG_FIELD_TYPE_BE32,   "num_plays" },
    { PNG_FIELD_TYPE_NONE }
};

/*-------------------------------------------------------------------*/
static uint32_t
ffe_import_fields(
        json_t *jchunk,
        uint8_t **pbuf,
        unsigned int *pbuf_size,
        uint32_t length,
        const png_field_desc_t *field)
{
    uint8_t *buf = av_fast_realloc(*pbuf, pbuf_size, length);
    PutByteContext pb;
    bytestream2_init_writer(&pb, buf, length);
    while ( field->type != PNG_FIELD_TYPE_NONE )
    {
        json_t *jval = json_object_get(jchunk, field->name);
        switch ( field->type )
        {
        case PNG_FIELD_TYPE_BYTE:
            bytestream2_put_byte(&pb, json_int_val(jval));
            break;
        case PNG_FIELD_TYPE_BE16:
            bytestream2_put_be16(&pb, json_int_val(jval));
            break;
        case PNG_FIELD_TYPE_BE16U:
            bytestream2_put_be16u(&pb, json_int_val(jval));
            break;
        case PNG_FIELD_TYPE_BE32:
            bytestream2_put_be32(&pb, json_int_val(jval));
            break;
        case PNG_FIELD_TYPE_BE32U:
            bytestream2_put_be32u(&pb, json_int_val(jval));
            break;
        case PNG_FIELD_TYPE_STRING:
            bytestream2_put_buffer(&pb, json_string_get(jval), json_string_length(jval));
            bytestream2_put_byte(&pb, 0);
            break;
        case PNG_FIELD_TYPE_ITEXT:
            if ( json_int_val(json_object_get(jchunk, "flag")) != 0 )
                goto _ztext;
            /* fallthrough */
        case PNG_FIELD_TYPE_TEXT:
            bytestream2_put_buffer(&pb, json_string_get(jval), json_string_length(jval));
            break;
        case PNG_FIELD_TYPE_ZTEXT:
_ztext:
            ffe_zstream_write_buf(&pb, json_string_get(jval), json_string_length(jval));
            break;
        case PNG_FIELD_TYPE_ZDATA:
            {
                size_t array_length = json_array_length(jval);
                uint8_t *data = av_malloc(array_length);
                for ( size_t i = 0; i < array_length; i++ )
                    data[i] = jval->array_of_ints[i];
                ffe_zstream_write_buf(&pb, data, array_length);
                av_free(data);
            }
            break;
        }
        field++;
    }
    *pbuf = buf;
    return length;
}

/*-------------------------------------------------------------------*/
static json_t *
ffe_bytestream_get_string(json_ctx_t *jctx, GetByteContext *gb)
{
    const uint8_t *ptr = gb->buffer;
    while ( bytestream2_get_bytes_left(gb) > 0 )
        if ( bytestream2_get_byte(gb) == 0 )
            return json_string_len_new(jctx, ptr, (gb->buffer - ptr - 1));
    return NULL;
}

/*-------------------------------------------------------------------*/
static int decode_zbuf(AVBPrint *bp, const uint8_t *data,
                       const uint8_t *data_end, void *logctx);

/*-------------------------------------------------------------------*/
static json_t *
ffe_export_fields(
        json_ctx_t *jctx,
        GetByteContext *gb,
        const png_field_desc_t *field)
{
    json_t *jchunk = json_object_new(jctx);
    while ( field->type != PNG_FIELD_TYPE_NONE )
    {
        json_t *jval = NULL;
        switch ( field->type )
        {
        case PNG_FIELD_TYPE_BYTE:
            jval = json_int_new(jctx, bytestream2_get_byte(gb));
            break;
        case PNG_FIELD_TYPE_BE16:
            jval = json_int_new(jctx, bytestream2_get_be16(gb));
            break;
        case PNG_FIELD_TYPE_BE16U:
            jval = json_int_new(jctx, bytestream2_get_be16u(gb));
            break;
        case PNG_FIELD_TYPE_BE32:
            jval = json_int_new(jctx, bytestream2_get_be32(gb));
            break;
        case PNG_FIELD_TYPE_BE32U:
            jval = json_int_new(jctx, bytestream2_get_be32u(gb));
            break;
        case PNG_FIELD_TYPE_STRING:
            jval = ffe_bytestream_get_string(jctx, gb);
            break;
        case PNG_FIELD_TYPE_ITEXT:
            if ( json_int_val(json_object_get(jchunk, "flag")) != 0 )
                goto _ztext;
            /* fallthrough */
        case PNG_FIELD_TYPE_TEXT:
            jval = json_string_len_new(jctx, gb->buffer, bytestream2_get_bytes_left(gb));
            break;
        case PNG_FIELD_TYPE_ZTEXT:
_ztext:
            {
                AVBPrint bp;
                if ( decode_zbuf(&bp, gb->buffer, gb->buffer_end, NULL) >= 0 )
                {
                    jval = json_string_len_new(jctx, bp.str, bp.len);
                    av_bprint_finalize(&bp, NULL);
                }
            }
            break;
        case PNG_FIELD_TYPE_ZDATA:
            {
                AVBPrint bp;
                if ( decode_zbuf(&bp, gb->buffer, gb->buffer_end, NULL) >= 0 )
                {
                    size_t array_length = bp.len;
                    uint8_t *data = bp.str;
                    jval = json_array_of_ints_new(jctx, array_length);
                    for ( size_t i = 0; i < array_length; i++ )
                        jval->array_of_ints[i] = data[i];
                    json_set_pflags(jval, JSON_PFLAGS_SPLIT8);
                    av_bprint_finalize(&bp, NULL);
                }
            }
            break;
        }
        /* unknown field type or inflate failed */
        if ( jval == NULL )
            jval = json_null_new(jctx);
        json_object_add(jchunk, field->name, jval);
        field++;
    }
    json_object_done(jctx, jchunk);
    return jchunk;
}

/*-------------------------------------------------------------------*/
static void
ffe_decode_fields(
        PNGDecContext *s,
        AVFrame *f,
        GetByteContext *gb_chunk,
        uint32_t tag,
        uint32_t (*import_func)(json_t *, uint8_t **, unsigned int *, uint32_t, const png_field_desc_t *),
        json_t *(*export_func)(json_ctx_t *, GetByteContext *, const png_field_desc_t *),
        const png_field_desc_t *fields)
{
    if ( f == NULL )
        return;
    if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_HEADERS)) != 0 )
    {
        json_t *jheaders = f->ffedit_sd[FFEDIT_FEAT_HEADERS];
        json_t *jchunk = json_object_get(jheaders, av_fourcc2str(tag));
        // TODO fix import but not apply
        PutByteContext *pb = gb_chunk->pb;
        uint32_t length = bytestream2_get_bytes_left(gb_chunk);
        length = import_func(jchunk, &s->tmp_buf, &s->tmp_buf_size, length, fields);
        // update gb_chunk with temporary buffer
        bytestream2_init(gb_chunk, s->tmp_buf, length);
        gb_chunk->pb = pb;
    }
    else if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_HEADERS)) != 0 )
    {
        json_t *jheaders = f->ffedit_sd[FFEDIT_FEAT_HEADERS];
        json_t *jchunk;
        GetByteContext gb = *gb_chunk;
        gb.pb = NULL;
        jchunk = export_func(f->jctx, &gb, fields);
        json_object_add(jheaders, av_fourcc2str(tag), jchunk);
    }
}

/*-------------------------------------------------------------------*/
static void
ffe_decode_png_chunk(
        PNGDecContext *s,
        AVFrame *f,
        GetByteContext *gb_chunk,
        uint32_t tag,
        const png_field_desc_t *fields)
{
    return ffe_decode_fields(s, f, gb_chunk, tag, ffe_import_fields, ffe_export_fields, fields);
}

/*-------------------------------------------------------------------*/
static void
ffe_decode_fields_multiple(
        PNGDecContext *s,
        AVFrame *f,
        GetByteContext *gb_chunk,
        uint32_t tag,
        uint32_t (*import_func)(json_t *, uint8_t **, unsigned int *, uint32_t, const png_field_desc_t *),
        json_t *(*export_func)(json_ctx_t *, GetByteContext *, const png_field_desc_t *),
        const png_field_desc_t *fields)
{
    if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_HEADERS)) != 0 )
    {
        json_t *jheaders = f->ffedit_sd[FFEDIT_FEAT_HEADERS];
        json_t *jchunk = json_object_get(jheaders, av_fourcc2str(tag));
        uintptr_t index = (uintptr_t) json_array_userdata_get(jchunk);
        json_t *jentry = json_array_get(jchunk, index++);
        // TODO fix import but not apply
        PutByteContext *pb = gb_chunk->pb;
        uint32_t length = bytestream2_get_bytes_left(gb_chunk);
        length = import_func(jentry, &s->tmp_buf, &s->tmp_buf_size, length, fields);
        // update gb_chunk with temporary buffer
        bytestream2_init(gb_chunk, s->tmp_buf, length);
        gb_chunk->pb = pb;
        json_array_userdata_set(jchunk, (void *) index);
    }
    else if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_HEADERS)) != 0 )
    {
        json_t *jheaders = f->ffedit_sd[FFEDIT_FEAT_HEADERS];
        json_t *jchunk = json_object_get(jheaders, av_fourcc2str(tag));
        json_t *jentry;
        GetByteContext gb = *gb_chunk;
        gb.pb = NULL;
        jentry = export_func(f->jctx, &gb, fields);
        json_dynamic_array_add(jchunk, jentry);
    }
}

/*-------------------------------------------------------------------*/
static void
ffe_decode_png_chunk_multiple(
        PNGDecContext *s,
        AVFrame *f,
        GetByteContext *gb_chunk,
        uint32_t tag,
        const png_field_desc_t *fields)
{
    return ffe_decode_fields_multiple(s, f, gb_chunk, tag, ffe_import_fields, ffe_export_fields, fields);
}

/*-------------------------------------------------------------------*/
static uint32_t
ffe_import_plte(
        json_t *jchunk,
        uint8_t **pbuf,
        unsigned int *pbuf_size,
        uint32_t length,
        const png_field_desc_t *field)
{
    size_t n = json_array_length(jchunk);
    uint8_t *buf;
    PutByteContext pb;
    length = n * 3;
    buf = av_fast_realloc(*pbuf, pbuf_size, length);
    bytestream2_init_writer(&pb, buf, length);
    for ( size_t i = 0; i < n; i++ )
    {
        json_t *jentry = json_array_get(jchunk, i);
        bytestream2_put_byte(&pb, jentry->array_of_ints[0]);
        bytestream2_put_byte(&pb, jentry->array_of_ints[1]);
        bytestream2_put_byte(&pb, jentry->array_of_ints[2]);
    }
    *pbuf = buf;
    return length;
}

/*-------------------------------------------------------------------*/
static json_t *
ffe_export_plte(
        json_ctx_t *jctx,
        GetByteContext *gb,
        const png_field_desc_t *field)
{
    unsigned int n = bytestream2_get_bytes_left(gb) / 3;
    json_t *jchunk = json_array_new_uninit(jctx, n);
    for ( size_t i = 0; i < n; i++ )
    {
        json_t *jentry = json_array_of_ints_new(jctx, 3);
        jentry->array_of_ints[0] = bytestream2_get_byte(gb);
        jentry->array_of_ints[1] = bytestream2_get_byte(gb);
        jentry->array_of_ints[2] = bytestream2_get_byte(gb);
        json_set_pflags(jentry, JSON_PFLAGS_NO_LF);
        json_array_set(jchunk, i, jentry);
    }
    return jchunk;
}

/*-------------------------------------------------------------------*/
static void
ffe_decode_plte_chunk(
        PNGDecContext *s,
        AVFrame *f,
        GetByteContext *gb_chunk,
        uint32_t tag)
{
    return ffe_decode_fields(s, f, gb_chunk, tag, ffe_import_plte, ffe_export_plte, NULL);
}

/*-------------------------------------------------------------------*/
static uint32_t
ffe_import_splt(
        json_t *jchunk,
        uint8_t **pbuf,
        unsigned int *pbuf_size,
        uint32_t length,
        const png_field_desc_t *field)
{
    json_t *jname = json_object_get(jchunk, "name");
    json_t *jsample_depth = json_object_get(jchunk, "sample_depth");
    json_t *jentries = json_object_get(jchunk, "entries");
    size_t name_length = json_string_length(jname);
    uint8_t sample_depth = json_int_val(jsample_depth);
    size_t entry_size = (sample_depth == 8) ? 6 : 10;
    size_t n = json_array_length(jentries);
    uint8_t *buf;
    PutByteContext pb;
    length = name_length + 1 + 1 + n * entry_size;
    buf = av_fast_realloc(*pbuf, pbuf_size, length);
    bytestream2_init_writer(&pb, buf, length);
    bytestream2_put_buffer(&pb, json_string_get(jname), name_length);
    bytestream2_put_byte(&pb, 0);
    bytestream2_put_byte(&pb, sample_depth);
    for ( size_t i = 0; i < n; i++ )
    {
        json_t *jentry = json_array_get(jentries, i);
        if ( sample_depth == 8 )
        {
            bytestream2_put_byte(&pb, jentry->array_of_ints[0]);
            bytestream2_put_byte(&pb, jentry->array_of_ints[1]);
            bytestream2_put_byte(&pb, jentry->array_of_ints[2]);
            bytestream2_put_byte(&pb, jentry->array_of_ints[3]);
        }
        else
        {
            bytestream2_put_be16u(&pb, jentry->array_of_ints[0]);
            bytestream2_put_be16u(&pb, jentry->array_of_ints[1]);
            bytestream2_put_be16u(&pb, jentry->array_of_ints[2]);
            bytestream2_put_be16u(&pb, jentry->array_of_ints[3]);
        }
        bytestream2_put_be16u(&pb, jentry->array_of_ints[4]);
    }
    *pbuf = buf;
    return length;
}

/*-------------------------------------------------------------------*/
static json_t *
ffe_export_splt(
        json_ctx_t *jctx,
        GetByteContext *gb,
        const png_field_desc_t *field)
{
    json_t *jchunk = json_object_new(jctx);
    json_t *jname = ffe_bytestream_get_string(jctx, gb);
    uint8_t sample_depth = bytestream2_get_byte(gb);
    json_t *jsample_depth = json_int_new(jctx, sample_depth);
    size_t entry_size = (sample_depth == 8) ? 6 : 10;
    unsigned int n = bytestream2_get_bytes_left(gb) / entry_size;
    json_t *jentries = json_array_new_uninit(jctx, n);
    for ( size_t i = 0; i < n; i++ )
    {
        json_t *jentry = json_array_of_ints_new(jctx, 5);
        if ( sample_depth == 8 )
        {
            jentry->array_of_ints[0] = bytestream2_get_byte(gb);
            jentry->array_of_ints[1] = bytestream2_get_byte(gb);
            jentry->array_of_ints[2] = bytestream2_get_byte(gb);
            jentry->array_of_ints[3] = bytestream2_get_byte(gb);
        }
        else
        {
            jentry->array_of_ints[0] = bytestream2_get_be16u(gb);
            jentry->array_of_ints[1] = bytestream2_get_be16u(gb);
            jentry->array_of_ints[2] = bytestream2_get_be16u(gb);
            jentry->array_of_ints[3] = bytestream2_get_be16u(gb);
        }
        jentry->array_of_ints[4] = bytestream2_get_be16u(gb);
        json_set_pflags(jentry, JSON_PFLAGS_NO_LF);
        json_array_set(jentries, i, jentry);
    }
    json_object_add(jchunk, "name", jname);
    json_object_add(jchunk, "sample_depth", jsample_depth);
    json_object_add(jchunk, "entries", jentries);
    return jchunk;
}

/*-------------------------------------------------------------------*/
static void
ffe_decode_splt_chunk(
        PNGDecContext *s,
        AVFrame *f,
        GetByteContext *gb_chunk,
        uint32_t tag)
{
    return ffe_decode_fields_multiple(s, f, gb_chunk, tag, ffe_import_splt, ffe_export_splt, NULL);
}

/*-------------------------------------------------------------------*/
static uint32_t
ffe_import_vararray_8(
        json_t *jchunk,
        uint8_t **pbuf,
        unsigned int *pbuf_size,
        uint32_t length,
        const png_field_desc_t *field)
{
    size_t n = json_array_length(jchunk);
    uint8_t *buf = av_fast_realloc(*pbuf, pbuf_size, n);
    PutByteContext pb;
    bytestream2_init_writer(&pb, buf, n);
    for ( size_t i = 0; i < n; i++ )
        bytestream2_put_byte(&pb, jchunk->array_of_ints[i]);
    *pbuf = buf;
    return n;
}

/*-------------------------------------------------------------------*/
static json_t *
ffe_export_vararray_8(
        json_ctx_t *jctx,
        GetByteContext *gb,
        const png_field_desc_t *field)
{
    unsigned int n = bytestream2_get_bytes_left(gb);
    json_t *jchunk = json_array_of_ints_new(jctx, n);
    json_set_pflags(jchunk, JSON_PFLAGS_SPLIT8);
    for ( size_t i = 0; i < n; i++ )
        jchunk->array_of_ints[i] = bytestream2_get_byte(gb);
    return jchunk;
}

/*-------------------------------------------------------------------*/
static void
ffe_decode_vararray_8(
        PNGDecContext *s,
        AVFrame *f,
        GetByteContext *gb_chunk,
        uint32_t tag)
{
    return ffe_decode_fields(s, f, gb_chunk, tag, ffe_import_vararray_8, ffe_export_vararray_8, NULL);
}

/*-------------------------------------------------------------------*/
static uint32_t
ffe_import_vararray_16(
        json_t *jchunk,
        uint8_t **pbuf,
        unsigned int *pbuf_size,
        uint32_t length,
        const png_field_desc_t *field)
{
    size_t n = json_array_length(jchunk);
    uint8_t *buf = av_fast_realloc(*pbuf, pbuf_size, n * 2);
    PutByteContext pb;
    bytestream2_init_writer(&pb, buf, n);
    for ( size_t i = 0; i < n; i++ )
        bytestream2_put_be16u(&pb, jchunk->array_of_ints[i]);
    *pbuf = buf;
    return n * 2;
}

/*-------------------------------------------------------------------*/
static json_t *
ffe_export_vararray_16(
        json_ctx_t *jctx,
        GetByteContext *gb,
        const png_field_desc_t *field)
{
    unsigned int n = bytestream2_get_bytes_left(gb) / 2;
    json_t *jchunk = json_array_of_ints_new(jctx, n);
    json_set_pflags(jchunk, JSON_PFLAGS_SPLIT8);
    for ( size_t i = 0; i < n; i++ )
        jchunk->array_of_ints[i] = bytestream2_get_be16u(gb);
    return jchunk;
}

/*-------------------------------------------------------------------*/
static void
ffe_decode_vararray_16(
        PNGDecContext *s,
        AVFrame *f,
        GetByteContext *gb_chunk,
        uint32_t tag)
{
    return ffe_decode_fields(s, f, gb_chunk, tag, ffe_import_vararray_16, ffe_export_vararray_16, NULL);
}

/*-------------------------------------------------------------------*/
static void
ffe_decode_sbit_chunk(
        PNGDecContext *s,
        AVFrame *f,
        GetByteContext *gb_chunk,
        uint32_t tag)
{
    const png_field_desc_t *fields = NULL;
    switch ( s->color_type )
    {
    case PNG_COLOR_TYPE_GRAY:       fields = byte_y_fields;    break;
    case PNG_COLOR_TYPE_RGB:        fields = byte_rgb_fields;  break;
    case PNG_COLOR_TYPE_PALETTE:    fields = byte_rgb_fields;  break;
    case PNG_COLOR_TYPE_GRAY_ALPHA: fields = byte_ya_fields;   break;
    case PNG_COLOR_TYPE_RGB_ALPHA:  fields = byte_rgba_fields; break;
    }
    if ( fields != NULL )
        ffe_decode_png_chunk(s, f, gb_chunk, tag, fields);
}

/*-------------------------------------------------------------------*/
static void
ffe_decode_bkgd_chunk(
        PNGDecContext *s,
        AVFrame *f,
        GetByteContext *gb_chunk,
        uint32_t tag)
{
    const png_field_desc_t *fields = NULL;
    int palette = 0;
    switch ( s->color_type )
    {
    case PNG_COLOR_TYPE_GRAY:       fields = be16_y_fields;   break;
    case PNG_COLOR_TYPE_RGB:        fields = be16_rgb_fields; break;
    case PNG_COLOR_TYPE_PALETTE:    palette = 1;              break;
    case PNG_COLOR_TYPE_GRAY_ALPHA: fields = be16_y_fields;   break;
    case PNG_COLOR_TYPE_RGB_ALPHA:  fields = be16_rgb_fields; break;
    }
    if ( fields != NULL )
        ffe_decode_png_chunk(s, f, gb_chunk, tag, fields);
    else if ( palette != 0 )
        ffe_decode_vararray_8(s, f, gb_chunk, tag);
}

/*-------------------------------------------------------------------*/
static void
ffe_decode_trns_chunk(
        PNGDecContext *s,
        AVFrame *f,
        GetByteContext *gb_chunk,
        uint32_t tag)
{
    const png_field_desc_t *fields = NULL;
    int palette = 0;
    switch ( s->color_type )
    {
    case PNG_COLOR_TYPE_GRAY:       fields = be16_y_fields;   break;
    case PNG_COLOR_TYPE_RGB:        fields = be16_rbg_fields; break;
    case PNG_COLOR_TYPE_PALETTE:    palette = 1;              break;
    }
    if ( fields != NULL )
        ffe_decode_png_chunk(s, f, gb_chunk, tag, fields);
    else if ( palette != 0 )
        ffe_decode_vararray_8(s, f, gb_chunk, tag);
}

/*-------------------------------------------------------------------*/
static void
ffe_png_headers_export_init(PNGDecContext *s, AVFrame *f)
{
    json_t *jheaders = json_object_new(f->jctx);
    for ( size_t i = 0; i < FF_ARRAY_ELEMS(multiple_fields); i++ )
    {
        const char *name = multiple_fields[i];
        json_t *jchunk = json_dynamic_array_new(f->jctx);
        json_object_add(jheaders, name, jchunk);
    }
    f->ffedit_sd[FFEDIT_FEAT_HEADERS] = jheaders;
}

/*-------------------------------------------------------------------*/
static void
ffe_png_headers_import_init(PNGDecContext *s, AVFrame *f)
{
    json_t *jheaders = f->ffedit_sd[FFEDIT_FEAT_HEADERS];
    for ( size_t i = 0; i < FF_ARRAY_ELEMS(multiple_fields); i++ )
    {
        const char *name = multiple_fields[i];
        json_t *jchunk = json_object_get(jheaders, name);
        if ( jchunk != NULL )
            json_array_userdata_set(jchunk, 0);
    }
}

/*-------------------------------------------------------------------*/
static void
ffe_png_headers_export_cleanup(PNGDecContext *s, AVFrame *f)
{
    json_t *jheaders = f->ffedit_sd[FFEDIT_FEAT_HEADERS];
    for ( size_t i = 0; i < FF_ARRAY_ELEMS(multiple_fields); i++ )
    {
        const char *name = multiple_fields[i];
        json_t *jchunk = json_object_get(jheaders, name);
        if ( json_array_length(jchunk) == 0 )
            json_object_del(jheaders, name);
        else
            json_dynamic_array_done(f->jctx, jchunk);
    }
    json_object_done(f->jctx, jheaders);
}

/*-------------------------------------------------------------------*/
/* FFEDIT_FEAT_IDAT                                                  */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
static size_t interlaced_y(size_t y, int pass)
{
    static const uint8_t pass_ymin[]   = { 0, 0, 4, 0, 2, 0, 1, };
    static const uint8_t pass_yshift[] = { 3, 3, 3, 2, 2, 1, 1, };
    uint8_t ymin = pass_ymin[pass];
    uint8_t yshift = pass_yshift[pass];
    return ((y - ymin - 1) + (1 << yshift)) >> yshift;
}

/*-------------------------------------------------------------------*/
static void
ffe_png_idat_export_init(PNGDecContext *s, AVFrame *f)
{
    json_t *jframe = json_object_new(f->jctx);
    json_t *jcompression_level = json_int_new(f->jctx, FF_COMPRESSION_DEFAULT);

    if ( s->interlace_type == 1 )
    {
        json_t *jpasses = json_array_new(f->jctx, NB_PASSES);

        json_object_add(jframe, "passes", jpasses);

        for ( size_t p = 0; p < NB_PASSES; p++ )
        {
            size_t num_rows = interlaced_y(s->height, p);
            json_t *jrows = json_array_new(f->jctx, num_rows);
            int crow_size = 1 + ff_png_pass_row_size(p, s->bits_per_pixel, s->cur_w);
            json_array_set(jpasses, p, jrows);
            for ( size_t i = 0; i < num_rows; i++ )
            {
                json_t *jrow = json_array_of_ints_new(f->jctx, crow_size);
                json_set_pflags(jrow, JSON_PFLAGS_NO_LF);
                json_array_set(jrows, i, jrow);
            }
        }

        json_object_userdata_set(jframe, jpasses);
    }
    else
    {
        json_t *jrows = json_array_new(f->jctx, s->height);

        json_object_add(jframe, "rows", jrows);

        for ( size_t i = 0; i < s->height; i++ )
        {
            json_t *jrow = json_array_of_ints_new(f->jctx, s->crow_size);
            json_set_pflags(jrow, JSON_PFLAGS_NO_LF);
            json_array_set(jrows, i, jrow);
        }

        json_object_userdata_set(jframe, jrows);
    }

    json_object_add(jframe, "compression_level", jcompression_level);
    f->ffedit_sd[FFEDIT_FEAT_IDAT] = jframe;
}

/*-------------------------------------------------------------------*/
static void
ffe_png_idat_import_init(PNGDecContext *s, AVFrame *f)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_IDAT];
    json_t *jcompression_level = json_object_get(jframe, "compression_level");
    int compression_level = json_int_val(jcompression_level);
    json_t *jdata;
    if ( s->interlace_type == 1 )
        jdata = json_object_get(jframe, "passes");
    else
        jdata = json_object_get(jframe, "rows");
    json_object_userdata_set(jframe, jdata);
    compression_level = compression_level == FF_COMPRESSION_DEFAULT
                      ? Z_DEFAULT_COMPRESSION
                      : av_clip(compression_level, 0, 9);
    ff_deflate_init(&s->zstream_enc, compression_level, s->avctx);
}

/*-------------------------------------------------------------------*/
static void
ffe_png_idat_export(
        PNGDecContext *s,
        AVFrame *f,
        size_t offset,
        size_t length)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_IDAT];
    json_t *jdata = json_object_userdata_get(jframe);
    json_t *jrow;
    uint8_t *crow_buf = s->crow_buf;
    if ( s->interlace_type == 1 )
    {
        json_t *jpass = json_array_get(jdata, s->pass);
        jrow = json_array_get(jpass, interlaced_y(s->y, s->pass));
    }
    else
    {
        jrow = json_array_get(jdata, s->y);
    }
    for ( size_t i = 0; i < length; i++ )
        jrow->array_of_ints[offset + i] = crow_buf[offset + i];
}

/*-------------------------------------------------------------------*/
static void
ffe_png_idat_import(
        PNGDecContext *s,
        AVFrame *f,
        size_t offset,
        size_t length)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_IDAT];
    json_t *jdata = json_object_userdata_get(jframe);
    json_t *jrow;
    uint8_t *crow_buf = s->crow_buf;
    if ( s->interlace_type == 1 )
    {
        json_t *jpass = json_array_get(jdata, s->pass);
        jrow = json_array_get(jpass, interlaced_y(s->y, s->pass));
    }
    else
    {
        jrow = json_array_get(jdata, s->y);
    }
    for ( size_t i = 0; i < length; i++ )
        crow_buf[offset + i] = av_clip_uint8(jrow->array_of_ints[offset + i]);
}

/*-------------------------------------------------------------------*/
static void
ffe_png_idat_chunk(
        PNGDecContext *s,
        GetByteContext *gb,
        AVFrame *f,
        z_stream *const zstream,
        z_stream *const orig_zstream)
{
    size_t out_offset = (s->crow_size - orig_zstream->avail_out);
    size_t out_length = ((s->crow_size - zstream->avail_out) - out_offset);
    if ( (s->avctx->ffedit_import & (1 << FFEDIT_FEAT_IDAT)) != 0 )
        ffe_png_idat_import(s, f, out_offset, out_length);
    if ( gb->pb != NULL )
    {
        if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_IDAT)) != 0 )
            ffe_zstream_write(&s->zstream_enc.zstream, gb->pb, s->crow_buf + out_offset, out_length);
        else
            bytestream2_put_buffer(gb->pb, orig_zstream->next_in, (orig_zstream->avail_in - zstream->avail_in));
    }
    if ( (s->avctx->ffedit_export & (1 << FFEDIT_FEAT_IDAT)) != 0 )
        ffe_png_idat_export(s, f, out_offset, out_length);
}

/*-------------------------------------------------------------------*/
static void
ffe_png_idat_chunk_flush(PNGDecContext *s, GetByteContext *gb, int ret)
{
    if ( (s->avctx->ffedit_apply & (1 << FFEDIT_FEAT_IDAT)) != 0 )
    {
        int flush = (ret == Z_STREAM_END) ? Z_FINISH : Z_SYNC_FLUSH;
        int stop_at = (ret == Z_STREAM_END) ? Z_STREAM_END : Z_BUF_ERROR;
        ffe_zstream_flush(&s->zstream_enc.zstream, gb->pb, flush, stop_at);
    }
}

/*-------------------------------------------------------------------*/
static void
ffe_png_idat_init(PNGDecContext *s, AVFrame *f)
{
    AVCodecContext *avctx = s->avctx;

    /* FFEDIT_FEAT_IDAT */
    if ( (avctx->ffedit_export & (1 << FFEDIT_FEAT_IDAT)) != 0 )
        ffe_png_idat_export_init(s, f);
    else if ( (avctx->ffedit_import & (1 << FFEDIT_FEAT_IDAT)) != 0 )
        ffe_png_idat_import_init(s, f);
}

/*-------------------------------------------------------------------*/
static void
ffe_png_idat_export_cleanup(PNGDecContext *s, AVFrame *f)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_IDAT];
    json_object_done(f->jctx, jframe);
}

/*-------------------------------------------------------------------*/
static void
ffe_png_fdat(PNGDecContext *s, AVFrame *f, GetByteContext *gb)
{
    AVCodecContext *avctx = s->avctx;

    /* FFEDIT_FEAT_IDAT */
    if ( (avctx->ffedit_export & (1 << FFEDIT_FEAT_IDAT)) != 0 )
    {
        json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_IDAT];
        uint32_t sequence_number = AV_RB32(gb->buffer);
        json_t *jsequence_number = json_int_new(f->jctx, sequence_number);
        json_object_add(jframe, "sequence_number", jsequence_number);
    }
    else if ( (avctx->ffedit_import & (1 << FFEDIT_FEAT_IDAT)) != 0 )
    {
        json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_IDAT];
        json_t *jsequence_number = json_object_get(jframe, "sequence_number");
        uint32_t sequence_number = json_int_val(jsequence_number);
        AV_WB32(gb->buffer, sequence_number);
    }
}

/*-------------------------------------------------------------------*/
/* FFEDIT_FEAT_LAST                                                  */
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
static int
ffe_png_transplicate_init(
        PNGDecContext *s,
        AVPacket *avpkt,
        FFEditTransplicateBytesContext *xp)
{
    AVCodecContext *avctx = s->avctx;
    if ( (avctx->ffedit_apply & (1 << FFEDIT_FEAT_LAST)) != 0 )
    {
        int ret = ffe_transplicate_bytes_init(avctx, xp, avpkt->size);
        if ( ret < 0 )
            return ret;
        s->gb.pb = ffe_transplicate_bytes_pb(xp);
    }
    return 0;
}

/*-------------------------------------------------------------------*/
static void
ffe_png_transplicate_cleanup(
        PNGDecContext *s,
        AVPacket *avpkt,
        FFEditTransplicateBytesContext *xp)
{
    AVCodecContext *avctx = s->avctx;
    if ( (avctx->ffedit_apply & (1 << FFEDIT_FEAT_LAST)) != 0 )
        ffe_transplicate_bytes_flush(avctx, xp, avpkt);
}

/*-------------------------------------------------------------------*/
static int
ffe_png_prepare_frame(PNGDecContext *s, AVFrame *f, AVPacket *avpkt)
{
    AVCodecContext *avctx = s->avctx;

    /* jctx */
    if ( avctx->ffedit_export != 0 )
        f->jctx = json_ctx_new(1);
    else if ( avctx->ffedit_import != 0 )
        f->jctx = avpkt->jctx;

    /* ffedit_sd */
    if ( avctx->ffedit_import != 0 )
        memcpy(f->ffedit_sd, avpkt->ffedit_sd, sizeof(json_t *)*FFEDIT_FEAT_LAST);

    /* FFEDIT_FEAT_HEADERS */
    if ( (avctx->ffedit_export & (1 << FFEDIT_FEAT_HEADERS)) != 0 )
        ffe_png_headers_export_init(s, f);
    else if ( (avctx->ffedit_import & (1 << FFEDIT_FEAT_HEADERS)) != 0 )
        ffe_png_headers_import_init(s, f);

    return 0;
}

/*-------------------------------------------------------------------*/
static void
ffe_png_cleanup_frame(PNGDecContext *s, AVFrame *f)
{
    AVCodecContext *avctx = s->avctx;

    /* FFEDIT_FEAT_HEADERS */
    if ( (avctx->ffedit_export & (1 << FFEDIT_FEAT_HEADERS)) != 0 )
        ffe_png_headers_export_cleanup(s, f);

    /* FFEDIT_FEAT_IDAT */
    if ( (avctx->ffedit_export & (1 << FFEDIT_FEAT_IDAT)) != 0 )
        ffe_png_idat_export_cleanup(s, f);
}
