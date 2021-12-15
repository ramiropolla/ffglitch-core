#include "libavutil/json.h"

#include "ffedit.h"
#include "ffedit_json.h"
#include "ffedit_mb.h"
#include "ffedit_xp.h"
#include "internal.h"

/*-------------------------------------------------------------------*/
void ffe_mb_export_init(
        json_ctx_t *jctx,
        AVFrame *f,
        int mb_height,
        int mb_width)
{
    // {
    //  "sizes": [ ] # line
    //           [ ] # column
    //           val # MUST NOT CHANGE!!!
    //  "data":  [ ] # line
    //           [ ] # column
    //           hex sequence for macroblock
    //  ]
    // }

    json_t *jframe = json_object_new(jctx);
    ffe_mb_ctx *ctx = json_allocator_get0(jctx, sizeof(ffe_mb_ctx));
    json_userdata_set(jframe, ctx);

    ctx->jdatas = ffe_jblock_new(jctx, mb_width, mb_height, JSON_PFLAGS_NO_LF);
    ctx->jsizes = ffe_jblock_new(jctx, mb_width, mb_height, JSON_PFLAGS_NO_LF);
    json_object_add(jframe, "data", ctx->jdatas);
    json_object_add(jframe, "sizes", ctx->jsizes);

    f->ffedit_sd[FFEDIT_FEAT_MB] = jframe;
}

/*-------------------------------------------------------------------*/
void ffe_mb_export_cleanup(json_ctx_t *jctx, AVFrame *f)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_MB];
    json_object_done(jctx, jframe);
}

/*-------------------------------------------------------------------*/
void ffe_mb_import_init(json_ctx_t *jctx, AVFrame *f)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_MB];
    ffe_mb_ctx *ctx = json_allocator_get0(jctx, sizeof(ffe_mb_ctx));
    json_userdata_set(jframe, ctx);
    ctx->jsizes = json_object_get(jframe, "sizes");
    ctx->jdatas = json_object_get(jframe, "data");
}

/*-------------------------------------------------------------------*/
void ffe_mb_export_init_mb(ffe_mb_mb_ctx *mbctx, GetBitContext *gb)
{
    int size = (get_bits_left(gb) + 7) >> 3; // in bytes

    mbctx->data = av_malloc(size);

    init_put_bits(&mbctx->pb, mbctx->data, size);

    gb->pb = &mbctx->pb;
}

/*-------------------------------------------------------------------*/
static char
hexchar(uint8_t nibble)
{
    if ( nibble <= 9 )
        return '0' + nibble;
    return 'A' + nibble - 10;
}

void ffe_mb_export_flush_mb(
        ffe_mb_mb_ctx *mbctx,
        json_ctx_t *jctx,
        AVFrame *f,
        GetBitContext *gb,
        int mb_y,
        int mb_x)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_MB];
    ffe_mb_ctx *ctx = json_userdata_get(jframe);
    json_t *jsize_y = json_array_get(ctx->jsizes, mb_y);
    json_t *jdata_y = json_array_get(ctx->jdatas, mb_y);
    int size = put_bits_count(&mbctx->pb); // in bits
    int i;

    json_t *jsize = json_int_new(jctx, size);
    json_t *jdata;
    char *str;

    size = (size + 7) >> 3; // in bytes
    str = av_malloc(size*2 + 1);
    str[size*2] = '\0';

    flush_put_bits(&mbctx->pb);
    for ( i = 0; i < size; i++ )
    {
        str[i*2+0] = hexchar(mbctx->pb.buf[i] >> 4);
        str[i*2+1] = hexchar(mbctx->pb.buf[i] & 0x0F);
    }

    jdata = json_string_new(jctx, str);
    free(str);

    json_array_set(jsize_y, mb_x, jsize);
    json_array_set(jdata_y, mb_x, jdata);

    av_free(mbctx->data);
    gb->pb = NULL;
}

/*-------------------------------------------------------------------*/
static int
hexval(char c)
{
    if ( c >= '0' && c <= '9' )
        return c - '0';
    return c - 'A' + 10;
}

void ffe_mb_import_init_mb(
        ffe_mb_mb_ctx *mbctx,
        AVFrame *f,
        GetBitContext *gb,
        FFEditTransplicateContext *xp,
        int mb_y,
        int mb_x)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_MB];
    ffe_mb_ctx *ctx = json_userdata_get(jframe);
    json_t *jdata_y = json_array_get(ctx->jdatas, mb_y);
    json_t *jdata = json_array_get(jdata_y, mb_x);
    const char *str = json_string_get(jdata);
    int size = strlen(str) / 2;
    int i;

    mbctx->data = av_mallocz(size + 4);

    for ( i = 0; i < size; i++ )
    {
        int c1 = hexval(str[i*2+0]);
        int c2 = hexval(str[i*2+1]);
        mbctx->data[i] = (c1 << 4) | c2;
    }

    mbctx->saved = *gb;
    init_get_bits(gb, mbctx->data, (size + 4) * 8);

    if ( xp != NULL )
        gb->pb = ffe_transplicate_pb(xp);
}

/*-------------------------------------------------------------------*/
void ffe_mb_import_flush_mb(
        ffe_mb_mb_ctx *mbctx,
        AVFrame *f,
        GetBitContext *gb,
        FFEditTransplicateContext *xp,
        int mb_y,
        int mb_x)
{
    json_t *jframe = f->ffedit_sd[FFEDIT_FEAT_MB];
    ffe_mb_ctx *ctx = json_userdata_get(jframe);
    json_t *jsize_y = json_array_get(ctx->jsizes, mb_y);
    int size = json_array_get_int(jsize_y, mb_x); // in bits

    av_free(mbctx->data);
    *gb = mbctx->saved;

    if ( xp != NULL )
        gb->pb = NULL;

    while ( size > 32 )
    {
        skip_bits_long(gb, 32);
        size -= 32;
    }
    if ( size > 0 )
        skip_bits_long(gb, size);

    if ( xp != NULL )
        gb->pb = ffe_transplicate_pb(xp);
}
