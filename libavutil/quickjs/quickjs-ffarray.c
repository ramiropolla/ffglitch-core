/*********************************************************************/
static int js_parse_range(JSContext *ctx,
                          int32_t *pidx, int32_t *pend, int32_t *plength,
                          int argc, JSValueConst *argv)
{
    if ( (argc > 0) && JS_ToInt32Clamp(ctx, pidx, argv[0], 0, *pend, *pend) < 0)
        return -1;
    if ( (argc > 1) && JS_ToInt32Clamp(ctx, pend, argv[1], 0, *pend, *pend) < 0)
        return -1;
    *plength = *pend - *pidx;
    return 0;
}

/*********************************************************************/
/* FFArray ***********************************************************/
/*********************************************************************/

static void js_ffarray_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    js_free_rt(rt, p->u.array.u.ptr);
}

static inline JSValue JS_InitFFArray(JSContext *ctx, JSValue val, void **pptr, uint32_t len, int set_zero, int class_id)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    int size_log2 = ffarray_or_ffptr_size_log2(class_id);
    void *new_array_prop = NULL;

    /* allocate array values */
    if ( len != 0 )
    {
        new_array_prop = js_malloc_rt(ctx->rt, len << size_log2);
        if ( unlikely(!new_array_prop) )
        {
            JS_FreeValue(ctx, val);
            return JS_EXCEPTION;
        }
        if ( set_zero == 1 )
            memset(new_array_prop, 0, len << size_log2);
        else if ( set_zero == -1 )
            memset(new_array_prop, 0xFF, len << size_log2);
    }

    p->prop[0].u.value = JS_NewInt32(ctx, len);
    p->u.array.u.values = new_array_prop;
    p->u.array.u1.typed_array = NULL; /* clear all of u1 */
    p->u.array.count = len;

    *pptr = new_array_prop;

    return val;
}

static JSValue js_ffarray_constructor(JSContext *ctx,
                                      JSValueConst new_target,
                                      int argc, JSValueConst *argv,
                                      int magic)
{
    int class_id = JS_CLASS_INT8_FFARRAY + magic;
    void *ptr;
    uint64_t length;
    JSValue val;
    if ( JS_ToIndex(ctx, &length, argv[0]) )
        return JS_ThrowTypeError(ctx, "FFArray() takes a positive length as argument");
    if ( likely(JS_IsUndefined(new_target)) )
        val = JS_NewObjectFromShape(ctx, js_dup_shape(ctx->ffarray_shapes[magic]), class_id);
    else
        val = js_create_from_ctor(ctx, new_target, class_id);
    if ( unlikely(JS_IsException(val)) )
        return val;
    return JS_InitFFArray(ctx, val, &ptr, length, 1, class_id);
}

static JSValue JS_NewFFArray(JSContext *ctx, void **pptr, uint32_t len, int set_zero, int class_id)
{
    int magic = class_id - JS_CLASS_INT8_FFARRAY;
    JSValue val = JS_NewObjectFromShape(ctx, js_dup_shape(ctx->ffarray_shapes[magic]), class_id);
    if ( unlikely(JS_IsException(val)) )
        return val;
    return JS_InitFFArray(ctx, val, pptr, len, set_zero, class_id);
}

JSValue JS_NewInt8FFArray  (JSContext *ctx, int8_t   **pint8,   uint32_t len, int set_zero) { return JS_NewFFArray(ctx, (void **) pint8,   len, set_zero, JS_CLASS_INT8_FFARRAY); }
JSValue JS_NewUint8FFArray (JSContext *ctx, uint8_t  **puint8,  uint32_t len, int set_zero) { return JS_NewFFArray(ctx, (void **) puint8,  len, set_zero, JS_CLASS_UINT8_FFARRAY); }
JSValue JS_NewInt16FFArray (JSContext *ctx, int16_t  **pint16,  uint32_t len, int set_zero) { return JS_NewFFArray(ctx, (void **) pint16,  len, set_zero, JS_CLASS_INT16_FFARRAY); }
JSValue JS_NewUint16FFArray(JSContext *ctx, uint16_t **puint16, uint32_t len, int set_zero) { return JS_NewFFArray(ctx, (void **) puint16, len, set_zero, JS_CLASS_UINT16_FFARRAY); }
JSValue JS_NewInt32FFArray (JSContext *ctx, int32_t  **pint32,  uint32_t len, int set_zero) { return JS_NewFFArray(ctx, (void **) pint32,  len, set_zero, JS_CLASS_INT32_FFARRAY); }
JSValue JS_NewUint32FFArray(JSContext *ctx, uint32_t **puint32, uint32_t len, int set_zero) { return JS_NewFFArray(ctx, (void **) puint32, len, set_zero, JS_CLASS_UINT32_FFARRAY); }
JSValue JS_NewInt64FFArray (JSContext *ctx, int64_t  **pint64,  uint32_t len, int set_zero) { return JS_NewFFArray(ctx, (void **) pint64,  len, set_zero, JS_CLASS_INT64_FFARRAY); }
JSValue JS_NewUint64FFArray(JSContext *ctx, uint64_t **puint64, uint32_t len, int set_zero) { return JS_NewFFArray(ctx, (void **) puint64, len, set_zero, JS_CLASS_UINT64_FFARRAY); }

int JS_IsInt8FFArray  (JSValueConst val) { return JS_IsObjectClass(val, JS_CLASS_INT8_FFARRAY)   != NULL; }
int JS_IsUint8FFArray (JSValueConst val) { return JS_IsObjectClass(val, JS_CLASS_UINT8_FFARRAY)  != NULL; }
int JS_IsInt16FFArray (JSValueConst val) { return JS_IsObjectClass(val, JS_CLASS_INT16_FFARRAY)  != NULL; }
int JS_IsUint16FFArray(JSValueConst val) { return JS_IsObjectClass(val, JS_CLASS_UINT16_FFARRAY) != NULL; }
int JS_IsInt32FFArray (JSValueConst val) { return JS_IsObjectClass(val, JS_CLASS_INT32_FFARRAY)  != NULL; }
int JS_IsUint32FFArray(JSValueConst val) { return JS_IsObjectClass(val, JS_CLASS_UINT32_FFARRAY) != NULL; }
int JS_IsInt64FFArray (JSValueConst val) { return JS_IsObjectClass(val, JS_CLASS_INT64_FFARRAY)  != NULL; }
int JS_IsUint64FFArray(JSValueConst val) { return JS_IsObjectClass(val, JS_CLASS_UINT64_FFARRAY) != NULL; }

static force_inline
int JS_GetFFArray(JSValueConst val, void **pptr, uint32_t *plen, int class_id)
{
    JSObject *p = JS_IsObjectClass(val, class_id);
    if ( p == NULL )
        return FALSE;
    *pptr = p->u.array.u.ptr;
    *plen = p->u.array.count;
    return TRUE;
}

int JS_GetInt8FFArray  (JSValueConst val, int8_t   **pint8,   uint32_t *plen) { return JS_GetFFArray(val, (void **) pint8,   plen, JS_CLASS_INT8_FFARRAY  ); }
int JS_GetUint8FFArray (JSValueConst val, uint8_t  **puint8,  uint32_t *plen) { return JS_GetFFArray(val, (void **) puint8,  plen, JS_CLASS_UINT8_FFARRAY ); }
int JS_GetInt16FFArray (JSValueConst val, int16_t  **pint16,  uint32_t *plen) { return JS_GetFFArray(val, (void **) pint16,  plen, JS_CLASS_INT16_FFARRAY ); }
int JS_GetUint16FFArray(JSValueConst val, uint16_t **puint16, uint32_t *plen) { return JS_GetFFArray(val, (void **) puint16, plen, JS_CLASS_UINT16_FFARRAY); }
int JS_GetInt32FFArray (JSValueConst val, int32_t  **pint32,  uint32_t *plen) { return JS_GetFFArray(val, (void **) pint32,  plen, JS_CLASS_INT32_FFARRAY ); }
int JS_GetUint32FFArray(JSValueConst val, uint32_t **puint32, uint32_t *plen) { return JS_GetFFArray(val, (void **) puint32, plen, JS_CLASS_UINT32_FFARRAY); }
int JS_GetInt64FFArray (JSValueConst val, int64_t  **pint64,  uint32_t *plen) { return JS_GetFFArray(val, (void **) pint64,  plen, JS_CLASS_INT64_FFARRAY ); }
int JS_GetUint64FFArray(JSValueConst val, uint64_t **puint64, uint32_t *plen) { return JS_GetFFArray(val, (void **) puint64, plen, JS_CLASS_UINT64_FFARRAY); }

static force_inline
void ffarray_join_internal(StringBuffer *b, JSObject *p, int c, JSString *str_p)
{
    uint32_t len = p->u.array.count;
    if ( p->class_id == JS_CLASS_MV || p->class_id == JS_CLASS_MVREF )
        len = 1;
    /* string_buffer_putc8(b, '['); */
    for ( size_t i = 0; i < len; i++ )
    {
        char buf[32];
        if ( i != 0 )
        {
            if ( c < 0 )
                string_buffer_concat(b, str_p, 0, str_p->len);
            else
                string_buffer_putc8(b, c);
        }
        switch ( p->class_id )
        {
        case JS_CLASS_INT8_FFARRAY:
        case JS_CLASS_INT8_FFPTR:
            output_int32(buf, p->u.array.u.int8_ptr[i])[0] = '\0';
            break;
        case JS_CLASS_UINT8_FFARRAY:
        case JS_CLASS_UINT8_FFPTR:
            output_uint32(buf, p->u.array.u.uint8_ptr[i])[0] = '\0';
            break;
        case JS_CLASS_INT16_FFARRAY:
        case JS_CLASS_INT16_FFPTR:
            output_int32(buf, p->u.array.u.int16_ptr[i])[0] = '\0';
            break;
        case JS_CLASS_UINT16_FFARRAY:
        case JS_CLASS_UINT16_FFPTR:
            output_uint32(buf, p->u.array.u.uint16_ptr[i])[0] = '\0';
            break;
        case JS_CLASS_INT32_FFARRAY:
        case JS_CLASS_INT32_FFPTR:
            output_int32(buf, p->u.array.u.int32_ptr[i])[0] = '\0';
            break;
        case JS_CLASS_UINT32_FFARRAY:
        case JS_CLASS_UINT32_FFPTR:
            output_uint32(buf, p->u.array.u.uint32_ptr[i])[0] = '\0';
            break;
        case JS_CLASS_INT64_FFARRAY:
        case JS_CLASS_INT64_FFPTR:
            sprintf(buf, "%" PRId64, p->u.array.u.int64_ptr[i]);
            break;
        case JS_CLASS_UINT64_FFARRAY:
        case JS_CLASS_UINT64_FFPTR:
            sprintf(buf, "%" PRIu64, p->u.array.u.uint64_ptr[i]);
            break;
        case JS_CLASS_MV:
        case JS_CLASS_MVREF:
        case JS_CLASS_MVARRAY:
        case JS_CLASS_MVPTR:
            print_mv(buf, &p->u.array.u.int32_ptr[i << 1]);
            break;
        case JS_CLASS_MVMASK:
            if ( p->u.array.u.int64_ptr[i] == 0 )
            {
                buf[0] = 'f';
                buf[1] = 'a';
                buf[2] = 'l';
                buf[3] = 's';
                buf[4] = 'e';
                buf[5] = '\0';
            }
            else
            {
                buf[0] = 't';
                buf[1] = 'r';
                buf[2] = 'u';
                buf[3] = 'e';
                buf[4] = '\0';
            }
            break;
        }
        string_buffer_puts8(b, buf);
    }
    /* string_buffer_putc8(b, ']'); */
}

static JSValue js_ffarray_toString(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    StringBuffer b_s, *b = &b_s;

    string_buffer_init(ctx, b, 0);

    ffarray_join_internal(b, p, ',', NULL);

    return string_buffer_end(b);
}

static JSValue js_ffarray_join(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    JSValue sep = JS_UNDEFINED;
    StringBuffer b_s, *b = &b_s;
    JSString *str_p = NULL;
    int c = ',';

    if ( argc > 0 )
    {
        sep = JS_ToString(ctx, argv[0]);
        if ( JS_IsException(sep) )
            return sep;
        str_p = JS_VALUE_GET_STRING(sep);
        if ( str_p->len == 1 && !str_p->is_wide_char )
            c = str_p->u.str8[0];
        else
            c = -1;
    }

    string_buffer_init(ctx, b, 0);

    ffarray_join_internal(b, p, c, str_p);

    JS_FreeValue(ctx, sep);

    return string_buffer_end(b);
}

static JSValue js_ffarray_copyWithin(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    int64_t len = p->u.array.count;
    int64_t to;
    int64_t from = 0;
    int64_t final = len;
    int64_t count;
    uint8_t *ptr = p->u.array.u.uint8_ptr;
    int size_log2 = ffarray_or_ffptr_size_log2(p->class_id);

    if ( (JS_ToInt64Clamp(ctx, &to, argv[0], 0, len, len) < 0)
      || ((argc > 1) && JS_ToInt64Clamp(ctx, &from, argv[1], 0, len, len) < 0)
      || ((argc > 2) && JS_ToInt64Clamp(ctx, &final, argv[2], 0, len, len) < 0) )
    {
        return JS_ThrowRangeError(ctx, "error parsing range arguments");
    }

    count = min_int64(final - from, len - to);

    memmove(ptr + (to << size_log2), ptr + (from << size_log2), (count << size_log2));

    return JS_DupValue(ctx, this_val);
}

static JSValue js_ffarray_subarray(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    int32_t idx = 0;
    int32_t end = p->u.array.count;
    int32_t length;

    /* optional range arguments */
    if ( js_parse_range(ctx, &idx, &end, &length, argc, argv) < 0 )
        return JS_ThrowRangeError(ctx, "error parsing range arguments");

    if ( idx < 0 || length <= 0 || end > p->u.array.count )
        return JS_ThrowRangeError(ctx, "out-of-bound access");

    switch ( p->class_id )
    {
    case JS_CLASS_INT8_FFARRAY:
    case JS_CLASS_INT8_FFPTR:
        return JS_NewInt8FFPtr(ctx, p->u.array.u.int8_ptr + idx, length);
    case JS_CLASS_UINT8_FFARRAY:
    case JS_CLASS_UINT8_FFPTR:
        return JS_NewUint8FFPtr(ctx, p->u.array.u.uint8_ptr + idx, length);
    case JS_CLASS_INT16_FFARRAY:
    case JS_CLASS_INT16_FFPTR:
        return JS_NewInt16FFPtr(ctx, p->u.array.u.int16_ptr + idx, length);
    case JS_CLASS_UINT16_FFARRAY:
    case JS_CLASS_UINT16_FFPTR:
        return JS_NewUint16FFPtr(ctx, p->u.array.u.uint16_ptr + idx, length);
    case JS_CLASS_INT32_FFARRAY:
    case JS_CLASS_INT32_FFPTR:
        return JS_NewInt32FFPtr(ctx, p->u.array.u.int32_ptr + idx, length);
    case JS_CLASS_UINT32_FFARRAY:
    case JS_CLASS_UINT32_FFPTR:
        return JS_NewUint32FFPtr(ctx, p->u.array.u.uint32_ptr + idx, length);
    case JS_CLASS_INT64_FFARRAY:
    case JS_CLASS_INT64_FFPTR:
        return JS_NewInt64FFPtr(ctx, p->u.array.u.int64_ptr + idx, length);
    case JS_CLASS_UINT64_FFARRAY:
    case JS_CLASS_UINT64_FFPTR:
        return JS_NewUint64FFPtr(ctx, p->u.array.u.uint64_ptr + idx, length);
    case JS_CLASS_MVARRAY:
    case JS_CLASS_MVPTR:
        return JS_NewMVPtr(ctx, p->u.array.u.int32_ptr + (idx << 1), length);
    case JS_CLASS_MVMASK:
        return JS_NewMVMaskPtr(ctx, p->u.array.u.int64_ptr + (idx << 1), length);
    }

    return JS_UNDEFINED;
}

static int normalized_class(int class_id)
{
    if ( class_id >= JS_CLASS_INT8_FFARRAY && class_id <= JS_CLASS_UINT64_FFARRAY )
        return class_id - JS_CLASS_INT8_FFARRAY;
    if ( class_id >= JS_CLASS_INT8_FFPTR && class_id <= JS_CLASS_UINT64_FFPTR )
        return class_id - JS_CLASS_INT8_FFPTR;
    if ( class_id == JS_CLASS_MVARRAY )
        return JS_CLASS_MVPTR;
    return class_id;
}

static JSValue js_ffarray_set(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    int size_log2 = ffarray_or_ffptr_size_log2(p->class_id);
    JSObject *src_p;
    uint8_t *src_ptr;
    int64_t src_length;
    int64_t dst_length;
    int64_t idx = 0;
    uint8_t *dst_ptr;

    /* nop if no arguments given */
    if ( argc == 0 )
        return JS_DupValue(ctx, this_val);

    /* check for type mismatch */
    src_p = JS_VALUE_GET_OBJ(argv[0]);
    if ( normalized_class(p->class_id) != normalized_class(src_p->class_id) )
        return JS_ThrowTypeError(ctx, "type mismatch");

    /* set src_ptr and src_length */
    src_ptr = src_p->u.array.u.ptr;
    src_length = src_p->u.array.count;
    dst_length = p->u.array.count;

    /* check for targetOffset */
    if ( (argc > 1) && JS_ToInt64Clamp(ctx, &idx, argv[1], 0, dst_length, dst_length) < 0)
        return JS_ThrowRangeError(ctx, "error parsing targetOffset argument");
    if ( idx < 0 || (idx + src_length) > dst_length )
        return JS_ThrowRangeError(ctx, "out-of-bound access");

    dst_ptr = p->u.array.u.uint8_ptr + (idx << size_log2);
    memmove(dst_ptr, src_ptr, (src_length << size_log2));

    return JS_DupValue(ctx, this_val);
}

static JSValue js_ffarray_fill(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    int64_t fill;
    int32_t idx = 0;
    int32_t end = p->u.array.count;
    int32_t length;

    /* nop if no arguments given */
    if ( argc == 0 )
        return JS_DupValue(ctx, this_val);

    if ( !JS_IsNumber(argv[0]) || JS_ToInt64(ctx, &fill, argv[0]) )
        return JS_ThrowTypeError(ctx, "the first argument to fill() must be a number");

    /* optional range arguments */
    if ( js_parse_range(ctx, &idx, &end, &length, argc - 1, argv + 1) < 0 )
        return JS_ThrowRangeError(ctx, "error parsing range arguments");

    if ( idx < 0 || length <= 0 || end > p->u.array.count )
        return JS_ThrowRangeError(ctx, "out-of-bound access");

    switch ( ffarray_or_ffptr_size_log2(p->class_id) )
    {
    case 0: for ( size_t i = 0; i < length; i++ ) p->u.array.u.int8_ptr [idx + i] = fill; break;
    case 1: for ( size_t i = 0; i < length; i++ ) p->u.array.u.int16_ptr[idx + i] = fill; break;
    case 2: for ( size_t i = 0; i < length; i++ ) p->u.array.u.int32_ptr[idx + i] = fill; break;
    case 3: for ( size_t i = 0; i < length; i++ ) p->u.array.u.int64_ptr[idx + i] = fill; break;
    }

    return JS_DupValue(ctx, this_val);
}

static JSValue js_ffarray_reverse(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    int ret = js_typed_array_reverse_internal(ctx, this_val, 0);
    if ( unlikely(ret < 0) )
        return JS_EXCEPTION;
    return JS_DupValue(ctx, this_val);
}

static JSValue js_ffarray_sort(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv)
{
    return js_typed_array_sort_internal(ctx, this_val, argc, argv, 0);
}

static JSValue js_ffarray_new(JSContext *ctx, void **dst_ptr, uint32_t length, int class_id)
{
    switch ( class_id )
    {
    case JS_CLASS_INT8_FFARRAY:
    case JS_CLASS_INT8_FFPTR:
        return JS_NewInt8FFArray(ctx, (int8_t **) dst_ptr, length, 0);
    case JS_CLASS_UINT8_FFARRAY:
    case JS_CLASS_UINT8_FFPTR:
        return JS_NewUint8FFArray(ctx, (uint8_t **) dst_ptr, length, 0);
    case JS_CLASS_INT16_FFARRAY:
    case JS_CLASS_INT16_FFPTR:
        return JS_NewInt16FFArray(ctx, (int16_t **) dst_ptr, length, 0);
    case JS_CLASS_UINT16_FFARRAY:
    case JS_CLASS_UINT16_FFPTR:
        return JS_NewUint16FFArray(ctx, (uint16_t **) dst_ptr, length, 0);
    case JS_CLASS_INT32_FFARRAY:
    case JS_CLASS_INT32_FFPTR:
        return JS_NewInt32FFArray(ctx, (int32_t **) dst_ptr, length, 0);
    case JS_CLASS_UINT32_FFARRAY:
    case JS_CLASS_UINT32_FFPTR:
        return JS_NewUint32FFArray(ctx, (uint32_t **) dst_ptr, length, 0);
    case JS_CLASS_INT64_FFARRAY:
    case JS_CLASS_INT64_FFPTR:
        return JS_NewInt64FFArray(ctx, (int64_t **) dst_ptr, length, 0);
    case JS_CLASS_UINT64_FFARRAY:
    case JS_CLASS_UINT64_FFPTR:
        return JS_NewUint64FFArray(ctx, (uint64_t **) dst_ptr, length, 0);
    case JS_CLASS_MVARRAY:
    case JS_CLASS_MVPTR:
        return JS_NewMVArray(ctx, (int32_t **) dst_ptr, length, 0);
    case JS_CLASS_MVMASK:
        return JS_NewMVMask(ctx, (int64_t **) dst_ptr, length, 0);
    }
    return JS_EXCEPTION;
}

static JSValue js_ffarray_slice(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    int32_t idx = 0;
    int32_t end = p->u.array.count;
    int32_t length;
    JSValue dst_array;
    void *dst_ptr;
    int size_log2 = ffarray_or_ffptr_size_log2(p->class_id);

    /* optional range arguments */
    if ( js_parse_range(ctx, &idx, &end, &length, argc, argv) < 0 )
        return JS_ThrowRangeError(ctx, "error parsing range arguments");

    if ( idx < 0 || length <= 0 || end > p->u.array.count )
        return JS_ThrowRangeError(ctx, "out-of-bound access");

    dst_array = js_ffarray_new(ctx, &dst_ptr, length, p->class_id);

    if ( unlikely(JS_IsException(dst_array)) )
        return dst_array;

    memcpy(dst_ptr, p->u.array.u.uint8_ptr + (idx << size_log2), (length << size_log2));

    return dst_array;
}

static JSValue js_ffarray_dup(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    return js_ffarray_slice(ctx, this_val, 0, NULL);
}

static JSValue js_ffarray_find(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv, int findIndex)
{
    return js_typed_array_find_internal(ctx, this_val, argc, argv, findIndex, 0);
}

static JSValue js_ffarray_indexOf(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv, int special)
{
    return js_typed_array_indexOf_internal(ctx, this_val, argc, argv, special, 0);
}

static const JSCFunctionListEntry js_ffarray_base_proto_funcs[] = {
    JS_CFUNC_DEF("toString", 0, js_ffarray_toString ),
    JS_CFUNC_DEF("join", 1, js_ffarray_join ),
    JS_CFUNC_DEF("copyWithin", 1, js_ffarray_copyWithin ),
    JS_CFUNC_DEF("subarray", 2, js_ffarray_subarray ),
    JS_CFUNC_DEF("set", 2, js_ffarray_set ),
    JS_CFUNC_DEF("fill", 1, js_ffarray_fill ),
    JS_CFUNC_DEF("reverse", 0, js_ffarray_reverse ),
    JS_CFUNC_DEF("sort", 1, js_ffarray_sort ),
    JS_CFUNC_DEF("slice", 2, js_ffarray_slice ),
    JS_CFUNC_DEF("dup", 0, js_ffarray_dup ),
    JS_CFUNC_MAGIC_DEF("every", 1, js_array_every, special_every | special_FF ),
    JS_CFUNC_MAGIC_DEF("some", 1, js_array_every, special_some | special_FF ),
    JS_CFUNC_MAGIC_DEF("forEach", 1, js_array_every, special_forEach | special_FF ),
    JS_CFUNC_MAGIC_DEF("map", 1, js_array_every, special_map | special_FF ),
    JS_CFUNC_MAGIC_DEF("find", 1, js_ffarray_find, 0 ),
    JS_CFUNC_MAGIC_DEF("findIndex", 1, js_ffarray_find, 1 ),
    JS_CFUNC_MAGIC_DEF("indexOf", 1, js_ffarray_indexOf, special_indexOf ),
    JS_CFUNC_MAGIC_DEF("lastIndexOf", 1, js_ffarray_indexOf, special_lastIndexOf ),
    JS_CFUNC_MAGIC_DEF("includes", 1, js_ffarray_indexOf, special_includes ),
    JS_CFUNC_MAGIC_DEF("reduce", 1, js_array_reduce, special_reduce | special_FF ),
    JS_CFUNC_MAGIC_DEF("reduceRight", 1, js_array_reduce, special_reduceRight | special_FF ),
    JS_CFUNC_MAGIC_DEF("values", 0, js_create_array_iterator, JS_ITERATOR_KIND_VALUE ),
    JS_ALIAS_DEF("[Symbol.iterator]", "values" ),
    JS_CFUNC_MAGIC_DEF("keys", 0, js_create_array_iterator, JS_ITERATOR_KIND_KEY ),
    JS_CFUNC_MAGIC_DEF("entries", 0, js_create_array_iterator, JS_ITERATOR_KIND_KEY_AND_VALUE ),
};

/*********************************************************************/
/* FFPtr *************************************************************/
/*********************************************************************/

static inline JSValue JS_InitFFPtr(JSContext *ctx, JSValue val, void *ptr, uint32_t len)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    p->prop[0].u.value = JS_NewInt32(ctx, len);
    p->u.array.u.values = ptr;
    p->u.array.count = len;
    return val;
}

static JSValue js_ffptr_constructor(JSContext *ctx,
                                    JSValueConst new_target,
                                    int argc, JSValueConst *argv,
                                    int magic)
{
    JSObject *p;
    int class_id;
    JSValue val;

    /* check for type mismatch */
    if ( JS_VALUE_GET_TAG(argv[0]) != JS_TAG_OBJECT )
        return JS_ThrowTypeError(ctx, "type mismatch");
    p = JS_VALUE_GET_OBJ(argv[0]);
    class_id = p->class_id;
    if ( class_id >= JS_CLASS_INT8_FFARRAY && class_id <= JS_CLASS_UINT64_FFARRAY )
        class_id = JS_CLASS_INT8_FFPTR + (class_id - JS_CLASS_INT8_FFARRAY);
    if ( class_id < JS_CLASS_INT8_FFPTR || class_id > JS_CLASS_UINT64_FFPTR )
        return JS_ThrowTypeError(ctx, "type mismatch");
    if ( class_id != (JS_CLASS_INT8_FFPTR + magic) )
        return JS_ThrowTypeError(ctx, "type mismatch");

    if ( likely(JS_IsUndefined(new_target)) )
        val = JS_NewObjectFromShape(ctx, js_dup_shape(ctx->ffptr_shapes[magic]), class_id);
    else
        val = js_create_from_ctor(ctx, new_target, class_id);
    if ( unlikely(JS_IsException(val)) )
        return val;

    return JS_InitFFPtr(ctx, val, p->u.array.u.ptr, p->u.array.count);
}

static inline JSValue JS_NewFFPtr(JSContext *ctx, JSShape *sh, void *ptr, uint32_t len, int class_id)
{
    JSValue val = JS_NewObjectFromShape(ctx, js_dup_shape(sh), class_id);
    if ( unlikely(JS_IsException(val)) )
        return val;
    return JS_InitFFPtr(ctx, val, ptr, len);
}

JSValue JS_NewInt8FFPtr  (JSContext *ctx, int8_t   *pint8,   uint32_t len) { return JS_NewFFPtr(ctx, ctx->ffptr_shapes[0], (void *) pint8,   len, JS_CLASS_INT8_FFPTR); }
JSValue JS_NewUint8FFPtr (JSContext *ctx, uint8_t  *puint8,  uint32_t len) { return JS_NewFFPtr(ctx, ctx->ffptr_shapes[1], (void *) puint8,  len, JS_CLASS_UINT8_FFPTR); }
JSValue JS_NewInt16FFPtr (JSContext *ctx, int16_t  *pint16,  uint32_t len) { return JS_NewFFPtr(ctx, ctx->ffptr_shapes[2], (void *) pint16,  len, JS_CLASS_INT16_FFPTR); }
JSValue JS_NewUint16FFPtr(JSContext *ctx, uint16_t *puint16, uint32_t len) { return JS_NewFFPtr(ctx, ctx->ffptr_shapes[3], (void *) puint16, len, JS_CLASS_UINT16_FFPTR); }
JSValue JS_NewInt32FFPtr (JSContext *ctx, int32_t  *pint32,  uint32_t len) { return JS_NewFFPtr(ctx, ctx->ffptr_shapes[4], (void *) pint32,  len, JS_CLASS_INT32_FFPTR); }
JSValue JS_NewUint32FFPtr(JSContext *ctx, uint32_t *puint32, uint32_t len) { return JS_NewFFPtr(ctx, ctx->ffptr_shapes[5], (void *) puint32, len, JS_CLASS_UINT32_FFPTR); }
JSValue JS_NewInt64FFPtr (JSContext *ctx, int64_t  *pint64,  uint32_t len) { return JS_NewFFPtr(ctx, ctx->ffptr_shapes[6], (void *) pint64,  len, JS_CLASS_INT64_FFPTR); }
JSValue JS_NewUint64FFPtr(JSContext *ctx, uint64_t *puint64, uint32_t len) { return JS_NewFFPtr(ctx, ctx->ffptr_shapes[7], (void *) puint64, len, JS_CLASS_UINT64_FFPTR); }

int JS_IsInt8FFPtr  (JSValueConst val) { return JS_IsObjectClass(val, JS_CLASS_INT8_FFPTR)   != NULL; }
int JS_IsUint8FFPtr (JSValueConst val) { return JS_IsObjectClass(val, JS_CLASS_UINT8_FFPTR)  != NULL; }
int JS_IsInt16FFPtr (JSValueConst val) { return JS_IsObjectClass(val, JS_CLASS_INT16_FFPTR)  != NULL; }
int JS_IsUint16FFPtr(JSValueConst val) { return JS_IsObjectClass(val, JS_CLASS_UINT16_FFPTR) != NULL; }
int JS_IsInt32FFPtr (JSValueConst val) { return JS_IsObjectClass(val, JS_CLASS_INT32_FFPTR)  != NULL; }
int JS_IsUint32FFPtr(JSValueConst val) { return JS_IsObjectClass(val, JS_CLASS_UINT32_FFPTR) != NULL; }
int JS_IsInt64FFPtr (JSValueConst val) { return JS_IsObjectClass(val, JS_CLASS_INT64_FFPTR)  != NULL; }
int JS_IsUint64FFPtr(JSValueConst val) { return JS_IsObjectClass(val, JS_CLASS_UINT64_FFPTR) != NULL; }

int JS_GetInt8FFPtr  (JSValueConst val, int8_t   **pint8,   uint32_t *plen) { return JS_GetFFArray(val, (void **) pint8,   plen, JS_CLASS_INT8_FFPTR); }
int JS_GetUint8FFPtr (JSValueConst val, uint8_t  **puint8,  uint32_t *plen) { return JS_GetFFArray(val, (void **) puint8,  plen, JS_CLASS_UINT8_FFPTR); }
int JS_GetInt16FFPtr (JSValueConst val, int16_t  **pint16,  uint32_t *plen) { return JS_GetFFArray(val, (void **) pint16,  plen, JS_CLASS_INT16_FFPTR); }
int JS_GetUint16FFPtr(JSValueConst val, uint16_t **puint16, uint32_t *plen) { return JS_GetFFArray(val, (void **) puint16, plen, JS_CLASS_UINT16_FFPTR); }
int JS_GetInt32FFPtr (JSValueConst val, int32_t  **pint32,  uint32_t *plen) { return JS_GetFFArray(val, (void **) pint32,  plen, JS_CLASS_INT32_FFPTR); }
int JS_GetUint32FFPtr(JSValueConst val, uint32_t **puint32, uint32_t *plen) { return JS_GetFFArray(val, (void **) puint32, plen, JS_CLASS_UINT32_FFPTR); }
int JS_GetInt64FFPtr (JSValueConst val, int64_t  **pint64,  uint32_t *plen) { return JS_GetFFArray(val, (void **) pint64,  plen, JS_CLASS_INT64_FFPTR); }
int JS_GetUint64FFPtr(JSValueConst val, uint64_t **puint64, uint32_t *plen) { return JS_GetFFArray(val, (void **) puint64, plen, JS_CLASS_UINT64_FFPTR); }

/*********************************************************************/
void JS_AddIntrinsicFFArrays(JSContext *ctx)
{
    for ( size_t i = JS_CLASS_INT8_FFARRAY; i <= JS_CLASS_UINT64_FFARRAY; i++ )
    {
        size_t magic = i - JS_CLASS_INT8_FFARRAY;
        char buf[ATOM_GET_STR_BUF_SIZE];
        const char *name = JS_AtomGetStr(ctx, buf, sizeof(buf), JS_ATOM_Int8FFArray + magic);
        ctx->class_proto[i] = JS_NewObjectProtoClass(ctx, ctx->class_proto[JS_CLASS_OBJECT], i);
        JS_SetPropertyFunctionList(ctx, ctx->class_proto[i], js_ffarray_base_proto_funcs, countof(js_ffarray_base_proto_funcs));
        ctx->ffarray_shapes[magic] = create_arraylike_shape(ctx, ctx->class_proto[i]);
        JS_NewGlobalCConstructorMagic(ctx, name, js_ffarray_constructor, 1, magic, ctx->class_proto[i]);
    }
    for ( size_t i = JS_CLASS_INT8_FFPTR; i <= JS_CLASS_UINT64_FFPTR; i++ )
    {
        size_t magic = i - JS_CLASS_INT8_FFPTR;
        char buf[ATOM_GET_STR_BUF_SIZE];
        const char *name = JS_AtomGetStr(ctx, buf, sizeof(buf), JS_ATOM_Int8FFPtr + magic);
        ctx->class_proto[i] = JS_NewObjectProtoClass(ctx, ctx->class_proto[JS_CLASS_OBJECT], i);
        JS_SetPropertyFunctionList(ctx, ctx->class_proto[i], js_ffarray_base_proto_funcs, countof(js_ffarray_base_proto_funcs));
        ctx->ffptr_shapes[magic] = create_arraylike_shape(ctx, ctx->class_proto[i]);
        JS_NewGlobalCConstructorMagic(ctx, name, js_ffptr_constructor, 1, magic, ctx->class_proto[i]);
    }
}
