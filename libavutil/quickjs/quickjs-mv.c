/*********************************************************************/
#define mv_op_add     0x00
#define mv_op_sub     0x01
#define mv_op_mul     0x02
#define mv_op_div     0x03
#define mv_op_ass     0x04
#define mv_cmp_op__eq 0x05
#define mv_cmp_op_neq 0x06
#define mv_cmp_op__gt 0x07
#define mv_cmp_op_gte 0x08
#define mv_cmp_op__lt 0x09
#define mv_cmp_op_lte 0x0A
#define mv_op_h       0x10
#define mv_op_v       0x20

static inline int js_mv_parse_args(JSContext *ctx, int32_t *mv, int argc, JSValueConst *argv)
{
    uint32_t tag;
    /* ()      -> leave as-is */
    /* (null)  -> null */
    /* (mv)    -> [ mv[0], mv[1] ]*/
    /* (x, y)  -> [ x, y ] */
    if ( argc <= 0 )
        return 0;
    tag = JS_VALUE_GET_TAG(argv[0]);
    if ( tag == JS_TAG_NULL )
    {
        mv[0] = 0x80000000;
        mv[1] = 0x80000000;
        return 1;
    }
    if ( tag == JS_TAG_MV )
    {
        mv[0] = JS_VALUE_GET_MV0(argv[0]);
        mv[1] = JS_VALUE_GET_MV1(argv[0]);
        return 1;
    }
    if ( tag == JS_TAG_OBJECT )
    {
        JSObject *p = JS_VALUE_GET_OBJ(argv[0]);
        switch ( p->class_id )
        {
        case JS_CLASS_MV:
        case JS_CLASS_MVREF:
            mv[0] = p->u.array.u.int32_ptr[0];
            mv[1] = p->u.array.u.int32_ptr[1];
            return 1;
        }
    }
    if ( argc < 2 )
        return -1;
    if ( JS_IsNumber(argv[0]) && JS_ToInt32(ctx, &mv[0], argv[0]) == 0
      && JS_IsNumber(argv[1]) && JS_ToInt32(ctx, &mv[1], argv[1]) == 0 )
        return 2;
    return -1;
}

static inline JSValue js_mv_or_el_parse_args(
        JSContext *ctx,
        int32_t *mv,
        int argc,
        JSValueConst *argv,
        int magic)
{
    const int do_h = !!(magic & mv_op_h);
    const int do_v = !!(magic & mv_op_v);
    /* parse arguments */
    if ( do_h && do_v )
    {
        if ( js_mv_parse_args(ctx, mv, argc, argv) != argc )
            return JS_ThrowTypeError(ctx, "error parsing motion vector argument");
    }
    else
    {
        int32_t *ptr32 = &mv[(do_v && !do_h) ? 1 : 0];
        if ( argc != 1 || !JS_IsNumber(argv[0]) || JS_ToInt32(ctx, ptr32, argv[0]) < 0 )
            return JS_ThrowTypeError(ctx, "error parsing motion vector element argument");
    }
    return JS_UNDEFINED;
}

/*********************************************************************/
/* MV ****************************************************************/
/*********************************************************************/

static JSValue js_mv_constructor(JSContext *ctx,
                                 JSValueConst new_target,
                                 int argc, JSValueConst *argv)
{
    if ( likely(JS_IsUndefined(new_target)) )
    {
        // MV(mv0, mv1)
        JSValue val;
        if ( js_mv_parse_args(ctx, val.u.mv, argc, argv) != argc )
            return JS_ThrowTypeError(ctx, "error parsing motion vector argument");
        val.tag = JS_TAG_MV;
        return val;
    }
    else
    {
        // new MV(mv0, mv1)
        JSObject *p;
        JSValue val = js_create_from_ctor(ctx, new_target, JS_CLASS_MV);
        if ( unlikely(JS_IsException(val)) )
            return val;

        /* parse arguments */
        p = JS_VALUE_GET_OBJ(val);
        if ( js_mv_parse_args(ctx, p->u.array.u1.mv, argc, argv) != argc )
        {
            JS_FreeValue(ctx, val);
            return JS_ThrowTypeError(ctx, "error parsing motion vector argument");
        }
        return val;
    }
}

JSValue JS_NewMV(JSContext *ctx, int32_t x, int32_t y)
{
    JSValue val;
    JSObject *p;

    /* create object */
    val = JS_NewObjectFromShape(ctx, js_dup_shape(ctx->mv_shape), JS_CLASS_MV);
    if ( unlikely(JS_IsException(val)) )
        return val;

    /* set values */
    p = JS_VALUE_GET_OBJ(val);
    p->u.array.u.int32_ptr[0] = x;
    p->u.array.u.int32_ptr[1] = y;

    return val;
}

int JS_IsMV(JSValueConst val)
{
    return JS_IsObjectClass(val, JS_CLASS_MV) != NULL;
}

int JS_GetMV(JSValueConst val, int32_t *px, int32_t *py)
{
    JSObject *p = JS_IsObjectClass(val, JS_CLASS_MV);
    if ( p == NULL )
        return FALSE;
    *px = p->u.array.u.int32_ptr[0];
    *py = p->u.array.u.int32_ptr[1];
    return TRUE;
}

static force_inline
void js_mv_op_kernel(int32_t *dst_mv, int32_t mv0, int32_t mv1, int magic)
{
    const int op = (magic & 0x0F);
    const int do_h = (magic & mv_op_h);
    const int do_v = (magic & mv_op_v);
    /* set values */
    switch ( op )
    {
    case mv_op_add:
        if ( do_h )
            dst_mv[0] += mv0;
        if ( do_v )
            dst_mv[1] += mv1;
        break;
    case mv_op_sub:
        if ( do_h )
            dst_mv[0] -= mv0;
        if ( do_v )
            dst_mv[1] -= mv1;
        break;
    case mv_op_mul:
        if ( do_h )
            dst_mv[0] *= mv0;
        if ( do_v )
            dst_mv[1] *= mv1;
        break;
    case mv_op_div:
        if ( do_h && mv0 != 1 )
            dst_mv[0] = lround((double) dst_mv[0] / mv0);
        if ( do_v && mv1 != 1 )
            dst_mv[1] = lround((double) dst_mv[1] / mv1);
        break;
    case mv_op_ass:
        if ( do_h )
            dst_mv[0] = mv0;
        if ( do_v )
            dst_mv[1] = mv1;
        break;
    }
}

static JSValue js_mv_op(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv, int magic)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    int32_t *dst_mv = p->u.array.u.int32_ptr;
    int32_t src_mv[2] = { 0, 0 };
    JSValue ret;

    /* parse arguments */
    ret = js_mv_or_el_parse_args(ctx, src_mv, argc, argv, magic);
    if ( unlikely(JS_IsException(ret)) )
        return ret;

    /* only allow null when assigning */
    if ( src_mv[0] == 0x80000000 && (magic != (mv_op_ass | mv_op_h | mv_op_v)) )
        return JS_DupValue(ctx, this_val);

    /* set values */
    switch ( magic )
    {
    case mv_op_add | mv_op_h | mv_op_v: js_mv_op_kernel(dst_mv, src_mv[0], src_mv[1], mv_op_add | mv_op_h | mv_op_v); break;
    case mv_op_sub | mv_op_h | mv_op_v: js_mv_op_kernel(dst_mv, src_mv[0], src_mv[1], mv_op_sub | mv_op_h | mv_op_v); break;
    case mv_op_mul | mv_op_h | mv_op_v: js_mv_op_kernel(dst_mv, src_mv[0], src_mv[1], mv_op_mul | mv_op_h | mv_op_v); break;
    case mv_op_div | mv_op_h | mv_op_v: js_mv_op_kernel(dst_mv, src_mv[0], src_mv[1], mv_op_div | mv_op_h | mv_op_v); break;
    case mv_op_ass | mv_op_h | mv_op_v: js_mv_op_kernel(dst_mv, src_mv[0], src_mv[1], mv_op_ass | mv_op_h | mv_op_v); break;
    case mv_op_add | mv_op_h:           js_mv_op_kernel(dst_mv, src_mv[0], src_mv[1], mv_op_add | mv_op_h);           break;
    case mv_op_sub | mv_op_h:           js_mv_op_kernel(dst_mv, src_mv[0], src_mv[1], mv_op_sub | mv_op_h);           break;
    case mv_op_mul | mv_op_h:           js_mv_op_kernel(dst_mv, src_mv[0], src_mv[1], mv_op_mul | mv_op_h);           break;
    case mv_op_div | mv_op_h:           js_mv_op_kernel(dst_mv, src_mv[0], src_mv[1], mv_op_div | mv_op_h);           break;
    case mv_op_ass | mv_op_h:           js_mv_op_kernel(dst_mv, src_mv[0], src_mv[1], mv_op_ass | mv_op_h);           break;
    case mv_op_add | mv_op_v:           js_mv_op_kernel(dst_mv, src_mv[0], src_mv[1], mv_op_add | mv_op_v);           break;
    case mv_op_sub | mv_op_v:           js_mv_op_kernel(dst_mv, src_mv[0], src_mv[1], mv_op_sub | mv_op_v);           break;
    case mv_op_mul | mv_op_v:           js_mv_op_kernel(dst_mv, src_mv[0], src_mv[1], mv_op_mul | mv_op_v);           break;
    case mv_op_div | mv_op_v:           js_mv_op_kernel(dst_mv, src_mv[0], src_mv[1], mv_op_div | mv_op_v);           break;
    case mv_op_ass | mv_op_v:           js_mv_op_kernel(dst_mv, src_mv[0], src_mv[1], mv_op_ass | mv_op_v);           break;
    }

    return JS_DupValue(ctx, this_val);
}

static JSValue js_mv_compare(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv, int magic)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    int32_t *dst_mv = p->u.array.u.int32_ptr;
    int32_t src_mv[2] = { 0, 0 };
    const int op = (magic & 0x0F);
    const int do_h = !!(magic & mv_op_h);
    const int do_v = !!(magic & mv_op_v);
    int ok = TRUE;
    JSValue ret;

    /* parse arguments */
    ret = js_mv_or_el_parse_args(ctx, src_mv, argc, argv, magic);
    if ( unlikely(JS_IsException(ret)) )
        return ret;

    /* compare */
    if ( do_h )
        ok = ok && (dst_mv[0] == src_mv[0]);
    if ( do_v )
        ok = ok && (dst_mv[1] == src_mv[1]);

    /* neq */
    if ( op == mv_cmp_op_neq )
        ok = !ok;

    return ok ? JS_TRUE : JS_FALSE;
}

static JSValue js_mv_magnitude(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    int32_t *src_mv = p->u.array.u.int32_ptr;
    int32_t mv0 = src_mv[0];
    int32_t mv1 = src_mv[1];
    uint64_t magnitude_sq = (mv0 * mv0) + (mv1 * mv1);
    return JS_NewFloat64(ctx, sqrt(magnitude_sq));
}

static JSValue js_mv_magnitude_sq(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    int32_t *src_mv = p->u.array.u.int32_ptr;
    int32_t mv0 = src_mv[0];
    int32_t mv1 = src_mv[1];
    uint64_t magnitude_sq = (mv0 * mv0) + (mv1 * mv1);
    return JS_NewInt64(ctx, magnitude_sq);
}

static force_inline
void js_mv_swap_hv_kernel(int32_t *mv)
{
    int32_t mv0 = mv[0];
    int32_t mv1 = mv[1];
    if ( mv0 != 0x80000000 )
    {
        mv[0] = mv1;
        mv[1] = mv0;
    }
}

static JSValue js_mv_swap_hv(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    js_mv_swap_hv_kernel(p->u.array.u.int32_ptr);
    return JS_DupValue(ctx, this_val);
}

static JSValue js_mv_clear(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    int32_t *dst_mv = p->u.array.u.int32_ptr;
    dst_mv[0] = 0;
    dst_mv[1] = 0;
    return JS_DupValue(ctx, this_val);
}

static const JSCFunctionListEntry js_mv_proto_funcs[] = {
    JS_CFUNC_DEF("toString", 0, js_ffarray_toString ),
    JS_CFUNC_DEF("magnitude", 0, js_mv_magnitude ),
    JS_CFUNC_DEF("magnitude_sq", 0, js_mv_magnitude_sq ),
    JS_CFUNC_DEF("swap_hv", 0, js_mv_swap_hv ),
    JS_CFUNC_DEF("clear", 0, js_mv_clear ),
    JS_CFUNC_MAGIC_DEF("add", 2, js_mv_op, mv_op_add | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("sub", 2, js_mv_op, mv_op_sub | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("mul", 2, js_mv_op, mv_op_mul | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("div", 2, js_mv_op, mv_op_div | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("assign", 2, js_mv_op, mv_op_ass | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("add_h", 1, js_mv_op, mv_op_add | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("sub_h", 1, js_mv_op, mv_op_sub | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("mul_h", 1, js_mv_op, mv_op_mul | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("div_h", 1, js_mv_op, mv_op_div | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("assign_h", 1, js_mv_op, mv_op_ass | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("add_v", 1, js_mv_op, mv_op_add | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("sub_v", 1, js_mv_op, mv_op_sub | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("mul_v", 1, js_mv_op, mv_op_mul | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("div_v", 1, js_mv_op, mv_op_div | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("assign_v", 1, js_mv_op, mv_op_ass | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("compare_eq", 2, js_mv_compare, mv_cmp_op__eq | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("compare_neq", 2, js_mv_compare, mv_cmp_op_neq | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("compare_eq_h", 1, js_mv_compare, mv_cmp_op__eq | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("compare_neq_h", 1, js_mv_compare, mv_cmp_op_neq | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("compare_eq_v", 1, js_mv_compare, mv_cmp_op__eq | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("compare_neq_v", 1, js_mv_compare, mv_cmp_op_neq | mv_op_v ),
};

/*********************************************************************/
/* MVRef *************************************************************/
/*********************************************************************/

static JSValue js_mvref_constructor(JSContext *ctx,
                                    JSValueConst new_target,
                                    int argc, JSValueConst *argv)
{
    JSObject *p;
    if ( argc != 1 )
    {
arg_error:
        return JS_ThrowTypeError(ctx, "MVRef() takes either an MV object or an MVRef as sole argument");
    }
    p = JS_IsObjectClass(argv[0], JS_CLASS_MVREF);
    if ( p == NULL )
        p = JS_IsObjectClass(argv[0], JS_CLASS_MV);
    if ( p == NULL )
        goto arg_error;

    return JS_NewFFPtr(ctx, ctx->mvref_shape, p->u.array.u.int32_ptr, 2, JS_CLASS_MVREF);
}

static JSValue JS_NewMVRef(JSContext *ctx, int32_t *ptr)
{
    return JS_NewFFPtr(ctx, ctx->mvref_shape, (void *) ptr, 2, JS_CLASS_MVREF);
}

/*********************************************************************/
/* MVArray ***********************************************************/
/*********************************************************************/

static void js_mvarray_finalizer(JSRuntime *rt, JSValue val)
{
    js_ffarray_finalizer(rt, val);
    js_mvptr_finalizer(rt, val);
}

static JSValue js_mvarray_constructor(JSContext *ctx,
                                      JSValueConst new_target,
                                      int argc, JSValueConst *argv)
{
    int32_t *ptr32;
    uint64_t len;
    JSValue val;
    if ( JS_ToIndex(ctx, &len, argv[0]) )
        return JS_ThrowTypeError(ctx, "MVArray() takes a positive length as argument");
    if ( likely(JS_IsUndefined(new_target)) )
        val = JS_NewObjectFromShape(ctx, js_dup_shape(ctx->mvarray_shape), JS_CLASS_MVARRAY);
    else
        val = js_create_from_ctor(ctx, new_target, JS_CLASS_MVARRAY);
    if ( unlikely(JS_IsException(val)) )
        return val;
    return JS_InitFFArray(ctx, val, (void *) &ptr32, len, 1, JS_CLASS_MVARRAY);
}

JSValue JS_NewMVArray(JSContext *ctx, int32_t **pint32, uint32_t len, int set_zero)
{
    JSValue val = JS_NewObjectFromShape(ctx, js_dup_shape(ctx->mvarray_shape), JS_CLASS_MVARRAY);
    if ( unlikely(JS_IsException(val)) )
        return val;
    return JS_InitFFArray(ctx, val, (void *) pint32, len, set_zero, JS_CLASS_MVARRAY);
}

int JS_IsMVArray(JSValueConst val)
{
    return JS_IsObjectClass(val, JS_CLASS_MVARRAY) != NULL;
}

int JS_GetMVArray(JSValueConst val, int32_t **pint32, uint32_t *plen)
{
    return JS_GetFFArray(val, (void **) pint32, plen, JS_CLASS_MVARRAY);
}

static force_inline
void print_mvarray(StringBuffer *b, int32_t *ptr, uint32_t len, int c, JSString *str_p)
{
    string_buffer_putc8(b, '[');
    for ( size_t i = 0; i < len; i++ )
    {
        int32_t mv0 = ptr[0];
        char buf[32];
        char *q = buf;
        if ( i != 0 )
            *q++ = ',';
        if ( mv0 == 0x80000000 )
        {
            *q++ = 'n';
            *q++ = 'u';
            *q++ = 'l';
            *q++ = 'l';
        }
        else
        {
            *q++ = '[';
            q = output_int32(q, ptr[0]);
            *q++ = ',';
            q = output_int32(q, ptr[1]);
            *q++ = ']';
        }
        *q++ = '\0';
        string_buffer_puts8(b, buf);
        ptr += 2;
    }
    string_buffer_putc8(b, ']');
}

static force_inline
void js_mvarray_fill_internal(int32_t *dst_ptr, int32_t *src_mv, uint32_t idx, uint32_t length)
{
    for ( size_t i = 0; i < length; i++ )
    {
        dst_ptr[((idx + i) << 1) + 0] = src_mv[0];
        dst_ptr[((idx + i) << 1) + 1] = src_mv[1];
    }
}

static JSValue js_mvarray_fill(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    int32_t mv[2];
    int32_t idx = 0;
    int32_t end = p->u.array.count;
    int32_t length;

    /* nop if no arguments given */
    if ( argc == 0 )
        return JS_DupValue(ctx, this_val);

    if ( mv_to_int32ptr(mv, argv[0]) < 0 )
        return JS_ThrowTypeError(ctx, "error parsing motion vector argument");

    argc--;
    argv++;
    /* optional range arguments */
    if ( js_parse_range(ctx, &idx, &end, &length, argc, argv) < 0 )
        return JS_ThrowRangeError(ctx, "error parsing range arguments");

    if ( idx < 0 || length <= 0 || end > p->u.array.count )
        return JS_ThrowRangeError(ctx, "out-of-bound access");

    js_mvarray_fill_internal(p->u.array.u.int32_ptr, mv, idx, length);

    return JS_DupValue(ctx, this_val);
}

static JSValue js_mvarray_sort(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv)
{
    return js_typed_array_sort_internal(ctx, this_val, argc, argv, -1);
}

static force_inline
JSValue js_mvarray_every_internal(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv, int special)
{
    const int op = (special & 0x7);
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    int64_t *ptr64 = p->u.array.u.int64_ptr;
    uint32_t length = p->u.array.count;
    JSValue obj = JS_DupValue(ctx, this_val);
    JSValue ret = JS_UNDEFINED;
    JSValueConst this_arg = JS_UNDEFINED;
    const int has_mask_arg = (op == special_maskedForEach);
    const int this_is_mask = !!(special & special_MVMASK);
    int64_t *mask_ptr = NULL;
    JSValueConst func;
    int32_t *dst_ptr;

    if ( has_mask_arg )
    {
        JSObject *mask_p = JS_IsObjectClass(argv[0], JS_CLASS_MVMASK);
        if ( mask_p == NULL )
        {
            JS_ThrowTypeError(ctx, "first argument is not an MVMask");
            goto exception;
        }
        if ( mask_p->u.array.count != length )
        {
            JS_ThrowRangeError(ctx, "MVMask length mismatch");
            goto exception;
        }
        mask_ptr = mask_p->u.array.u.int64_ptr;
        argc--;
        argv++;
    }

    if ( argc > 1 )
        this_arg = argv[1];

    func = argv[0];
    if ( check_function(ctx, func) )
        goto exception;

    switch ( op )
    {
    case special_every:
        ret = JS_TRUE;
        break;
    case special_some:
        ret = JS_FALSE;
        break;
    case special_map:
        ret = JS_NewMVArray(ctx, &dst_ptr, length, 0);
        if ( JS_IsException(ret) )
            goto exception;
        break;
    }

    for ( uint32_t i = 0; i < length; i++ )
    {
        JSValue val_i = JS_NewInt32(ctx, i);
        JSValue val;
        JSValue res;
        JSValueConst args[3];
        if ( has_mask_arg && mask_ptr[i] == 0 )
            continue;
        if ( this_is_mask )
            val = JS_NewBool(ctx, ptr64[i]);
        else
            val = JS_GetMVRef(ctx, p, i);
        args[0] = val;
        args[1] = val_i;
        args[2] = obj;
        res = JS_Call(ctx, func, this_arg, 4, args);
        if ( !this_is_mask )
            JS_FreeValue(ctx, val);
        if ( JS_IsException(res) )
            goto exception;
        switch ( op )
        {
        case special_every:
            if ( !JS_ToBoolFree(ctx, res) )
            {
                ret = JS_FALSE;
                goto done;
            }
            break;
        case special_some:
            if ( JS_ToBoolFree(ctx, res) )
            {
                ret = JS_TRUE;
                goto done;
            }
            break;
        case special_map:
            switch ( JS_VALUE_GET_TAG(res) )
            {
            case JS_TAG_MV:
                *dst_ptr++ = JS_VALUE_GET_MV0(res);
                *dst_ptr++ = JS_VALUE_GET_MV1(res);
                break;
            case JS_TAG_OBJECT:
                {
                    JSObject *res_p = JS_VALUE_GET_OBJ(res);
                    switch ( res_p->class_id )
                    {
                    case JS_CLASS_MV:
                    case JS_CLASS_MVREF:
                        *dst_ptr++ = res_p->u.array.u.int32_ptr[0];
                        *dst_ptr++ = res_p->u.array.u.int32_ptr[1];
                        break;
                    default:
                        goto exception;
                    }
                    JS_FreeValue(ctx, res);
                }
            default:
                goto exception;
            }
            break;
        default:
            JS_FreeValue(ctx, res);
            break;
        }
    }
done:
    JS_FreeValue(ctx, obj);
    return ret;

exception:
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue js_mvarray_every        (JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) { return js_mvarray_every_internal(ctx, this_val, argc, argv, special_every        ); }
static JSValue js_mvarray_some         (JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) { return js_mvarray_every_internal(ctx, this_val, argc, argv, special_some         ); }
static JSValue js_mvarray_forEach      (JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) { return js_mvarray_every_internal(ctx, this_val, argc, argv, special_forEach      ); }
static JSValue js_mvarray_maskedForEach(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) { return js_mvarray_every_internal(ctx, this_val, argc, argv, special_maskedForEach); }
static JSValue js_mvarray_map          (JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) { return js_mvarray_every_internal(ctx, this_val, argc, argv, special_map          ); }

static JSValue js_mvarray_indexOf(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv, int special)
{
    return js_typed_array_indexOf_internal(ctx, this_val, argc, argv, special, -1);
}

static inline
int js_mvXXarray_op_parse_args_src(
        JSContext *ctx,
        int argc,
        JSValueConst *argv,
        int32_t *src_mv,
        JSValue **psrc_values,
        int32_t **psrc_ptr,
        uint32_t *psrc_height,
        uint32_t *psrc_width,
        int *pmagic)
{
    int magic = *pmagic;
    const int op = (magic & 0x0F);
    const int do_h = !!(magic & mv_op_h);
    const int do_v = !!(magic & mv_op_v);
    int arg_idx = 0;
    JSObject *src_p = NULL;
    int32_t mvx;

    if ( argc == 0 )
        return -1;

    /* JS_CLASS_MV2DARRAY */
    if ( (psrc_values != NULL) && (psrc_height != NULL) )
        src_p = JS_IsObjectClass(argv[0], JS_CLASS_MV2DARRAY);
    if ( src_p != NULL )
    {
        struct JSMV2DArray *mv2d = src_p->u.array.u1.mv2darray;
        *psrc_values = src_p->u.array.u.values;
        *psrc_height = mv2d->height;
        *psrc_width = mv2d->width;
        return 1;
    }
    /* JS_CLASS_MVARRAY / JS_CLASS_MVPTR */
    if ( src_p == NULL )
        src_p = JS_IsObjectClass(argv[0], JS_CLASS_MVARRAY);
    if ( src_p == NULL )
        src_p = JS_IsObjectClass(argv[0], JS_CLASS_MVPTR);
    if ( src_p != NULL )
    {
        *psrc_ptr = src_p->u.array.u.int32_ptr;
        *psrc_width = src_p->u.array.count;
        return 1;
    }

    /* null, JS_CLASS_MV, JS_CLASS_MVREF, (x, y) */
    arg_idx = js_mv_parse_args(ctx, src_mv, argc, argv);
    if ( arg_idx > 0 )
    {
        *psrc_ptr = src_mv;
        return arg_idx;
    }

    /* (n) -> magnitude_sq() for _hv comparisons */
    if ( (op >= mv_cmp_op__eq && op <= mv_cmp_op_lte) && do_h && do_v )
    {
        if ( JS_IsNumber(argv[0]) && JS_ToInt32(ctx, src_mv, argv[0]) == 0 )
        {
            *pmagic &= ~(mv_op_h | mv_op_v);
            *psrc_ptr = src_mv;
            return 1;
        }
    }

    /* (n) -> MV(n,n) for _h or _v methods */
    if ( do_h != do_v && JS_IsNumber(argv[0]) && JS_ToInt32(ctx, &mvx, argv[0]) == 0 )
    {
        if ( do_h )
            src_mv[0] = mvx;
        if ( do_v )
            src_mv[1] = mvx;
        *psrc_ptr = src_mv;
        return 1;
    }

    return -1;
}

static inline
int js_mvXXarray_op_parse_args(
        JSContext *ctx,
        int argc,
        JSValueConst *argv,
        int32_t *src_mv,
        JSValue **psrc_values,
        int32_t **psrc_ptr,
        uint32_t *psrc_height,
        uint32_t *psrc_width,
        JSValue **pmask_values,
        int64_t **pmask_ptr,
        uint32_t *pmask_height,
        uint32_t *pmask_width,
        int *pmagic)
{
    JSObject *mask_p = NULL;
    int arg_idx = js_mvXXarray_op_parse_args_src(ctx, argc, argv, src_mv,
                                                 psrc_values, psrc_ptr, psrc_height, psrc_width,
                                                 pmagic);

    /* no src found */
    if ( arg_idx < 0 )
        return -1;

    argc -= arg_idx;
    argv += arg_idx;
    if ( argc == 0 )
        goto the_end;

    /* JS_CLASS_MV2DMASK */
    if ( (pmask_values != NULL) && (pmask_height != NULL) )
        mask_p = JS_IsObjectClass(argv[0], JS_CLASS_MV2DMASK);
    if ( mask_p != NULL )
    {
        struct JSMV2DArray *mv2d = mask_p->u.array.u1.mv2darray;
        *pmask_values = mask_p->u.array.u.values;
        *pmask_height = mv2d->height;
        *pmask_width = mv2d->width;
        arg_idx = 1;
        goto the_end;
    }
    /* JS_CLASS_MVMASK */
    mask_p = JS_IsObjectClass(argv[0], JS_CLASS_MVMASK);
    if ( mask_p != NULL )
    {
        *pmask_ptr = mask_p->u.array.u.int64_ptr;
        *pmask_width = mask_p->u.array.count;
        arg_idx = 1;
        goto the_end;
    }

the_end:
    return 0;
}

static force_inline
void js_mvarray_op_mv_kernel(int32_t *dst_mv, int32_t *src_mv, uint32_t length, int magic)
{
    const int op = (magic & 0x0F);
    int32_t mv0 = src_mv[0];
    int32_t mv1 = src_mv[1];
    if ( mv0 == 0x80000000 && (op != mv_op_ass) )
        return;
    /* set values */
    for ( uint32_t i = 0; i < length; i++ )
    {
        if ( dst_mv[0] == 0x80000000 && (magic != (mv_op_ass | mv_op_h | mv_op_v)) )
            goto next_line;
        js_mv_op_kernel(dst_mv, mv0, mv1, magic);
next_line:
        dst_mv += 2;
    }
}

static force_inline
void js_mvarray_op_mvarray_kernel(int32_t *dst_mv, int32_t *src_mv, uint32_t length, int magic)
{
    const int op = (magic & 0x0F);
    /* set values */
    for ( uint32_t i = 0; i < length; i++ )
    {
        if ( (dst_mv[0] == 0x80000000 || src_mv[0] == 0x80000000) && (op != mv_op_ass) )
            goto next_line;
        js_mv_op_kernel(dst_mv, src_mv[0], src_mv[1], magic);
next_line:
        src_mv += 2;
        dst_mv += 2;
    }
}

static no_inline void js_mvarray_op_add_mv     (int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { js_mvarray_op_mv_kernel     (dst_ptr, src_ptr, dst_len, mv_op_add | mv_op_h | mv_op_v); }
static no_inline void js_mvarray_op_sub_mv     (int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { js_mvarray_op_mv_kernel     (dst_ptr, src_ptr, dst_len, mv_op_sub | mv_op_h | mv_op_v); }
static no_inline void js_mvarray_op_mul_mv     (int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { js_mvarray_op_mv_kernel     (dst_ptr, src_ptr, dst_len, mv_op_mul | mv_op_h | mv_op_v); }
static no_inline void js_mvarray_op_div_mv     (int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { js_mvarray_op_mv_kernel     (dst_ptr, src_ptr, dst_len, mv_op_div | mv_op_h | mv_op_v); }
static no_inline void js_mvarray_op_ass_mv     (int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len)
{
    if ( src_ptr[0] == 0 && src_ptr[1] == 0 )
        memset(dst_ptr, 0x00, dst_len * 2 * sizeof(int32_t));
    else if ( src_ptr[0] == -1 && src_ptr[1] == -1 )
        memset(dst_ptr, 0xFF, dst_len * 2 * sizeof(int32_t));
    else
        js_mvarray_op_mv_kernel(dst_ptr, src_ptr, dst_len, mv_op_ass | mv_op_h | mv_op_v);
}
static no_inline void js_mvarray_op_add_mvarray(int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { js_mvarray_op_mvarray_kernel(dst_ptr, src_ptr, dst_len, mv_op_add | mv_op_h | mv_op_v); }
static no_inline void js_mvarray_op_sub_mvarray(int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { js_mvarray_op_mvarray_kernel(dst_ptr, src_ptr, dst_len, mv_op_sub | mv_op_h | mv_op_v); }
static no_inline void js_mvarray_op_mul_mvarray(int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { js_mvarray_op_mvarray_kernel(dst_ptr, src_ptr, dst_len, mv_op_mul | mv_op_h | mv_op_v); }
static no_inline void js_mvarray_op_div_mvarray(int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { js_mvarray_op_mvarray_kernel(dst_ptr, src_ptr, dst_len, mv_op_div | mv_op_h | mv_op_v); }
static no_inline void js_mvarray_op_ass_mvarray(int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { memcpy(dst_ptr, src_ptr, dst_len * sizeof(int32_t) * 2); }

static no_inline void js_mvarray_op_add_mv_h     (int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { js_mvarray_op_mv_kernel     (dst_ptr, src_ptr, dst_len, mv_op_add | mv_op_h); }
static no_inline void js_mvarray_op_sub_mv_h     (int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { js_mvarray_op_mv_kernel     (dst_ptr, src_ptr, dst_len, mv_op_sub | mv_op_h); }
static no_inline void js_mvarray_op_mul_mv_h     (int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { js_mvarray_op_mv_kernel     (dst_ptr, src_ptr, dst_len, mv_op_mul | mv_op_h); }
static no_inline void js_mvarray_op_div_mv_h     (int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { js_mvarray_op_mv_kernel     (dst_ptr, src_ptr, dst_len, mv_op_div | mv_op_h); }
static no_inline void js_mvarray_op_ass_mv_h     (int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { js_mvarray_op_mv_kernel     (dst_ptr, src_ptr, dst_len, mv_op_ass | mv_op_h); }
static no_inline void js_mvarray_op_add_mvarray_h(int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { js_mvarray_op_mvarray_kernel(dst_ptr, src_ptr, dst_len, mv_op_add | mv_op_h); }
static no_inline void js_mvarray_op_sub_mvarray_h(int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { js_mvarray_op_mvarray_kernel(dst_ptr, src_ptr, dst_len, mv_op_sub | mv_op_h); }
static no_inline void js_mvarray_op_mul_mvarray_h(int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { js_mvarray_op_mvarray_kernel(dst_ptr, src_ptr, dst_len, mv_op_mul | mv_op_h); }
static no_inline void js_mvarray_op_div_mvarray_h(int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { js_mvarray_op_mvarray_kernel(dst_ptr, src_ptr, dst_len, mv_op_div | mv_op_h); }
static no_inline void js_mvarray_op_ass_mvarray_h(int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { js_mvarray_op_mvarray_kernel(dst_ptr, src_ptr, dst_len, mv_op_ass | mv_op_h); }

static no_inline void js_mvarray_op_add_mv_v     (int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { js_mvarray_op_mv_kernel     (dst_ptr, src_ptr, dst_len, mv_op_add | mv_op_v); }
static no_inline void js_mvarray_op_sub_mv_v     (int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { js_mvarray_op_mv_kernel     (dst_ptr, src_ptr, dst_len, mv_op_sub | mv_op_v); }
static no_inline void js_mvarray_op_mul_mv_v     (int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { js_mvarray_op_mv_kernel     (dst_ptr, src_ptr, dst_len, mv_op_mul | mv_op_v); }
static no_inline void js_mvarray_op_div_mv_v     (int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { js_mvarray_op_mv_kernel     (dst_ptr, src_ptr, dst_len, mv_op_div | mv_op_v); }
static no_inline void js_mvarray_op_ass_mv_v     (int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { js_mvarray_op_mv_kernel     (dst_ptr, src_ptr, dst_len, mv_op_ass | mv_op_v); }
static no_inline void js_mvarray_op_add_mvarray_v(int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { js_mvarray_op_mvarray_kernel(dst_ptr, src_ptr, dst_len, mv_op_add | mv_op_v); }
static no_inline void js_mvarray_op_sub_mvarray_v(int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { js_mvarray_op_mvarray_kernel(dst_ptr, src_ptr, dst_len, mv_op_sub | mv_op_v); }
static no_inline void js_mvarray_op_mul_mvarray_v(int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { js_mvarray_op_mvarray_kernel(dst_ptr, src_ptr, dst_len, mv_op_mul | mv_op_v); }
static no_inline void js_mvarray_op_div_mvarray_v(int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { js_mvarray_op_mvarray_kernel(dst_ptr, src_ptr, dst_len, mv_op_div | mv_op_v); }
static no_inline void js_mvarray_op_ass_mvarray_v(int32_t *dst_ptr, int32_t *src_ptr, uint32_t dst_len) { js_mvarray_op_mvarray_kernel(dst_ptr, src_ptr, dst_len, mv_op_ass | mv_op_v); }

static force_inline
void js_mvarray_maskop_mv_kernel(int32_t *dst_mv, int32_t *src_mv, int64_t *mask_ptr, uint32_t length, int magic)
{
    const int op = (magic & 0x0F);
    int32_t mv0 = src_mv[0];
    int32_t mv1 = src_mv[1];
    if ( mv0 == 0x80000000 && (op != mv_op_ass) )
        return;
    /* set values */
    for ( uint32_t i = 0; i < length; i++ )
    {
        if ( mask_ptr[i] == 0 )
            goto next_line;
        if ( dst_mv[0] == 0x80000000 && (magic != (mv_op_ass | mv_op_h | mv_op_v)) )
            goto next_line;
        js_mv_op_kernel(dst_mv, mv0, mv1, magic);
next_line:
        dst_mv += 2;
    }
}

static force_inline
void js_mvarray_maskop_mvarray_kernel(int32_t *dst_mv, int32_t *src_mv, int64_t *mask_ptr, uint32_t length, int magic)
{
    const int op = (magic & 0x0F);
    /* set values */
    for ( uint32_t i = 0; i < length; i++ )
    {
        if ( mask_ptr[i] == 0 )
            goto next_line;
        if ( (dst_mv[0] == 0x80000000 || src_mv[0] == 0x80000000) && (op != mv_op_ass) )
            goto next_line;
        js_mv_op_kernel(dst_mv, src_mv[0], src_mv[1], magic);
next_line:
        src_mv += 2;
        dst_mv += 2;
    }
}

static no_inline void js_mvarray_maskop_add_mv       (int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mv_kernel     (dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_add | mv_op_h | mv_op_v); }
static no_inline void js_mvarray_maskop_sub_mv       (int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mv_kernel     (dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_sub | mv_op_h | mv_op_v); }
static no_inline void js_mvarray_maskop_mul_mv       (int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mv_kernel     (dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_mul | mv_op_h | mv_op_v); }
static no_inline void js_mvarray_maskop_div_mv       (int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mv_kernel     (dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_div | mv_op_h | mv_op_v); }
static no_inline void js_mvarray_maskop_ass_mv       (int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mv_kernel     (dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_ass | mv_op_h | mv_op_v); }
static no_inline void js_mvarray_maskop_add_mvarray  (int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mvarray_kernel(dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_add | mv_op_h | mv_op_v); }
static no_inline void js_mvarray_maskop_sub_mvarray  (int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mvarray_kernel(dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_sub | mv_op_h | mv_op_v); }
static no_inline void js_mvarray_maskop_mul_mvarray  (int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mvarray_kernel(dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_mul | mv_op_h | mv_op_v); }
static no_inline void js_mvarray_maskop_div_mvarray  (int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mvarray_kernel(dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_div | mv_op_h | mv_op_v); }
static no_inline void js_mvarray_maskop_ass_mvarray  (int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mvarray_kernel(dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_ass | mv_op_h | mv_op_v); }

static no_inline void js_mvarray_maskop_add_mv_h     (int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mv_kernel     (dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_add | mv_op_h); }
static no_inline void js_mvarray_maskop_sub_mv_h     (int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mv_kernel     (dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_sub | mv_op_h); }
static no_inline void js_mvarray_maskop_mul_mv_h     (int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mv_kernel     (dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_mul | mv_op_h); }
static no_inline void js_mvarray_maskop_div_mv_h     (int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mv_kernel     (dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_div | mv_op_h); }
static no_inline void js_mvarray_maskop_ass_mv_h     (int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mv_kernel     (dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_ass | mv_op_h); }
static no_inline void js_mvarray_maskop_add_mvarray_h(int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mvarray_kernel(dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_add | mv_op_h); }
static no_inline void js_mvarray_maskop_sub_mvarray_h(int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mvarray_kernel(dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_sub | mv_op_h); }
static no_inline void js_mvarray_maskop_mul_mvarray_h(int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mvarray_kernel(dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_mul | mv_op_h); }
static no_inline void js_mvarray_maskop_div_mvarray_h(int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mvarray_kernel(dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_div | mv_op_h); }
static no_inline void js_mvarray_maskop_ass_mvarray_h(int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mvarray_kernel(dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_ass | mv_op_h); }

static no_inline void js_mvarray_maskop_add_mv_v     (int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mv_kernel     (dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_add | mv_op_v); }
static no_inline void js_mvarray_maskop_sub_mv_v     (int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mv_kernel     (dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_sub | mv_op_v); }
static no_inline void js_mvarray_maskop_mul_mv_v     (int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mv_kernel     (dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_mul | mv_op_v); }
static no_inline void js_mvarray_maskop_div_mv_v     (int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mv_kernel     (dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_div | mv_op_v); }
static no_inline void js_mvarray_maskop_ass_mv_v     (int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mv_kernel     (dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_ass | mv_op_v); }
static no_inline void js_mvarray_maskop_add_mvarray_v(int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mvarray_kernel(dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_add | mv_op_v); }
static no_inline void js_mvarray_maskop_sub_mvarray_v(int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mvarray_kernel(dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_sub | mv_op_v); }
static no_inline void js_mvarray_maskop_mul_mvarray_v(int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mvarray_kernel(dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_mul | mv_op_v); }
static no_inline void js_mvarray_maskop_div_mvarray_v(int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mvarray_kernel(dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_div | mv_op_v); }
static no_inline void js_mvarray_maskop_ass_mvarray_v(int32_t *dst_ptr, int32_t *src_ptr, int64_t *mask_ptr, uint32_t dst_len) { js_mvarray_maskop_mvarray_kernel(dst_ptr, src_ptr, mask_ptr, dst_len, mv_op_ass | mv_op_v); }

static void js_mvarray_op_internal(
        int32_t *dst_ptr,
        uint32_t dst_len,
        int32_t *src_ptr,
        int64_t *mask_ptr,
        int is_src_mvarray,
        int magic)
{
    if ( is_src_mvarray )
    {
        /* set values */
        if ( mask_ptr != NULL )
        {
            switch ( magic )
            {
            case mv_op_add | mv_op_h | mv_op_v: js_mvarray_maskop_add_mvarray  (dst_ptr, src_ptr, mask_ptr, dst_len); break;
            case mv_op_sub | mv_op_h | mv_op_v: js_mvarray_maskop_sub_mvarray  (dst_ptr, src_ptr, mask_ptr, dst_len); break;
            case mv_op_mul | mv_op_h | mv_op_v: js_mvarray_maskop_mul_mvarray  (dst_ptr, src_ptr, mask_ptr, dst_len); break;
            case mv_op_div | mv_op_h | mv_op_v: js_mvarray_maskop_div_mvarray  (dst_ptr, src_ptr, mask_ptr, dst_len); break;
            case mv_op_ass | mv_op_h | mv_op_v: js_mvarray_maskop_ass_mvarray  (dst_ptr, src_ptr, mask_ptr, dst_len); break;
            case mv_op_add | mv_op_h:           js_mvarray_maskop_add_mvarray_h(dst_ptr, src_ptr, mask_ptr, dst_len); break;
            case mv_op_sub | mv_op_h:           js_mvarray_maskop_sub_mvarray_h(dst_ptr, src_ptr, mask_ptr, dst_len); break;
            case mv_op_mul | mv_op_h:           js_mvarray_maskop_mul_mvarray_h(dst_ptr, src_ptr, mask_ptr, dst_len); break;
            case mv_op_div | mv_op_h:           js_mvarray_maskop_div_mvarray_h(dst_ptr, src_ptr, mask_ptr, dst_len); break;
            case mv_op_ass | mv_op_h:           js_mvarray_maskop_ass_mvarray_h(dst_ptr, src_ptr, mask_ptr, dst_len); break;
            case mv_op_add | mv_op_v:           js_mvarray_maskop_add_mvarray_v(dst_ptr, src_ptr, mask_ptr, dst_len); break;
            case mv_op_sub | mv_op_v:           js_mvarray_maskop_sub_mvarray_v(dst_ptr, src_ptr, mask_ptr, dst_len); break;
            case mv_op_mul | mv_op_v:           js_mvarray_maskop_mul_mvarray_v(dst_ptr, src_ptr, mask_ptr, dst_len); break;
            case mv_op_div | mv_op_v:           js_mvarray_maskop_div_mvarray_v(dst_ptr, src_ptr, mask_ptr, dst_len); break;
            case mv_op_ass | mv_op_v:           js_mvarray_maskop_ass_mvarray_v(dst_ptr, src_ptr, mask_ptr, dst_len); break;
            }
        }
        else
        {
            switch ( magic )
            {
            case mv_op_add | mv_op_h | mv_op_v: js_mvarray_op_add_mvarray  (dst_ptr, src_ptr, dst_len); break;
            case mv_op_sub | mv_op_h | mv_op_v: js_mvarray_op_sub_mvarray  (dst_ptr, src_ptr, dst_len); break;
            case mv_op_mul | mv_op_h | mv_op_v: js_mvarray_op_mul_mvarray  (dst_ptr, src_ptr, dst_len); break;
            case mv_op_div | mv_op_h | mv_op_v: js_mvarray_op_div_mvarray  (dst_ptr, src_ptr, dst_len); break;
            case mv_op_ass | mv_op_h | mv_op_v: js_mvarray_op_ass_mvarray  (dst_ptr, src_ptr, dst_len); break;
            case mv_op_add | mv_op_h:           js_mvarray_op_add_mvarray_h(dst_ptr, src_ptr, dst_len); break;
            case mv_op_sub | mv_op_h:           js_mvarray_op_sub_mvarray_h(dst_ptr, src_ptr, dst_len); break;
            case mv_op_mul | mv_op_h:           js_mvarray_op_mul_mvarray_h(dst_ptr, src_ptr, dst_len); break;
            case mv_op_div | mv_op_h:           js_mvarray_op_div_mvarray_h(dst_ptr, src_ptr, dst_len); break;
            case mv_op_ass | mv_op_h:           js_mvarray_op_ass_mvarray_h(dst_ptr, src_ptr, dst_len); break;
            case mv_op_add | mv_op_v:           js_mvarray_op_add_mvarray_v(dst_ptr, src_ptr, dst_len); break;
            case mv_op_sub | mv_op_v:           js_mvarray_op_sub_mvarray_v(dst_ptr, src_ptr, dst_len); break;
            case mv_op_mul | mv_op_v:           js_mvarray_op_mul_mvarray_v(dst_ptr, src_ptr, dst_len); break;
            case mv_op_div | mv_op_v:           js_mvarray_op_div_mvarray_v(dst_ptr, src_ptr, dst_len); break;
            case mv_op_ass | mv_op_v:           js_mvarray_op_ass_mvarray_v(dst_ptr, src_ptr, dst_len); break;
            }
        }
    }
    else
    {
        /* set values */
        if ( mask_ptr != NULL )
        {
            switch ( magic )
            {
            case mv_op_add | mv_op_h | mv_op_v: js_mvarray_maskop_add_mv  (dst_ptr, src_ptr, mask_ptr, dst_len); break;
            case mv_op_sub | mv_op_h | mv_op_v: js_mvarray_maskop_sub_mv  (dst_ptr, src_ptr, mask_ptr, dst_len); break;
            case mv_op_mul | mv_op_h | mv_op_v: js_mvarray_maskop_mul_mv  (dst_ptr, src_ptr, mask_ptr, dst_len); break;
            case mv_op_div | mv_op_h | mv_op_v: js_mvarray_maskop_div_mv  (dst_ptr, src_ptr, mask_ptr, dst_len); break;
            case mv_op_ass | mv_op_h | mv_op_v: js_mvarray_maskop_ass_mv  (dst_ptr, src_ptr, mask_ptr, dst_len); break;
            case mv_op_add | mv_op_h:           js_mvarray_maskop_add_mv_h(dst_ptr, src_ptr, mask_ptr, dst_len); break;
            case mv_op_sub | mv_op_h:           js_mvarray_maskop_sub_mv_h(dst_ptr, src_ptr, mask_ptr, dst_len); break;
            case mv_op_mul | mv_op_h:           js_mvarray_maskop_mul_mv_h(dst_ptr, src_ptr, mask_ptr, dst_len); break;
            case mv_op_div | mv_op_h:           js_mvarray_maskop_div_mv_h(dst_ptr, src_ptr, mask_ptr, dst_len); break;
            case mv_op_ass | mv_op_h:           js_mvarray_maskop_ass_mv_h(dst_ptr, src_ptr, mask_ptr, dst_len); break;
            case mv_op_add | mv_op_v:           js_mvarray_maskop_add_mv_v(dst_ptr, src_ptr, mask_ptr, dst_len); break;
            case mv_op_sub | mv_op_v:           js_mvarray_maskop_sub_mv_v(dst_ptr, src_ptr, mask_ptr, dst_len); break;
            case mv_op_mul | mv_op_v:           js_mvarray_maskop_mul_mv_v(dst_ptr, src_ptr, mask_ptr, dst_len); break;
            case mv_op_div | mv_op_v:           js_mvarray_maskop_div_mv_v(dst_ptr, src_ptr, mask_ptr, dst_len); break;
            case mv_op_ass | mv_op_v:           js_mvarray_maskop_ass_mv_v(dst_ptr, src_ptr, mask_ptr, dst_len); break;
            }
        }
        else
        {
            switch ( magic )
            {
            case mv_op_add | mv_op_h | mv_op_v: js_mvarray_op_add_mv  (dst_ptr, src_ptr, dst_len); break;
            case mv_op_sub | mv_op_h | mv_op_v: js_mvarray_op_sub_mv  (dst_ptr, src_ptr, dst_len); break;
            case mv_op_mul | mv_op_h | mv_op_v: js_mvarray_op_mul_mv  (dst_ptr, src_ptr, dst_len); break;
            case mv_op_div | mv_op_h | mv_op_v: js_mvarray_op_div_mv  (dst_ptr, src_ptr, dst_len); break;
            case mv_op_ass | mv_op_h | mv_op_v: js_mvarray_op_ass_mv  (dst_ptr, src_ptr, dst_len); break;
            case mv_op_add | mv_op_h:           js_mvarray_op_add_mv_h(dst_ptr, src_ptr, dst_len); break;
            case mv_op_sub | mv_op_h:           js_mvarray_op_sub_mv_h(dst_ptr, src_ptr, dst_len); break;
            case mv_op_mul | mv_op_h:           js_mvarray_op_mul_mv_h(dst_ptr, src_ptr, dst_len); break;
            case mv_op_div | mv_op_h:           js_mvarray_op_div_mv_h(dst_ptr, src_ptr, dst_len); break;
            case mv_op_ass | mv_op_h:           js_mvarray_op_ass_mv_h(dst_ptr, src_ptr, dst_len); break;
            case mv_op_add | mv_op_v:           js_mvarray_op_add_mv_v(dst_ptr, src_ptr, dst_len); break;
            case mv_op_sub | mv_op_v:           js_mvarray_op_sub_mv_v(dst_ptr, src_ptr, dst_len); break;
            case mv_op_mul | mv_op_v:           js_mvarray_op_mul_mv_v(dst_ptr, src_ptr, dst_len); break;
            case mv_op_div | mv_op_v:           js_mvarray_op_div_mv_v(dst_ptr, src_ptr, dst_len); break;
            case mv_op_ass | mv_op_v:           js_mvarray_op_ass_mv_v(dst_ptr, src_ptr, dst_len); break;
            }
        }
    }
}

static JSValue js_mvarray_op(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv, int magic)
{
    JSObject *dst_p = JS_VALUE_GET_OBJ(this_val);
    int32_t *dst_ptr = dst_p->u.array.u.int32_ptr;
    uint32_t dst_len = dst_p->u.array.count;
    int32_t src_mv[2] = { 0, 0 };
    int32_t *src_ptr = NULL;
    uint32_t src_width = 0;
    int64_t *mask_ptr = NULL;
    uint32_t mask_width = 0;
    int is_src_mvarray;

    if ( js_mvXXarray_op_parse_args(ctx, argc, argv, src_mv,
                                    NULL, &src_ptr, NULL, &src_width,
                                    NULL, &mask_ptr, NULL, &mask_width,
                                    &magic) < 0 )
    {
        return JS_ThrowTypeError(ctx, "error parsing arguments");
    }
    if ( (src_width != 0 && src_width != dst_len)
      || (mask_width != 0 && mask_width != dst_len) )
    {
        return JS_ThrowRangeError(ctx, "dst/src/mask length mismatch");
    }
    is_src_mvarray = (src_width != 0);

    js_mvarray_op_internal(dst_ptr, dst_len, src_ptr, mask_ptr, is_src_mvarray, magic);

    return JS_DupValue(ctx, this_val);
}

static force_inline
void js_mvarray_cmp_sq_kernel(JSValueConst val, uint64_t *pcmp_sq, uint32_t *pcmp_sq_i, int magic)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    int32_t *ptr = p->u.array.u.int32_ptr;
    uint32_t len = p->u.array.count;
    uint64_t cmp_sq = *pcmp_sq;
    uint32_t cmp_sq_i = *pcmp_sq_i;
    for ( uint32_t i = 0; i < len; i++ )
    {
        int32_t mv0;
        int32_t mv1;
        uint64_t magnitude_sq;
        int ok;
        mv0 = ptr[(i << 1) + 0];
        if ( mv0 == 0x80000000 )
            continue;
        mv1 = ptr[(i << 1) + 1];
        magnitude_sq = (mv0 * mv0) + (mv1 * mv1);
        ok = (magic == 0)
           ? (magnitude_sq > cmp_sq)
           : (magnitude_sq < cmp_sq);
        if ( ok )
        {
            cmp_sq = magnitude_sq;
            cmp_sq_i = i;
        }
    }
    *pcmp_sq = cmp_sq;
    *pcmp_sq_i = cmp_sq_i;
}

static JSValue js_mvarray_cmp_sq(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv, int magic)
{
    uint64_t cmp_sq = (magic == 0) ? 0 : INT32_MAX;
    uint32_t cmp_sq_i = 0;
    JSValue val;
    int32_t *pout;

    if ( magic == 0 )
        js_mvarray_cmp_sq_kernel(this_val, &cmp_sq, &cmp_sq_i, 0);
    else
        js_mvarray_cmp_sq_kernel(this_val, &cmp_sq, &cmp_sq_i, 1);

    val = JS_NewInt32FFArray(ctx, &pout, 2, 0);
    if ( unlikely(JS_IsException(val)) )
        return val;
    pout[0] = cmp_sq_i;
    pout[1] = cmp_sq;

    return val;
}

static force_inline
void js_mvarray_swap_hv_kernel(JSContext *ctx, JSValueConst this_val)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    int32_t *ptr = p->u.array.u.int32_ptr;
    uint32_t length = p->u.array.count;

    for ( size_t i = 0; i < length; i++ )
    {
        js_mv_swap_hv_kernel(ptr);
        ptr += 2;
    }
}

static JSValue js_mvarray_swap_hv(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    js_mvarray_swap_hv_kernel(ctx, this_val);
    return JS_DupValue(ctx, this_val);
}

static force_inline
void js_mvarray_clear_kernel(JSContext *ctx, JSValueConst this_val)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    int32_t *ptr = p->u.array.u.int32_ptr;
    uint32_t length = p->u.array.count;
    memset(ptr, 0x00, length * 2 * sizeof(int32_t));
}

static JSValue js_mvarray_clear(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    js_mvarray_clear_kernel(ctx, this_val);
    return JS_DupValue(ctx, this_val);
}

static force_inline
void js_mv_compare_eq_op_kernel(int64_t *dst_ptr, int32_t *src0_ptr, int32_t mv0, int32_t mv1, int magic)
{
    const int op = (magic & 0x0F);
    const int is_eq = (op == mv_cmp_op__eq) || (op == mv_cmp_op_neq);
    const int do_h = (magic & mv_op_h);
    const int do_v = (magic & mv_op_v);
    int ok = 1;

    if ( is_eq && (do_h || do_v) )
    {
        if ( do_h )
            ok = ok && (src0_ptr[0] == mv0);
        if ( do_v )
            ok = ok && (src0_ptr[1] == mv1);
        if ( op == mv_cmp_op_neq )
            ok = !ok;
    }
    else
    {
        int64_t a;
        int64_t b;
        if ( do_h && do_v )
        {
            /* magnitude_sq */
            a = (src0_ptr[0] * src0_ptr[0]) + (src0_ptr[1] * src0_ptr[1]);
            b = (mv0 * mv0) + (mv1 * mv1);
        }
        else if ( do_h )
        {
            a = src0_ptr[0];
            b = mv0;
        }
        else if ( do_v )
        {
            a = src0_ptr[1];
            b = mv1;
        }
        else
        {
            /* mv0 is already magnitude_sq() */
            a = (src0_ptr[0] * src0_ptr[0]) + (src0_ptr[1] * src0_ptr[1]);
            b = mv0;
        }

        /* set values */
        switch ( op )
        {
        case mv_cmp_op__eq: ok = (a == b); break;
        case mv_cmp_op_neq: ok = (a != b); break;
        case mv_cmp_op__gt: ok = (a >  b); break;
        case mv_cmp_op_gte: ok = (a >= b); break;
        case mv_cmp_op__lt: ok = (a <  b); break;
        case mv_cmp_op_lte: ok = (a <= b); break;
        }
    }

    *dst_ptr = ok ? -1 : 0;
}

static force_inline
void js_mvarray_cmp_op_mv_kernel(int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length, int magic)
{
    const int op = (magic & 0x0F);
    const int is_eq = (op == mv_cmp_op__eq) || (op == mv_cmp_op_neq);
    int32_t mv0 = src1_ptr[0];
    int32_t mv1 = src1_ptr[1];
    /* null can only be compared for equality */
    if ( mv0 == 0x80000000 && !is_eq )
    {
        memset(dst_ptr, 0x00, length * sizeof(int64_t));
        return;
    }
    for ( uint32_t i = 0; i < length; i++ )
    {
        if ( src0_ptr[0] == 0x80000000 && !is_eq )
            *dst_ptr = 0;
        else
            js_mv_compare_eq_op_kernel(dst_ptr, src0_ptr, mv0, mv1, magic);
        dst_ptr += 1;
        src0_ptr += 2;
    }
}

static force_inline
void js_mvarray_cmp_op_mvarray_kernel(int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length, int magic)
{
    const int op = (magic & 0x0F);
    const int is_eq = (op == mv_cmp_op__eq) || (op == mv_cmp_op_neq);
    /* set values */
    for ( uint32_t i = 0; i < length; i++ )
    {
        if ( (src0_ptr[0] == 0x80000000 || src1_ptr[0] == 0x80000000) && !is_eq )
            *dst_ptr = 0;
        else
            js_mv_compare_eq_op_kernel(dst_ptr, src0_ptr, src1_ptr[0], src1_ptr[1], magic);
        dst_ptr += 1;
        src0_ptr += 2;
        src1_ptr += 2;
    }
}

static no_inline void js_mvarray_cmp_op__eq_mv       (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mv_kernel     (dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op__eq | mv_op_h | mv_op_v); }
static no_inline void js_mvarray_cmp_op_neq_mv       (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mv_kernel     (dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op_neq | mv_op_h | mv_op_v); }
static no_inline void js_mvarray_cmp_op__gt_mv       (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mv_kernel     (dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op__gt | mv_op_h | mv_op_v); }
static no_inline void js_mvarray_cmp_op_gte_mv       (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mv_kernel     (dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op_gte | mv_op_h | mv_op_v); }
static no_inline void js_mvarray_cmp_op__lt_mv       (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mv_kernel     (dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op__lt | mv_op_h | mv_op_v); }
static no_inline void js_mvarray_cmp_op_lte_mv       (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mv_kernel     (dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op_lte | mv_op_h | mv_op_v); }
static no_inline void js_mvarray_cmp_op__eq_mvarray  (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mvarray_kernel(dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op__eq | mv_op_h | mv_op_v); }
static no_inline void js_mvarray_cmp_op_neq_mvarray  (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mvarray_kernel(dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op_neq | mv_op_h | mv_op_v); }
static no_inline void js_mvarray_cmp_op__gt_mvarray  (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mvarray_kernel(dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op__gt | mv_op_h | mv_op_v); }
static no_inline void js_mvarray_cmp_op_gte_mvarray  (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mvarray_kernel(dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op_gte | mv_op_h | mv_op_v); }
static no_inline void js_mvarray_cmp_op__lt_mvarray  (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mvarray_kernel(dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op__lt | mv_op_h | mv_op_v); }
static no_inline void js_mvarray_cmp_op_lte_mvarray  (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mvarray_kernel(dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op_lte | mv_op_h | mv_op_v); }

static no_inline void js_mvarray_cmp_op__eq_mv_h     (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mv_kernel     (dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op__eq | mv_op_h); }
static no_inline void js_mvarray_cmp_op_neq_mv_h     (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mv_kernel     (dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op_neq | mv_op_h); }
static no_inline void js_mvarray_cmp_op__gt_mv_h     (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mv_kernel     (dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op__gt | mv_op_h); }
static no_inline void js_mvarray_cmp_op_gte_mv_h     (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mv_kernel     (dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op_gte | mv_op_h); }
static no_inline void js_mvarray_cmp_op__lt_mv_h     (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mv_kernel     (dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op__lt | mv_op_h); }
static no_inline void js_mvarray_cmp_op_lte_mv_h     (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mv_kernel     (dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op_lte | mv_op_h); }
static no_inline void js_mvarray_cmp_op__eq_mvarray_h(int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mvarray_kernel(dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op__eq | mv_op_h); }
static no_inline void js_mvarray_cmp_op_neq_mvarray_h(int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mvarray_kernel(dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op_neq | mv_op_h); }
static no_inline void js_mvarray_cmp_op__gt_mvarray_h(int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mvarray_kernel(dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op__gt | mv_op_h); }
static no_inline void js_mvarray_cmp_op_gte_mvarray_h(int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mvarray_kernel(dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op_gte | mv_op_h); }
static no_inline void js_mvarray_cmp_op__lt_mvarray_h(int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mvarray_kernel(dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op__lt | mv_op_h); }
static no_inline void js_mvarray_cmp_op_lte_mvarray_h(int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mvarray_kernel(dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op_lte | mv_op_h); }

static no_inline void js_mvarray_cmp_op__eq_mv_v     (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mv_kernel     (dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op__eq | mv_op_v); }
static no_inline void js_mvarray_cmp_op_neq_mv_v     (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mv_kernel     (dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op_neq | mv_op_v); }
static no_inline void js_mvarray_cmp_op__gt_mv_v     (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mv_kernel     (dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op__gt | mv_op_v); }
static no_inline void js_mvarray_cmp_op_gte_mv_v     (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mv_kernel     (dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op_gte | mv_op_v); }
static no_inline void js_mvarray_cmp_op__lt_mv_v     (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mv_kernel     (dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op__lt | mv_op_v); }
static no_inline void js_mvarray_cmp_op_lte_mv_v     (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mv_kernel     (dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op_lte | mv_op_v); }
static no_inline void js_mvarray_cmp_op__eq_mvarray_v(int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mvarray_kernel(dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op__eq | mv_op_v); }
static no_inline void js_mvarray_cmp_op_neq_mvarray_v(int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mvarray_kernel(dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op_neq | mv_op_v); }
static no_inline void js_mvarray_cmp_op__gt_mvarray_v(int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mvarray_kernel(dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op__gt | mv_op_v); }
static no_inline void js_mvarray_cmp_op_gte_mvarray_v(int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mvarray_kernel(dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op_gte | mv_op_v); }
static no_inline void js_mvarray_cmp_op__lt_mvarray_v(int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mvarray_kernel(dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op__lt | mv_op_v); }
static no_inline void js_mvarray_cmp_op_lte_mvarray_v(int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mvarray_kernel(dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op_lte | mv_op_v); }

static no_inline void js_mvarray_cmp_op__eq_mv_x     (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mv_kernel     (dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op__eq); }
static no_inline void js_mvarray_cmp_op_neq_mv_x     (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mv_kernel     (dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op_neq); }
static no_inline void js_mvarray_cmp_op__gt_mv_x     (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mv_kernel     (dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op__gt); }
static no_inline void js_mvarray_cmp_op_gte_mv_x     (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mv_kernel     (dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op_gte); }
static no_inline void js_mvarray_cmp_op__lt_mv_x     (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mv_kernel     (dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op__lt); }
static no_inline void js_mvarray_cmp_op_lte_mv_x     (int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mv_kernel     (dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op_lte); }
static no_inline void js_mvarray_cmp_op__eq_mvarray_x(int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mvarray_kernel(dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op__eq); }
static no_inline void js_mvarray_cmp_op_neq_mvarray_x(int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mvarray_kernel(dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op_neq); }
static no_inline void js_mvarray_cmp_op__gt_mvarray_x(int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mvarray_kernel(dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op__gt); }
static no_inline void js_mvarray_cmp_op_gte_mvarray_x(int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mvarray_kernel(dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op_gte); }
static no_inline void js_mvarray_cmp_op__lt_mvarray_x(int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mvarray_kernel(dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op__lt); }
static no_inline void js_mvarray_cmp_op_lte_mvarray_x(int64_t *dst_ptr, int32_t *src0_ptr, int32_t *src1_ptr, uint32_t length) { js_mvarray_cmp_op_mvarray_kernel(dst_ptr, src0_ptr, src1_ptr, length, mv_cmp_op_lte); }

static void js_mvarray_cmp_op_internal(
        int64_t *dst_ptr,
        int32_t *src0_ptr,
        int32_t *src1_ptr,
        uint32_t length,
        int is_src_mvarray,
        int magic)
{
    if ( is_src_mvarray )
    {
        switch ( magic )
        {
        case mv_cmp_op__eq | mv_op_h | mv_op_v: js_mvarray_cmp_op__eq_mvarray  (dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op_neq | mv_op_h | mv_op_v: js_mvarray_cmp_op_neq_mvarray  (dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op__gt | mv_op_h | mv_op_v: js_mvarray_cmp_op__gt_mvarray  (dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op_gte | mv_op_h | mv_op_v: js_mvarray_cmp_op_gte_mvarray  (dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op__lt | mv_op_h | mv_op_v: js_mvarray_cmp_op__lt_mvarray  (dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op_lte | mv_op_h | mv_op_v: js_mvarray_cmp_op_lte_mvarray  (dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op__eq | mv_op_h:           js_mvarray_cmp_op__eq_mvarray_h(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op_neq | mv_op_h:           js_mvarray_cmp_op_neq_mvarray_h(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op__gt | mv_op_h:           js_mvarray_cmp_op__gt_mvarray_h(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op_gte | mv_op_h:           js_mvarray_cmp_op_gte_mvarray_h(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op__lt | mv_op_h:           js_mvarray_cmp_op__lt_mvarray_h(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op_lte | mv_op_h:           js_mvarray_cmp_op_lte_mvarray_h(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op__eq | mv_op_v:           js_mvarray_cmp_op__eq_mvarray_v(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op_neq | mv_op_v:           js_mvarray_cmp_op_neq_mvarray_v(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op__gt | mv_op_v:           js_mvarray_cmp_op__gt_mvarray_v(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op_gte | mv_op_v:           js_mvarray_cmp_op_gte_mvarray_v(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op__lt | mv_op_v:           js_mvarray_cmp_op__lt_mvarray_v(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op_lte | mv_op_v:           js_mvarray_cmp_op_lte_mvarray_v(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op__eq:                     js_mvarray_cmp_op__eq_mvarray_x(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op_neq:                     js_mvarray_cmp_op_neq_mvarray_x(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op__gt:                     js_mvarray_cmp_op__gt_mvarray_x(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op_gte:                     js_mvarray_cmp_op_gte_mvarray_x(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op__lt:                     js_mvarray_cmp_op__lt_mvarray_x(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op_lte:                     js_mvarray_cmp_op_lte_mvarray_x(dst_ptr, src0_ptr, src1_ptr, length); break;
        }
    }
    else
    {
        switch ( magic )
        {
        case mv_cmp_op__eq | mv_op_h | mv_op_v: js_mvarray_cmp_op__eq_mv  (dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op_neq | mv_op_h | mv_op_v: js_mvarray_cmp_op_neq_mv  (dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op__gt | mv_op_h | mv_op_v: js_mvarray_cmp_op__gt_mv  (dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op_gte | mv_op_h | mv_op_v: js_mvarray_cmp_op_gte_mv  (dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op__lt | mv_op_h | mv_op_v: js_mvarray_cmp_op__lt_mv  (dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op_lte | mv_op_h | mv_op_v: js_mvarray_cmp_op_lte_mv  (dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op__eq | mv_op_h:           js_mvarray_cmp_op__eq_mv_h(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op_neq | mv_op_h:           js_mvarray_cmp_op_neq_mv_h(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op__gt | mv_op_h:           js_mvarray_cmp_op__gt_mv_h(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op_gte | mv_op_h:           js_mvarray_cmp_op_gte_mv_h(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op__lt | mv_op_h:           js_mvarray_cmp_op__lt_mv_h(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op_lte | mv_op_h:           js_mvarray_cmp_op_lte_mv_h(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op__eq | mv_op_v:           js_mvarray_cmp_op__eq_mv_v(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op_neq | mv_op_v:           js_mvarray_cmp_op_neq_mv_v(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op__gt | mv_op_v:           js_mvarray_cmp_op__gt_mv_v(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op_gte | mv_op_v:           js_mvarray_cmp_op_gte_mv_v(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op__lt | mv_op_v:           js_mvarray_cmp_op__lt_mv_v(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op_lte | mv_op_v:           js_mvarray_cmp_op_lte_mv_v(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op__eq:                     js_mvarray_cmp_op__eq_mv_x(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op_neq:                     js_mvarray_cmp_op_neq_mv_x(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op__gt:                     js_mvarray_cmp_op__gt_mv_x(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op_gte:                     js_mvarray_cmp_op_gte_mv_x(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op__lt:                     js_mvarray_cmp_op__lt_mv_x(dst_ptr, src0_ptr, src1_ptr, length); break;
        case mv_cmp_op_lte:                     js_mvarray_cmp_op_lte_mv_x(dst_ptr, src0_ptr, src1_ptr, length); break;
        }
    }
}

static JSValue js_mvarray_compare(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv, int magic)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    uint32_t len = p->u.array.count;
    JSValue ret = JS_UNDEFINED;
    int64_t *dst_ptr;

    ret = JS_NewMVMask(ctx, &dst_ptr, len, 0);
    if ( JS_IsException(ret) )
        return ret;

    /* (compareFn, thisArg) */
    if ( magic == -1 )
    {
        JSValue obj = JS_DupValue(ctx, this_val);
        JSValueConst this_arg = JS_UNDEFINED;
        JSValueConst func = argv[0];

        if ( argc > 1 )
            this_arg = argv[1];

        if ( check_function(ctx, func) )
            goto exception;

        for ( uint32_t i = 0; i < len; i++ )
        {
            JSValue mvref = JS_GetMVRef(ctx, p, i);
            JSValue val_i = JS_NewInt32(ctx, i);
            JSValue res;
            JSValueConst args[3];
            args[0] = mvref;
            args[1] = val_i;
            args[2] = obj;
            res = JS_Call(ctx, func, this_arg, 3, args);
            JS_FreeValue(ctx, mvref);
            if ( JS_IsException(res) )
                goto exception;
            dst_ptr[i] = JS_ToBoolFree(ctx, res) ? -1 : 0;
        }
        JS_FreeValue(ctx, obj);
        return ret;
exception:
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    else
    {
        int32_t *src0_ptr = p->u.array.u.int32_ptr; /* from this_val */
        int is_src_mvarray;

        /* parse arguments */
        int32_t src_mv[2] = { 0, 0 };
        int32_t *src1_ptr = NULL;
        uint32_t src1_width = 0;

        if ( js_mvXXarray_op_parse_args_src(ctx, argc, argv, src_mv,
                                            NULL, &src1_ptr, NULL, &src1_width,
                                            &magic) < 0 )
        {
            JS_FreeValue(ctx, ret);
            return JS_ThrowTypeError(ctx, "error parsing arguments");
        }

        if ( (src1_width != 0 && src1_width != len) )
        {
            JS_FreeValue(ctx, ret);
            return JS_ThrowRangeError(ctx, "dst/src length mismatch");
        }
        is_src_mvarray = (src1_width != 0);

        js_mvarray_cmp_op_internal(dst_ptr, src0_ptr, src1_ptr, len, is_src_mvarray, magic);
    }

    return ret;
}

static const JSCFunctionListEntry js_mvarray_proto_funcs[] = {
    JS_CFUNC_DEF("toString", 0, js_ffarray_toString ),
    JS_CFUNC_DEF("join", 1, js_ffarray_join ),
    JS_CFUNC_DEF("copyWithin", 1, js_ffarray_copyWithin ),
    JS_CFUNC_DEF("subarray", 2, js_ffarray_subarray ),
    JS_CFUNC_DEF("set", 2, js_ffarray_set ),
    JS_CFUNC_DEF("fill", 1, js_mvarray_fill ),
    JS_CFUNC_DEF("reverse", 0, js_ffarray_reverse ),
    JS_CFUNC_DEF("sort", 1, js_mvarray_sort ),
    JS_CFUNC_DEF("slice", 2, js_ffarray_slice ),
    JS_CFUNC_DEF("dup", 2, js_ffarray_dup ),
    JS_CFUNC_DEF("every", 1, js_mvarray_every ),
    JS_CFUNC_DEF("some", 1, js_mvarray_some ),
    JS_CFUNC_DEF("forEach", 1, js_mvarray_forEach ),
    JS_CFUNC_DEF("maskedForEach", 2, js_mvarray_maskedForEach ),
    JS_CFUNC_DEF("map", 1, js_mvarray_map ),
    JS_CFUNC_MAGIC_DEF("find", 1, js_ffarray_find, 0 ),
    JS_CFUNC_MAGIC_DEF("findIndex", 1, js_ffarray_find, 1 ),
    JS_CFUNC_MAGIC_DEF("indexOf", 1, js_mvarray_indexOf, special_indexOf ),
    JS_CFUNC_MAGIC_DEF("lastIndexOf", 1, js_mvarray_indexOf, special_lastIndexOf ),
    JS_CFUNC_MAGIC_DEF("includes", 1, js_mvarray_indexOf, special_includes ),
    JS_CFUNC_MAGIC_DEF("reduce", 1, js_array_reduce, special_reduce | special_MV ),
    JS_CFUNC_MAGIC_DEF("reduceRight", 1, js_array_reduce, special_reduceRight | special_MV ),
    JS_CFUNC_MAGIC_DEF("values", 0, js_create_array_iterator, JS_ITERATOR_KIND_VALUE ),
    JS_ALIAS_DEF("[Symbol.iterator]", "values" ),
    JS_CFUNC_MAGIC_DEF("keys", 0, js_create_array_iterator, JS_ITERATOR_KIND_KEY ),
    JS_CFUNC_MAGIC_DEF("entries", 0, js_create_array_iterator, JS_ITERATOR_KIND_KEY_AND_VALUE ),
    JS_CFUNC_MAGIC_DEF("largest_sq", 0, js_mvarray_cmp_sq, 0 ),
    JS_CFUNC_MAGIC_DEF("smallest_sq", 0, js_mvarray_cmp_sq, 1 ),
    JS_CFUNC_DEF("swap_hv", 0, js_mvarray_swap_hv ),
    JS_CFUNC_DEF("clear", 0, js_mvarray_clear ),
    JS_CFUNC_MAGIC_DEF("add", 3, js_mvarray_op, mv_op_add | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("sub", 3, js_mvarray_op, mv_op_sub | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("mul", 3, js_mvarray_op, mv_op_mul | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("div", 3, js_mvarray_op, mv_op_div | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("assign", 3, js_mvarray_op, mv_op_ass | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("add_h", 3, js_mvarray_op, mv_op_add | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("sub_h", 3, js_mvarray_op, mv_op_sub | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("mul_h", 3, js_mvarray_op, mv_op_mul | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("div_h", 3, js_mvarray_op, mv_op_div | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("assign_h", 3, js_mvarray_op, mv_op_ass | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("add_v", 3, js_mvarray_op, mv_op_add | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("sub_v", 3, js_mvarray_op, mv_op_sub | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("mul_v", 3, js_mvarray_op, mv_op_mul | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("div_v", 3, js_mvarray_op, mv_op_div | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("assign_v", 3, js_mvarray_op, mv_op_ass | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("compare", 1, js_mvarray_compare, -1 ),
    JS_CFUNC_MAGIC_DEF("compare_eq", 1, js_mvarray_compare, mv_cmp_op__eq | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("compare_neq", 1, js_mvarray_compare, mv_cmp_op_neq | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("compare_gt", 1, js_mvarray_compare, mv_cmp_op__gt | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("compare_gte", 1, js_mvarray_compare, mv_cmp_op_gte | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("compare_lt", 1, js_mvarray_compare, mv_cmp_op__lt | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("compare_lte", 1, js_mvarray_compare, mv_cmp_op_lte | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("compare_eq_h", 1, js_mvarray_compare, mv_cmp_op__eq | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("compare_neq_h", 1, js_mvarray_compare, mv_cmp_op_neq | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("compare_gt_h", 1, js_mvarray_compare, mv_cmp_op__gt | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("compare_gte_h", 1, js_mvarray_compare, mv_cmp_op_gte | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("compare_lt_h", 1, js_mvarray_compare, mv_cmp_op__lt | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("compare_lte_h", 1, js_mvarray_compare, mv_cmp_op_lte | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("compare_eq_v", 1, js_mvarray_compare, mv_cmp_op__eq | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("compare_neq_v", 1, js_mvarray_compare, mv_cmp_op_neq | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("compare_gt_v", 1, js_mvarray_compare, mv_cmp_op__gt | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("compare_gte_v", 1, js_mvarray_compare, mv_cmp_op_gte | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("compare_lt_v", 1, js_mvarray_compare, mv_cmp_op__lt | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("compare_lte_v", 1, js_mvarray_compare, mv_cmp_op_lte | mv_op_v ),
};

/*********************************************************************/
/* MVPtr *************************************************************/
/*********************************************************************/

static void js_mvptr_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    if ( p->u.array.u1.cached_mvref != NULL )
    {
        JS_FreeValueRT(rt, *p->u.array.u1.cached_mvref);
        js_free_rt(rt, p->u.array.u1.cached_mvref);
    }
}

static void js_mvptr_mark(JSRuntime *rt, JSValueConst val,
                              JS_MarkFunc *mark_func)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    if ( p->u.array.u1.cached_mvref != NULL )
        JS_MarkValue(rt, *p->u.array.u1.cached_mvref, mark_func);
}

static JSValue js_mvptr_constructor(JSContext *ctx,
                                    JSValueConst new_target,
                                    int argc, JSValueConst *argv)
{
    JSObject *p;
    JSValue val;
    p = JS_IsObjectClass(argv[0], JS_CLASS_MVARRAY);
    if ( p == NULL )
        p = JS_IsObjectClass(argv[0], JS_CLASS_MVPTR);
    if ( p == NULL )
        return JS_ThrowTypeError(ctx, "MVPtr() takes either an MVArray or an MVPtr as argument");
    if ( likely(JS_IsUndefined(new_target)) )
        val = JS_NewObjectFromShape(ctx, js_dup_shape(ctx->mvptr_shape), JS_CLASS_MVPTR);
    else
        val = js_create_from_ctor(ctx, new_target, JS_CLASS_MVPTR);
    if ( unlikely(JS_IsException(val)) )
        return val;
    return JS_InitFFPtr(ctx, val, p->u.array.u.ptr, p->u.array.count);
}

JSValue JS_NewMVPtr(JSContext *ctx, int32_t *pint32, uint32_t len)
{
    return JS_NewFFPtr(ctx, ctx->mvptr_shape, (void *) pint32, len, JS_CLASS_MVPTR);
}

int JS_IsMVPtr(JSValueConst val)
{
    return JS_IsObjectClass(val, JS_CLASS_MVPTR) != NULL;
}

int JS_GetMVPtr(JSValueConst val, int32_t **pint32, uint32_t *plen)
{
    return JS_GetFFArray(val, (void **) pint32, plen, JS_CLASS_MVPTR);
}

/*********************************************************************/
/* MVMask ************************************************************/
/*********************************************************************/

static void js_mvmask_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    if ( p->u.array.u1.size != 0 )
        js_free_rt(rt, p->u.array.u.ptr);
}

static JSValue js_mvmask_constructor(JSContext *ctx,
                                     JSValueConst new_target,
                                     int argc, JSValueConst *argv)
{
    int64_t *ptr64;
    uint64_t len;
    JSValue val;
    JSObject *p;
    if ( JS_ToIndex(ctx, &len, argv[0]) )
        return JS_ThrowTypeError(ctx, "MVMask() takes a positive length as argument");
    if ( likely(JS_IsUndefined(new_target)) )
        val = JS_NewObjectFromShape(ctx, js_dup_shape(ctx->mvmask_shape), JS_CLASS_MVMASK);
    else
        val = js_create_from_ctor(ctx, new_target, JS_CLASS_MVMASK);
    if ( unlikely(JS_IsException(val)) )
        return val;
    val = JS_InitFFArray(ctx, val, (void *) &ptr64, len, -1, JS_CLASS_MVMASK);
    p = JS_VALUE_GET_OBJ(val);
    p->u.array.u1.size = 1; /* HACK for js_mvmask_finalizer() */
    return val;
}

JSValue JS_NewMVMask(JSContext *ctx, int64_t **pint64, uint32_t len, int set_zero)
{
    JSValue val = JS_NewObjectFromShape(ctx, js_dup_shape(ctx->mvmask_shape), JS_CLASS_MVMASK);
    JSObject *p;
    if ( unlikely(JS_IsException(val)) )
        return val;
    val = JS_InitFFArray(ctx, val, (void *) pint64, len, set_zero, JS_CLASS_MVMASK);
    p = JS_VALUE_GET_OBJ(val);
    p->u.array.u1.size = 1; /* HACK for js_mvmask_finalizer() */
    return val;
}

JSValue JS_NewMVMaskPtr(JSContext *ctx, int64_t *pint64, uint32_t len)
{
    return JS_NewFFPtr(ctx, ctx->mvmask_shape, (void *) pint64, len, JS_CLASS_MVMASK);
}

static JSValue js_mvmask_fill(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    int64_t *ptr = p->u.array.u.int64_ptr;
    int32_t idx = 0;
    int32_t end = p->u.array.count;
    int32_t length;
    int val = JS_ToBool(ctx, argv[0]);

    argc--;
    argv++;
    /* optional range arguments */
    if ( js_parse_range(ctx, &idx, &end, &length, argc, argv) < 0 )
        return JS_ThrowRangeError(ctx, "error parsing range arguments");

    if ( idx < 0 || length <= 0 || end > p->u.array.count )
        return JS_ThrowRangeError(ctx, "out-of-bound access");

    if ( val < 0 )
        return JS_ThrowTypeError(ctx, "MVMask.fill() takes a boolean as argument");

    if ( val == 0 )
        memset(ptr + idx, 0, length * sizeof(int64_t));
    else
        memset(ptr + idx, 0xFF, length * sizeof(int64_t));

    return JS_DupValue(ctx, this_val);
}

static JSValue js_mvmask_forEach(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    return js_mvarray_every_internal(ctx, this_val, argc, argv, special_forEach | special_MVMASK);
}

static JSValue js_mvmask_not(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    int64_t *ptr = p->u.array.u.int64_ptr;
    int32_t idx = 0;
    int32_t end = p->u.array.count;
    int32_t length;

    /* optional range arguments */
    if ( js_parse_range(ctx, &idx, &end, &length, argc, argv) < 0 )
        return JS_ThrowRangeError(ctx, "error parsing range arguments");

    if ( idx < 0 || length <= 0 || end > p->u.array.count )
        return JS_ThrowRangeError(ctx, "out-of-bound access");

    for ( uint32_t i = 0; i < length; i++ )
        ptr[idx + i] ^= -1;

    return JS_DupValue(ctx, this_val);
}

static JSValue js_mvmask_op(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv, int magic)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    int64_t *ptr = p->u.array.u.int64_ptr;
    uint32_t length = p->u.array.count;
    JSObject *mask_p = JS_IsObjectClass(argv[0], JS_CLASS_MVMASK);
    int64_t *mask_ptr;
    uint32_t mask_length;

    if ( mask_p == NULL )
        return JS_ThrowTypeError(ctx, "MVMask operations takes an MVMask as argument");
    mask_ptr = mask_p->u.array.u.int64_ptr;
    mask_length = mask_p->u.array.count;

    if ( mask_length != length )
        return JS_ThrowRangeError(ctx, "MVMask length mismatch");

    switch ( magic )
    {
    case 0: for ( uint32_t i = 0; i < length; i++ ) ptr[i] &= mask_ptr[i]; break;
    case 1: for ( uint32_t i = 0; i < length; i++ ) ptr[i] |= mask_ptr[i]; break;
    case 2: for ( uint32_t i = 0; i < length; i++ ) ptr[i] ^= mask_ptr[i]; break;
    }

    return JS_DupValue(ctx, this_val);
}

static const JSCFunctionListEntry js_mvmask_proto_funcs[] = {
    JS_CFUNC_DEF("toString", 0, js_ffarray_toString ),
    JS_CFUNC_DEF("fill", 1, js_mvmask_fill ),
    JS_CFUNC_DEF("forEach", 1, js_mvmask_forEach ),
    JS_CFUNC_DEF("not", 0, js_mvmask_not ),
    JS_CFUNC_MAGIC_DEF("and", 1, js_mvmask_op, 0 ),
    JS_CFUNC_MAGIC_DEF("or", 1, js_mvmask_op, 1 ),
    JS_CFUNC_MAGIC_DEF("xor", 1, js_mvmask_op, 2 ),
};

/*********************************************************************/
/* MV2DArray *********************************************************/
/*********************************************************************/

static void js_mv2darray_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    js_array_finalizer(rt, val);
    if ( p->u.array.u1.mv2darray->u.ptr != NULL )
        js_free_rt(rt, p->u.array.u1.mv2darray->u.ptr);
    js_free_rt(rt, p->u.array.u1.mv2darray);
}

static inline JSValue JS_InitMV2D(JSContext *ctx, JSValue val, void **pptr, uint32_t width, uint32_t height, int set_zero, int class_id)
{
    int size_log2 = ffarray_or_ffptr_size_log2(class_id);
    struct JSMV2DArray *mv2d;
    void *ptr;
    JSValue *new_array_prop;
    JSObject *p;
    uint32_t length = width * height;

    /* allocate mv2darray values */
    mv2d = js_malloc_rt(ctx->rt, sizeof(struct JSMV2DArray));
    if ( unlikely(!mv2d) )
    {
        JS_FreeValue(ctx, val);
        return JS_EXCEPTION;
    }
    ptr = js_malloc_rt(ctx->rt, length << size_log2);
    if ( unlikely(!ptr) )
    {
        js_free_rt(ctx->rt, mv2d);
        JS_FreeValue(ctx, val);
        return JS_EXCEPTION;
    }
    if ( set_zero == 1 )
    {
        memset(ptr, 0, length << size_log2);
    }
    else if ( set_zero == -1 )
    {
        if ( class_id == JS_CLASS_MV2DARRAY )
        {
            int32_t *ptr32 = (int32_t *) ptr;
            for ( size_t i = 0; i < 2 * length; i++ )
                ptr32[i] = 0x80000000;
        }
        else /* JS_CLASS_MV2DMASK */
        {
            memset(ptr, 0xFF, length * sizeof(int64_t));
        }
    }
    mv2d->u.ptr = ptr;
    mv2d->width = width;
    mv2d->height = height;

    /* allocate JSValue array */
    new_array_prop = js_malloc_rt(ctx->rt, height * sizeof(JSValue));
    if ( unlikely(!new_array_prop) )
    {
        js_free_rt(ctx->rt, ptr);
        js_free_rt(ctx->rt, mv2d);
        JS_FreeValue(ctx, val);
        return JS_EXCEPTION;
    }
    if ( class_id == JS_CLASS_MV2DARRAY )
    {
        int32_t *ptr32 = (int32_t *) ptr;
        for ( size_t i = 0; i < height; i++ )
            new_array_prop[i] = JS_NewMVPtr(ctx, &ptr32[(i * width) << 1], width);
    }
    else /* JS_CLASS_MV2DMASK */
    {
        int64_t *ptr64 = (int64_t *) ptr;
        for ( size_t i = 0; i < height; i++ )
            new_array_prop[i] = JS_NewMVMaskPtr(ctx, &ptr64[i * width], width);
    }

    p = JS_VALUE_GET_OBJ(val);
    p->prop[0].u.value = JS_NewInt32(ctx, height); /* length of JSValue array */
    p->prop[1].u.value = JS_NewInt32(ctx, width);
    p->prop[2].u.value = JS_NewInt32(ctx, height);
    p->u.array.u.values = new_array_prop;
    p->u.array.u1.mv2darray = mv2d;
    p->u.array.count = height;

    *pptr = ptr;

    return val;
}

static JSValue js_mv2darray_constructor(JSContext *ctx,
                                        JSValueConst new_target,
                                        int argc, JSValueConst *argv)
{
    uint64_t width;
    uint64_t height;
    JSValue val;
    int32_t *ptr32;

    /* check arguments */
    if ( JS_ToIndex(ctx, &width, argv[0]) || width <= 0 || width >= 0x10000
      || JS_ToIndex(ctx, &height, argv[1]) || height <= 0 || height >= 0x10000 )
    {
        return JS_ThrowTypeError(ctx, "error parsing width/height arguments");
    }

    /* create new mv2darray */
    if ( likely(JS_IsUndefined(new_target)) )
        val = JS_NewObjectFromShape(ctx, js_dup_shape(ctx->mv2darray_shape), JS_CLASS_MV2DARRAY);
    else
        val = js_create_from_ctor(ctx, new_target, JS_CLASS_MV2DARRAY);
    if ( unlikely(JS_IsException(val)) )
        return val;

    return JS_InitMV2D(ctx, val, (void **) &ptr32, width, height, 1, JS_CLASS_MV2DARRAY);
}

JSValue JS_NewMV2DArray(JSContext *ctx, int32_t **pint32, uint32_t width, uint32_t height, int set_zero)
{
    JSValue val;

    /* create new mv2darray */
    val = JS_NewObjectFromShape(ctx, js_dup_shape(ctx->mv2darray_shape), JS_CLASS_MV2DARRAY);
    if ( unlikely(JS_IsException(val)) )
        return val;

    return JS_InitMV2D(ctx, val, (void **) pint32, width, height, set_zero, JS_CLASS_MV2DARRAY);
}

int JS_IsMV2DArray(JSValueConst val)
{
    return JS_IsObjectClass(val, JS_CLASS_MV2DARRAY) != NULL;
}

int JS_GetMV2DArray(JSValueConst val, JSValue **pvalues, uint32_t *pwidth, uint32_t *pheight)
{
    JSObject *p = JS_IsObjectClass(val, JS_CLASS_MV2DARRAY);
    if ( p != NULL )
    {
        struct JSMV2DArray *mv2d = p->u.array.u1.mv2darray;
        *pvalues = p->u.array.u.values;
        *pwidth = mv2d->width;
        *pheight = mv2d->height;
        return TRUE;
    }
    return FALSE;
}

static force_inline
JSValue js_mv2d_toString(JSContext *ctx, JSValueConst this_val, int class_id)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    JSValue *values = p->u.array.u.values;
    uint32_t count = p->u.array.count;
    StringBuffer b_s, *b = &b_s;

    string_buffer_init(ctx, b, 0);

    string_buffer_putc8(b, '[');
    string_buffer_putc8(b, '\n');
    for ( size_t i = 0; i < count; i++ )
    {
        JSObject *pline = JS_VALUE_GET_OBJ(values[i]);
        uint32_t len = pline->u.array.count;
        if ( i != 0 )
        {
            string_buffer_putc8(b, ',');
            string_buffer_putc8(b, '\n');
        }
        string_buffer_putc8(b, ' ');
        if ( class_id == JS_CLASS_MV2DARRAY )
        {
            int32_t *ptr32 = pline->u.array.u.int32_ptr;
            print_mvarray(b, ptr32, len, ',', NULL);
        }
        else /* JS_CLASS_MV2DMASK */
        {
            int64_t *ptr64 = pline->u.array.u.int64_ptr;
            string_buffer_putc8(b, '[');
            for ( size_t j = 0; j < len; j++ )
            {
                if ( j != 0 )
                    string_buffer_putc8(b, ',');
                if ( ptr64[j] == 0 )
                    string_buffer_puts8(b, "false");
                else
                    string_buffer_puts8(b, "true");
            }
            string_buffer_putc8(b, ']');
        }
    }
    string_buffer_putc8(b, '\n');
    string_buffer_putc8(b, ']');

    return string_buffer_end(b);
}

static JSValue js_mv2darray_toString(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv)
{
    return js_mv2d_toString(ctx, this_val, JS_CLASS_MV2DARRAY);
}

static inline
int js_mv2darray_parse_range(JSContext *ctx,
                             int32_t *pi_idx, int32_t *pi_end, int32_t *pi_length,
                             int32_t *pj_idx, int32_t *pj_end, int32_t *pj_length,
                             int argc, JSValueConst *argv)
{
    JSValue el;

    if ( argc <= 0 )
        goto the_end;

    if ( JS_IsArray(ctx, argv[0]) <= 0 )
        return -1;

    el = JS_GetPropertyUint32(ctx, argv[0], 0);
    if ( JS_IsException(el) || JS_ToInt32Clamp(ctx, pj_idx, el, 0, *pj_end, *pj_end) < 0 )
        return -1;
    el = JS_GetPropertyUint32(ctx, argv[0], 1);
    if ( JS_IsException(el) || JS_ToInt32Clamp(ctx, pi_idx, el, 0, *pi_end, *pi_end) < 0 )
        return -1;

    if ( argc <= 1 )
        goto the_end;

    el = JS_GetPropertyUint32(ctx, argv[1], 0);
    if ( JS_IsException(el) || JS_ToInt32Clamp(ctx, pj_end, el, 0, *pj_end, *pj_end) < 0 )
        return -1;
    el = JS_GetPropertyUint32(ctx, argv[1], 1);
    if ( JS_IsException(el) || JS_ToInt32Clamp(ctx, pi_end, el, 0, *pi_end, *pi_end) < 0 )
        return -1;

the_end:
    *pi_length = *pi_end - *pi_idx;
    *pj_length = *pj_end - *pj_idx;

    return 0;
}

static JSValue js_mv2darray_fill(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    struct JSMV2DArray *mv2d = p->u.array.u1.mv2darray;
    JSValue *values = p->u.array.u.values;
    uint32_t width = mv2d->width;
    uint32_t height = mv2d->height;
    int32_t i_idx = 0, i_end = height, i_length;
    int32_t j_idx = 0, j_end = width, j_length;
    int32_t mv[2];

    /* nop if no arguments given */
    if ( argc == 0 )
        return JS_DupValue(ctx, this_val);

    if ( mv_to_int32ptr(mv, argv[0]) < 0 )
        return JS_ThrowTypeError(ctx, "error parsing motion vector argument");

    argc--;
    argv++;
    /* optional range arguments */
    if ( js_mv2darray_parse_range(ctx, &i_idx, &i_end, &i_length, &j_idx, &j_end, &j_length, argc, argv) < 0 )
        return JS_ThrowTypeError(ctx, "error parsing range arguments");

    if ( i_idx < 0 || i_length <= 0 || i_end > height
      || j_idx < 0 || j_length <= 0 || j_end > width )
    {
        return JS_ThrowRangeError(ctx, "out-of-bound access");
    }

    /* fill data */
    for ( size_t i = 0; i < i_length; i++ )
    {
        JSObject *pline = JS_VALUE_GET_OBJ(values[i_idx + i]);
        int32_t *dst_ptr = pline->u.array.u.int32_ptr;
        js_mvarray_fill_internal(dst_ptr, mv, j_idx, j_length);
    }

    return JS_DupValue(ctx, this_val);
}

static int js_mv2darray_reverse_h_internal(JSContext *ctx, JSValueConst this_val)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    JSValue *values = p->u.array.u.values;
    uint32_t count = p->u.array.count;

    /* reverse rows */
    for ( uint32_t i = 0; i < count; i++ )
    {
        int ret = js_typed_array_reverse_internal(ctx, values[i], 0);
        if ( unlikely(ret < 0) )
            return ret;
    }

    return 0;
}

static int js_mv2darray_reverse_v_internal(JSContext *ctx, JSValueConst this_val)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    struct JSMV2DArray *mv2d = p->u.array.u1.mv2darray;
    JSValue *values = p->u.array.u.values;
    uint32_t count = p->u.array.count;
    uint32_t width = mv2d->width;
    uint32_t stride = width * 2 * sizeof(int32_t);
    int32_t *tmp = js_malloc_rt(ctx->rt, stride);
    JSValue *v1 = values;
    JSValue *v2 = v1 + count - 1;

    /* reverse columns */
    while ( v1 < v2 )
    {
        JSObject *p1 = JS_VALUE_GET_OBJ(*v1++);
        int32_t *ptr1 = p1->u.array.u.int32_ptr;
        JSObject *p2 = JS_VALUE_GET_OBJ(*v2--);
        int32_t *ptr2 = p2->u.array.u.int32_ptr;
        memcpy(tmp, ptr1, stride);
        memcpy(ptr1, ptr2, stride);
        memcpy(ptr2, tmp, stride);
    }

    js_free_rt(ctx->rt, tmp);

    return 0;
}

static JSValue js_mv2darray_reverse(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv)
{
    if ( js_mv2darray_reverse_v_internal(ctx, this_val) < 0 )
        return JS_EXCEPTION;
    if ( js_mv2darray_reverse_h_internal(ctx, this_val) < 0 )
        return JS_EXCEPTION;
    return JS_DupValue(ctx, this_val);
}

static JSValue js_mv2darray_reverse_h(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv)
{
    if ( js_mv2darray_reverse_h_internal(ctx, this_val) < 0 )
        return JS_EXCEPTION;
    return JS_DupValue(ctx, this_val);
}

static JSValue js_mv2darray_reverse_v(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv)
{
    if ( js_mv2darray_reverse_v_internal(ctx, this_val) < 0 )
        return JS_EXCEPTION;
    return JS_DupValue(ctx, this_val);
}

static JSValue js_mv2darray_slice(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    struct JSMV2DArray *mv2d = p->u.array.u1.mv2darray;
    JSValue *values = p->u.array.u.values;
    uint32_t width = mv2d->width;
    uint32_t height = mv2d->height;
    int32_t i_idx = 0, i_end = height, i_length;
    int32_t j_idx = 0, j_end = width, j_length;
    JSValue dst_array;
    int32_t *new_ptr;

    /* optional range arguments */
    if ( js_mv2darray_parse_range(ctx, &i_idx, &i_end, &i_length, &j_idx, &j_end, &j_length, argc, argv) < 0 )
        return JS_ThrowTypeError(ctx, "error parsing range arguments");

    if ( i_idx < 0 || i_length <= 0 || i_end > height
      || j_idx < 0 || j_length <= 0 || j_end > width )
    {
        return JS_ThrowRangeError(ctx, "out-of-bound access");
    }

    /* create new mv2darray */
    dst_array = JS_NewMV2DArray(ctx, &new_ptr, j_length, i_length, 0);
    if ( unlikely(JS_IsException(dst_array)) )
        return dst_array;

    /* copy data */
    for ( size_t i = 0; i < i_length; i++ )
    {
        JSObject *pline = JS_VALUE_GET_OBJ(values[i_idx + i]);
        int32_t *dst_ptr = &new_ptr[(i * j_length) << 1];
        int32_t *src_ptr = &pline->u.array.u.int32_ptr[j_idx << 1];
        memcpy(dst_ptr, src_ptr, (j_length * sizeof(int32_t)) << 1);
    }

    return dst_array;
}

static JSValue js_mv2darray_dup(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    return js_mv2darray_slice(ctx, this_val, 0, NULL);
}

static inline JSValue JS_InitMV2DPtr(JSContext *ctx, JSValue val, JSValue **pptr, uint32_t width, uint32_t height)
{
    struct JSMV2DArray *mv2d;
    JSValue *new_array_prop;
    JSObject *p;

    /* allocate mv2darray values */
    mv2d = js_malloc_rt(ctx->rt, sizeof(struct JSMV2DArray));
    if ( unlikely(!mv2d) )
    {
        JS_FreeValue(ctx, val);
        return JS_EXCEPTION;
    }
    mv2d->u.ptr = NULL;
    mv2d->width = width;
    mv2d->height = height;

    /* allocate JSValue array */
    new_array_prop = js_malloc_rt(ctx->rt, height * sizeof(JSValue));
    if ( unlikely(!new_array_prop) )
    {
        js_free_rt(ctx->rt, mv2d);
        JS_FreeValue(ctx, val);
        return JS_EXCEPTION;
    }
    for ( size_t i = 0; i < height; i++ )
        new_array_prop[i] = JS_NewMVPtr(ctx, NULL, width);

    p = JS_VALUE_GET_OBJ(val);
    p->prop[0].u.value = JS_NewInt32(ctx, height); /* length of JSValue array */
    p->prop[1].u.value = JS_NewInt32(ctx, width);
    p->prop[2].u.value = JS_NewInt32(ctx, height);
    p->u.array.u.values = new_array_prop;
    p->u.array.u1.mv2darray = mv2d;
    p->u.array.count = height;

    *pptr = new_array_prop;

    return val;
}

static JSValue JS_NewMV2DPtr(JSContext *ctx, JSValueConst new_target, JSValue **pptr, uint32_t width, uint32_t height)
{
    JSValue val;

    /* create new mv2dptr */
    if ( likely(JS_IsUndefined(new_target)) )
        val = JS_NewObjectFromShape(ctx, js_dup_shape(ctx->mv2dptr_shape), JS_CLASS_MV2DPTR);
    else
        val = js_create_from_ctor(ctx, new_target, JS_CLASS_MV2DPTR);
    if ( unlikely(JS_IsException(val)) )
        return val;

    return JS_InitMV2DPtr(ctx, val, pptr, width, height);
}

static JSValue js_mv2darray_subarray(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    struct JSMV2DArray *mv2d = p->u.array.u1.mv2darray;
    JSValue *src_values = p->u.array.u.values;
    uint32_t width = mv2d->width;
    uint32_t height = mv2d->height;
    int32_t i_idx = 0, i_end = height, i_length;
    int32_t j_idx = 0, j_end = width, j_length;
    JSValue dst_array;
    JSValue *dst_values;

    /* optional range arguments */
    if ( js_mv2darray_parse_range(ctx, &i_idx, &i_end, &i_length, &j_idx, &j_end, &j_length, argc, argv) < 0 )
        return JS_ThrowTypeError(ctx, "error parsing range arguments");

    if ( i_idx < 0 || i_length <= 0 || i_end > height
      || j_idx < 0 || j_length <= 0 || j_end > width )
    {
        return JS_ThrowRangeError(ctx, "out-of-bound access");
    }

    /* create new mv2dptr */
    dst_array = JS_NewMV2DPtr(ctx, JS_UNDEFINED, &dst_values, j_length, i_length);
    if ( unlikely(JS_IsException(dst_array)) )
        return dst_array;

    /* set pointers */
    for ( size_t i = 0; i < i_length; i++ )
    {
        JSObject *src_line = JS_VALUE_GET_OBJ(src_values[i_idx + i]);
        JSObject *dst_line = JS_VALUE_GET_OBJ(dst_values[i]);
        dst_line->u.array.u.int32_ptr = &src_line->u.array.u.int32_ptr[j_idx << 1];
    }

    return dst_array;
}

static JSValue js_mv2darray_op(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv, int magic)
{
    JSObject *dst_p = JS_VALUE_GET_OBJ(this_val);
    struct JSMV2DArray *mv2d = dst_p->u.array.u1.mv2darray;
    JSValue *dst_values = dst_p->u.array.u.values;
    uint32_t dst_height = mv2d->height;
    uint32_t dst_width = mv2d->width;
    int32_t src_mv[2] = { 0, 0 };
    JSValue *src_values = NULL;
    int32_t *src_ptr = NULL;
    uint32_t src_height = 0;
    uint32_t src_width = 0;
    JSValue *mask_values = NULL;
    int64_t *mask_ptr = NULL;
    uint32_t mask_height = 0;
    uint32_t mask_width = 0;
    int is_src_mvarray;

    if ( js_mvXXarray_op_parse_args(ctx, argc, argv, src_mv,
                                    &src_values, &src_ptr, &src_height, &src_width,
                                    &mask_values, &mask_ptr, &mask_height, &mask_width,
                                    &magic) < 0 )
    {
        return JS_ThrowTypeError(ctx, "error parsing arguments");
    }

    if ( (src_height != 0 && src_height != dst_height)
      || (mask_height != 0 && mask_height != dst_height) )
    {
        return JS_ThrowRangeError(ctx, "dst/src/mask height mismatch");
    }
    if ( (src_width != 0 && src_width != dst_width)
      || (mask_width != 0 && mask_width != dst_width) )
    {
        return JS_ThrowRangeError(ctx, "dst/src/mask width mismatch");
    }
    is_src_mvarray = (src_width != 0);

    for ( uint32_t i = 0; i < dst_height; i++ )
    {
        JSObject *dst_p = JS_VALUE_GET_OBJ(dst_values[i]);
        int32_t *dst_ptr = dst_p->u.array.u.int32_ptr;
        if ( src_values != NULL )
        {
            JSObject *src_p = JS_VALUE_GET_OBJ(src_values[i]);
            src_ptr = src_p->u.array.u.int32_ptr;
        }
        if ( mask_values != NULL )
        {
            JSObject *mask_p = JS_VALUE_GET_OBJ(mask_values[i]);
            mask_ptr = mask_p->u.array.u.int64_ptr;
        }
        js_mvarray_op_internal(dst_ptr, dst_width, src_ptr, mask_ptr, is_src_mvarray, magic);
    }

    return JS_DupValue(ctx, this_val);
}

static JSValue js_mv2darray_cmp_sq(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv, int magic)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    JSValue *values = p->u.array.u.values;
    uint32_t count = p->u.array.count;
    uint64_t cmp_sq = (magic == 0) ? 0 : INT32_MAX;
    uint32_t cmp_sq_i = 0;
    uint32_t cmp_sq_j = 0;
    JSValue val;
    int32_t *pout;

    if ( magic == 0 )
    {
        for ( uint32_t i = 0; i < count; i++ )
        {
            uint64_t last_cmp_sq = cmp_sq;
            js_mvarray_cmp_sq_kernel(values[i], &cmp_sq, &cmp_sq_j, 0);
            if ( cmp_sq != last_cmp_sq )
                cmp_sq_i = i;
        }
    }
    else
    {
        for ( uint32_t i = 0; i < count; i++ )
        {
            uint64_t last_cmp_sq = cmp_sq;
            js_mvarray_cmp_sq_kernel(values[i], &cmp_sq, &cmp_sq_j, 1);
            if ( cmp_sq != last_cmp_sq )
                cmp_sq_i = i;
        }
    }

    val = JS_NewInt32FFArray(ctx, &pout, 3, 0);
    if ( unlikely(JS_IsException(val)) )
        return val;
    pout[0] = cmp_sq_i;
    pout[1] = cmp_sq_j;
    pout[2] = cmp_sq;

    return val;
}

static JSValue js_mv2darray_swap_hv(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    JSValue *values = p->u.array.u.values;
    uint32_t count = p->u.array.count;

    for ( uint32_t i = 0; i < count; i++ )
        js_mvarray_swap_hv_kernel(ctx, values[i]);

    return JS_DupValue(ctx, this_val);
}

static JSValue js_mv2darray_clear(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    JSValue *values = p->u.array.u.values;
    uint32_t count = p->u.array.count;

    for ( uint32_t i = 0; i < count; i++ )
        js_mvarray_clear_kernel(ctx, values[i]);

    return JS_DupValue(ctx, this_val);
}

static force_inline
JSValue js_mv2darray_every_internal(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv, int special)
{
    const int op = (special & 0x7);
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    struct JSMV2DArray *mv2d = p->u.array.u1.mv2darray;
    JSValue *values = p->u.array.u.values;
    uint32_t width = mv2d->width;
    uint32_t height = mv2d->height;
    JSValue obj = JS_DupValue(ctx, this_val);
    JSValue ret = JS_UNDEFINED;
    JSValueConst this_arg = JS_UNDEFINED;
    const int has_mask_arg = (op == special_maskedForEach);
    const int this_is_mask = !!(special & special_MVMASK);
    JSValue *mask_values = NULL;
    JSValueConst func;
    int32_t *dst_ptr;

    if ( has_mask_arg )
    {
        JSObject *mask_p = JS_IsObjectClass(argv[0], JS_CLASS_MV2DMASK);
        struct JSMV2DArray *mask_mv2d;
        if ( mask_p == NULL )
        {
            JS_ThrowTypeError(ctx, "first argument is not an MV2DMask");
            goto exception;
        }
        mask_mv2d = mask_p->u.array.u1.mv2darray;
        if ( mask_mv2d->width != width || mask_mv2d->height != height )
        {
            JS_ThrowRangeError(ctx, "MV2DMask width/height mismatch");
            goto exception;
        }
        mask_values = mask_p->u.array.u.values;
        argc--;
        argv++;
    }

    if ( argc > 1 )
        this_arg = argv[1];

    func = argv[0];
    if ( check_function(ctx, func) )
        goto exception;

    switch ( op )
    {
    case special_every:
        ret = JS_TRUE;
        break;
    case special_some:
        ret = JS_FALSE;
        break;
    case special_map:
        ret = JS_NewMV2DArray(ctx, &dst_ptr, width, height, 0);
        if ( JS_IsException(ret) )
            goto exception;
        break;
    }

    for ( uint32_t i = 0; i < height; i++ )
    {
        JSValue val_i = JS_NewInt32(ctx, i);
        JSObject *row_p = JS_VALUE_GET_OBJ(values[i]);
        int64_t *mask_ptr;
        if ( has_mask_arg )
        {
            JSObject *mask_p = JS_VALUE_GET_OBJ(mask_values[i]);
            mask_ptr = mask_p->u.array.u.int64_ptr;
        }
        for ( uint32_t j = 0; j < width; j++ )
        {
            JSValue val;
            JSValue val_j;
            JSValue res;
            JSValueConst args[4];
            if ( has_mask_arg && mask_ptr[j] == 0 )
                continue;
            if ( this_is_mask )
                val = JS_NewBool(ctx, row_p->u.array.u.int64_ptr[j]);
            else
                val = JS_GetMVRef(ctx, row_p, j);
            val_j = JS_NewInt32(ctx, j);
            args[0] = val;
            args[1] = val_i;
            args[2] = val_j;
            args[3] = obj;
            res = JS_Call(ctx, func, this_arg, 4, args);
            JS_FreeValue(ctx, val);
            if ( JS_IsException(res) )
                goto exception;
            switch ( op )
            {
            case special_every:
                if ( !JS_ToBoolFree(ctx, res) )
                {
                    ret = JS_FALSE;
                    goto done;
                }
                break;
            case special_some:
                if ( JS_ToBoolFree(ctx, res) )
                {
                    ret = JS_TRUE;
                    goto done;
                }
                break;
            case special_map:
                switch ( JS_VALUE_GET_TAG(res) )
                {
                case JS_TAG_MV:
                    *dst_ptr++ = JS_VALUE_GET_MV0(res);
                    *dst_ptr++ = JS_VALUE_GET_MV1(res);
                    break;
                case JS_TAG_OBJECT:
                    {
                        JSObject *res_p = JS_VALUE_GET_OBJ(res);
                        switch ( res_p->class_id )
                        {
                        case JS_CLASS_MV:
                        case JS_CLASS_MVREF:
                            *dst_ptr++ = res_p->u.array.u.int32_ptr[0];
                            *dst_ptr++ = res_p->u.array.u.int32_ptr[1];
                            break;
                        default:
                            goto exception;
                        }
                        JS_FreeValue(ctx, res);
                    }
                default:
                    goto exception;
                }
                break;
            default:
                JS_FreeValue(ctx, res);
                break;
            }
        }
    }
done:
    JS_FreeValue(ctx, obj);
    return ret;

exception:
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue js_mv2darray_every        (JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) { return js_mv2darray_every_internal(ctx, this_val, argc, argv, special_every); }
static JSValue js_mv2darray_some         (JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) { return js_mv2darray_every_internal(ctx, this_val, argc, argv, special_some); }
static JSValue js_mv2darray_forEach      (JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) { return js_mv2darray_every_internal(ctx, this_val, argc, argv, special_forEach); }
static JSValue js_mv2darray_maskedForEach(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) { return js_mv2darray_every_internal(ctx, this_val, argc, argv, special_maskedForEach); }
static JSValue js_mv2darray_map          (JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) { return js_mv2darray_every_internal(ctx, this_val, argc, argv, special_map); }

static JSValue js_mv2darray_compare(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv, int magic)
{
    JSObject *src0_p = JS_VALUE_GET_OBJ(this_val);
    struct JSMV2DArray *mv2d = src0_p->u.array.u1.mv2darray;
    JSValue *src0_values = src0_p->u.array.u.values;
    uint32_t src0_width = mv2d->width;
    uint32_t src0_height = mv2d->height;

    JSValue ret = JS_UNDEFINED;
    int64_t *dst_ptr;

    ret = JS_NewMV2DMask(ctx, &dst_ptr, src0_width, src0_height, 0);
    if ( JS_IsException(ret) )
        return ret;

    /* (compareFn, thisArg) */
    if ( magic == -1 )
    {
        JSValue obj = JS_DupValue(ctx, this_val);
        JSValueConst this_arg = JS_UNDEFINED;
        JSValueConst func = argv[0];

        if ( argc > 1 )
            this_arg = argv[1];

        if ( check_function(ctx, func) )
            goto exception;

        for ( uint32_t i = 0; i < src0_height; i++ )
        {
            JSValue val_i = JS_NewInt32(ctx, i);
            JSObject *row_p = JS_VALUE_GET_OBJ(src0_values[i]);
            for ( uint32_t j = 0; j < src0_width; j++ )
            {
                JSValue mvref = JS_GetMVRef(ctx, row_p, j);
                JSValue val_j = JS_NewInt32(ctx, j);
                JSValue res;
                JSValueConst args[4];
                args[0] = mvref;
                args[1] = val_i;
                args[2] = val_j;
                args[3] = obj;
                res = JS_Call(ctx, func, this_arg, 4, args);
                JS_FreeValue(ctx, mvref);
                if ( JS_IsException(res) )
                    goto exception;
                *dst_ptr++ = JS_ToBoolFree(ctx, res) ? -1 : 0;
            }
        }
        JS_FreeValue(ctx, obj);
        return ret;
exception:
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    else
    {
        int32_t src_mv[2] = { 0, 0 };
        JSValue *src1_values = NULL;
        int32_t *src1_ptr = NULL;
        uint32_t src1_height = 0;
        uint32_t src1_width = 0;
        int is_src_mvarray;

        if ( js_mvXXarray_op_parse_args_src(ctx, argc, argv, src_mv,
                                            &src1_values, &src1_ptr, &src1_height, &src1_width,
                                            &magic) < 0 )
        {
            JS_FreeValue(ctx, ret);
            return JS_ThrowTypeError(ctx, "error parsing arguments");
        }

        if ( (src1_height != 0 && src1_height != src0_height) )
        {
            JS_FreeValue(ctx, ret);
            return JS_ThrowRangeError(ctx, "src height mismatch");
        }
        if ( (src1_width != 0 && src1_width != src0_width) )
        {
            JS_FreeValue(ctx, ret);
            return JS_ThrowRangeError(ctx, "src width mismatch");
        }
        is_src_mvarray = (src1_width != 0);

        for ( uint32_t i = 0; i < src0_height; i++ )
        {
            JSObject *src0_p = JS_VALUE_GET_OBJ(src0_values[i]);
            int32_t *src0_ptr = src0_p->u.array.u.int32_ptr;
            if ( src1_values != NULL )
            {
                JSObject *src1_p = JS_VALUE_GET_OBJ(src1_values[i]);
                src1_ptr = src1_p->u.array.u.int32_ptr;
            }
            js_mvarray_cmp_op_internal(dst_ptr, src0_ptr, src1_ptr, src0_width, is_src_mvarray, magic);
            dst_ptr += src0_width;
        }
    }

    return ret;
}

static const JSCFunctionListEntry js_mv2darray_proto_funcs[] = {
    JS_CFUNC_DEF("toString", 0, js_mv2darray_toString ),
    /* join */
    /* copyWithin */
    JS_CFUNC_DEF("subarray", 1, js_mv2darray_subarray ),
    /* set */
    JS_CFUNC_DEF("fill", 1, js_mv2darray_fill ),
    JS_CFUNC_DEF("reverse", 0, js_mv2darray_reverse ),
    JS_CFUNC_DEF("reverse_h", 0, js_mv2darray_reverse_h ),
    JS_CFUNC_DEF("reverse_v", 0, js_mv2darray_reverse_v ),
    /* sort */
    JS_CFUNC_DEF("slice", 2, js_mv2darray_slice ),
    JS_CFUNC_DEF("dup", 0, js_mv2darray_dup ),
    JS_CFUNC_DEF("every", 1, js_mv2darray_every ),
    JS_CFUNC_DEF("some", 1, js_mv2darray_some ),
    JS_CFUNC_DEF("forEach", 1, js_mv2darray_forEach ),
    JS_CFUNC_DEF("maskedForEach", 1, js_mv2darray_maskedForEach ),
    JS_CFUNC_DEF("map", 1, js_mv2darray_map ),
    /* find */
    /* findIndex */
    /* indexOf */
    /* lastIndexOf */
    /* includes */
    /* reduce */
    /* reduceRight */
    /* values */
    /* [Symbol.iterator] */
    /* keys */
    /* entries */
    JS_CFUNC_MAGIC_DEF("largest_sq", 0, js_mv2darray_cmp_sq, 0 ),
    JS_CFUNC_MAGIC_DEF("smallest_sq", 0, js_mv2darray_cmp_sq, 1 ),
    JS_CFUNC_DEF("swap_hv", 0, js_mv2darray_swap_hv ),
    JS_CFUNC_DEF("clear", 0, js_mv2darray_clear ),
    JS_CFUNC_MAGIC_DEF("add", 2, js_mv2darray_op, mv_op_add | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("sub", 2, js_mv2darray_op, mv_op_sub | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("mul", 2, js_mv2darray_op, mv_op_mul | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("div", 2, js_mv2darray_op, mv_op_div | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("assign", 2, js_mv2darray_op, mv_op_ass | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("add_h", 2, js_mv2darray_op, mv_op_add | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("sub_h", 2, js_mv2darray_op, mv_op_sub | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("mul_h", 2, js_mv2darray_op, mv_op_mul | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("div_h", 2, js_mv2darray_op, mv_op_div | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("assign_h", 2, js_mv2darray_op, mv_op_ass | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("add_v", 2, js_mv2darray_op, mv_op_add | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("sub_v", 2, js_mv2darray_op, mv_op_sub | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("mul_v", 2, js_mv2darray_op, mv_op_mul | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("div_v", 2, js_mv2darray_op, mv_op_div | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("assign_v", 2, js_mv2darray_op, mv_op_ass | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("compare", 1, js_mv2darray_compare, -1 ),
    JS_CFUNC_MAGIC_DEF("compare_eq", 1, js_mv2darray_compare, mv_cmp_op__eq | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("compare_neq", 1, js_mv2darray_compare, mv_cmp_op_neq | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("compare_gt", 1, js_mv2darray_compare, mv_cmp_op__gt | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("compare_gte", 1, js_mv2darray_compare, mv_cmp_op_gte | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("compare_lt", 1, js_mv2darray_compare, mv_cmp_op__lt | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("compare_lte", 1, js_mv2darray_compare, mv_cmp_op_lte | mv_op_h | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("compare_eq_h", 1, js_mv2darray_compare, mv_cmp_op__eq | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("compare_neq_h", 1, js_mv2darray_compare, mv_cmp_op_neq | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("compare_gt_h", 1, js_mv2darray_compare, mv_cmp_op__gt | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("compare_gte_h", 1, js_mv2darray_compare, mv_cmp_op_gte | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("compare_lt_h", 1, js_mv2darray_compare, mv_cmp_op__lt | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("compare_lte_h", 1, js_mv2darray_compare, mv_cmp_op_lte | mv_op_h ),
    JS_CFUNC_MAGIC_DEF("compare_eq_v", 1, js_mv2darray_compare, mv_cmp_op__eq | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("compare_neq_v", 1, js_mv2darray_compare, mv_cmp_op_neq | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("compare_gt_v", 1, js_mv2darray_compare, mv_cmp_op__gt | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("compare_gte_v", 1, js_mv2darray_compare, mv_cmp_op_gte | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("compare_lt_v", 1, js_mv2darray_compare, mv_cmp_op__lt | mv_op_v ),
    JS_CFUNC_MAGIC_DEF("compare_lte_v", 1, js_mv2darray_compare, mv_cmp_op_lte | mv_op_v ),
};

/*********************************************************************/
/* MV2DMask **********************************************************/
/*********************************************************************/

static JSValue js_mv2dmask_constructor(JSContext *ctx,
                                       JSValueConst new_target,
                                       int argc, JSValueConst *argv)
{
    uint64_t width;
    uint64_t height;
    JSValue val;
    int64_t *ptr64;

    /* check arguments */
    if ( JS_ToIndex(ctx, &width, argv[0]) || width <= 0 || width >= 0x10000
      || JS_ToIndex(ctx, &height, argv[1]) || height <= 0 || height >= 0x10000 )
    {
        return JS_ThrowTypeError(ctx, "error parsing width/height arguments");
    }

    /* create new mv2darray */
    if ( likely(JS_IsUndefined(new_target)) )
        val = JS_NewObjectFromShape(ctx, js_dup_shape(ctx->mv2dmask_shape), JS_CLASS_MV2DMASK);
    else
        val = js_create_from_ctor(ctx, new_target, JS_CLASS_MV2DMASK);
    if ( unlikely(JS_IsException(val)) )
        return val;

    return JS_InitMV2D(ctx, val, (void **) &ptr64, width, height, -1, JS_CLASS_MV2DMASK);
}

JSValue JS_NewMV2DMask(JSContext *ctx, int64_t **pint64, uint32_t width, uint32_t height, int set_zero)
{
    JSValue val;

    /* create new mv2darray */
    val = JS_NewObjectFromShape(ctx, js_dup_shape(ctx->mv2dmask_shape), JS_CLASS_MV2DMASK);
    if ( unlikely(JS_IsException(val)) )
        return val;

    return JS_InitMV2D(ctx, val, (void **) pint64, width, height, set_zero, JS_CLASS_MV2DMASK);
}

static JSValue js_mv2dmask_toString(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv)
{
    return js_mv2d_toString(ctx, this_val, JS_CLASS_MV2DMASK);
}

static JSValue js_mv2dmask_fill(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    struct JSMV2DArray *mv2d = p->u.array.u1.mv2darray;
    int64_t *ptr = mv2d->u.ptr64;
    uint32_t width = mv2d->width;
    uint32_t height = mv2d->height;
    uint32_t length = width * height;
    int32_t i_idx = 0, i_end = height, i_length;
    int32_t j_idx = 0, j_end = width, j_length;
    int val = JS_ToBool(ctx, argv[0]);

    if ( val < 0 )
        return JS_ThrowTypeError(ctx, "MV2DMask.fill() takes a boolean as argument");

    argc--;
    argv++;
    /* optional range arguments */
    if ( js_mv2darray_parse_range(ctx, &i_idx, &i_end, &i_length, &j_idx, &j_end, &j_length, argc, argv) < 0 )
        return JS_ThrowTypeError(ctx, "error parsing range arguments");

    if ( i_idx < 0 || i_length <= 0 || i_end > height
      || j_idx < 0 || j_length <= 0 || j_end > width )
    {
        return JS_ThrowRangeError(ctx, "out-of-bound access");
    }

    if ( i_length == height && j_length == width )
    {
        if ( val == 0 )
            memset(ptr, 0, length * sizeof(int64_t));
        else
            memset(ptr, 0xFF, length * sizeof(int64_t));
    }
    else
    {
        int64_t val64 = val ? -1 : 0;
        for ( uint32_t i = i_idx; i < i_end; i++ )
            for ( uint32_t j = j_idx; j < j_end; j++ )
                ptr[(i * width) + j] = val64;
    }

    return JS_DupValue(ctx, this_val);
}

static JSValue js_mv2dmask_forEach(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    return js_mv2darray_every_internal(ctx, this_val, argc, argv, special_forEach | special_MVMASK);
}

static JSValue js_mv2dmask_not(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    struct JSMV2DArray *mv2d = p->u.array.u1.mv2darray;
    int64_t *ptr = mv2d->u.ptr64;
    uint32_t width = mv2d->width;
    uint32_t height = mv2d->height;
    int32_t i_idx = 0, i_end = height, i_length;
    int32_t j_idx = 0, j_end = width, j_length;

    /* optional range arguments */
    if ( js_mv2darray_parse_range(ctx, &i_idx, &i_end, &i_length, &j_idx, &j_end, &j_length, argc, argv) < 0 )
        return JS_ThrowTypeError(ctx, "error parsing range arguments");

    if ( i_idx < 0 || i_length <= 0 || i_end > height
      || j_idx < 0 || j_length <= 0 || j_end > width )
    {
        return JS_ThrowRangeError(ctx, "out-of-bound access");
    }

    for ( uint32_t i = i_idx; i < i_end; i++ )
        for ( uint32_t j = j_idx; j < j_end; j++ )
            ptr[(i * width) + j] ^= -1;

    return JS_DupValue(ctx, this_val);
}

static JSValue js_mv2dmask_op(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv, int magic)
{
    JSObject *p = JS_VALUE_GET_OBJ(this_val);
    struct JSMV2DArray *mv2d = p->u.array.u1.mv2darray;
    int64_t *ptr = mv2d->u.ptr64;
    uint32_t width = mv2d->width;
    uint32_t height = mv2d->height;
    JSObject *mask_p = JS_IsObjectClass(argv[0], JS_CLASS_MV2DMASK);
    struct JSMV2DArray *mask_mv2d;
    int64_t *mask_ptr;
    uint32_t mask_width;
    uint32_t mask_height;
    uint32_t length;

    if ( mask_p == NULL )
        return JS_ThrowTypeError(ctx, "MV2DMask operations takes an MV2DMask as argument");
    mask_mv2d = mask_p->u.array.u1.mv2darray;
    mask_ptr = mask_mv2d->u.ptr64;
    mask_width = mask_mv2d->width;
    mask_height = mask_mv2d->height;

    if ( (mask_width != width) || (mask_height != height) )
        return JS_ThrowRangeError(ctx, "MV2DMask width/height mismatch");

    length = width * height;
    switch ( magic )
    {
    case 0: for ( uint32_t i = 0; i < length; i++ ) ptr[i] &= mask_ptr[i]; break;
    case 1: for ( uint32_t i = 0; i < length; i++ ) ptr[i] |= mask_ptr[i]; break;
    case 2: for ( uint32_t i = 0; i < length; i++ ) ptr[i] ^= mask_ptr[i]; break;
    }

    return JS_DupValue(ctx, this_val);
}

static const JSCFunctionListEntry js_mv2dmask_proto_funcs[] = {
    JS_CFUNC_DEF("toString", 0, js_mv2dmask_toString ),
    JS_CFUNC_DEF("fill", 1, js_mv2dmask_fill ),
    JS_CFUNC_DEF("forEach", 1, js_mv2dmask_forEach ),
    JS_CFUNC_DEF("not", 0, js_mv2dmask_not ),
    JS_CFUNC_MAGIC_DEF("and", 1, js_mv2dmask_op, 0 ),
    JS_CFUNC_MAGIC_DEF("or", 1, js_mv2dmask_op, 1 ),
    JS_CFUNC_MAGIC_DEF("xor", 1, js_mv2dmask_op, 2 ),
};

/*********************************************************************/
/* MV2DPtr ***********************************************************/
/*********************************************************************/

static void js_mv2dptr_finalizer(JSRuntime *rt, JSValue val)
{
    JSObject *p = JS_VALUE_GET_OBJ(val);
    js_array_finalizer(rt, val);
    js_free_rt(rt, p->u.array.u1.mv2darray);
}

static JSValue js_mv2dptr_constructor(JSContext *ctx,
                                      JSValueConst new_target,
                                      int argc, JSValueConst *argv)
{
    JSObject *p;
    struct JSMV2DArray *mv2d;
    JSValue *src_values;
    uint32_t width;
    uint32_t height;
    JSValue dst_array;
    JSValue *dst_values;

    /* check input argument */
    p = JS_IsObjectClass(argv[0], JS_CLASS_MV2DARRAY);
    if ( p == NULL )
        p = JS_IsObjectClass(argv[0], JS_CLASS_MV2DPTR);
    if ( p == NULL )
        return JS_ThrowTypeError(ctx, "MV2DPtr() takes either an MV2DArray or an MV2DPtr as argument");

    mv2d = p->u.array.u1.mv2darray;
    src_values = p->u.array.u.values;
    width = mv2d->width;
    height = mv2d->height;

    /* create new mv2dptr */
    dst_array = JS_NewMV2DPtr(ctx, new_target, &dst_values, width, height);
    if ( unlikely(JS_IsException(dst_array)) )
        return dst_array;

    /* set pointers */
    for ( size_t i = 0; i < height; i++ )
    {
        JSObject *src_line = JS_VALUE_GET_OBJ(src_values[i]);
        JSObject *dst_line = JS_VALUE_GET_OBJ(dst_values[i]);
        dst_line->u.array.u.int32_ptr = src_line->u.array.u.int32_ptr;
    }

    return dst_array;
}

/*********************************************************************/
void JS_AddIntrinsicMVs(JSContext *ctx)
{
    /* MV */
    INIT_CLASS_PROTO_FUNCS(ctx, &ctx->class_proto[JS_CLASS_MV], js_mv_proto_funcs);
    ctx->mv_shape = create_arraylike_shape(ctx, ctx->class_proto[JS_CLASS_MV]);
    JS_NewGlobalCConstructor(ctx, "MV", js_mv_constructor, 2, ctx->class_proto[JS_CLASS_MV]);

    /* MVRef */
    INIT_CLASS_PROTO_FUNCS(ctx, &ctx->class_proto[JS_CLASS_MVREF], js_mv_proto_funcs);
    ctx->mvref_shape = create_arraylike_shape(ctx, ctx->class_proto[JS_CLASS_MVREF]);
    JS_NewGlobalCConstructor(ctx, "MVRef", js_mvref_constructor, 1, ctx->class_proto[JS_CLASS_MVREF]);

    /* MVArray */
    INIT_CLASS_PROTO_FUNCS(ctx, &ctx->class_proto[JS_CLASS_MVARRAY], js_mvarray_proto_funcs);
    ctx->mvarray_shape = create_arraylike_shape(ctx, ctx->class_proto[JS_CLASS_MVARRAY]);
    JS_NewGlobalCConstructor(ctx, "MVArray", js_mvarray_constructor, 1, ctx->class_proto[JS_CLASS_MVARRAY]);

    /* MVPtr */
    INIT_CLASS_PROTO_FUNCS(ctx, &ctx->class_proto[JS_CLASS_MVPTR], js_mvarray_proto_funcs);
    ctx->mvptr_shape = create_arraylike_shape(ctx, ctx->class_proto[JS_CLASS_MVPTR]);
    JS_NewGlobalCConstructor(ctx, "MVPtr", js_mvptr_constructor, 1, ctx->class_proto[JS_CLASS_MVPTR]);

    /* MVMask */
    INIT_CLASS_PROTO_FUNCS(ctx, &ctx->class_proto[JS_CLASS_MVMASK], js_mvmask_proto_funcs);
    ctx->mvmask_shape = create_arraylike_shape(ctx, ctx->class_proto[JS_CLASS_MVMASK]);
    JS_NewGlobalCConstructor(ctx, "MVMask", js_mvmask_constructor, 1, ctx->class_proto[JS_CLASS_MVMASK]);

    /* MV2DArray */
    INIT_CLASS_PROTO_FUNCS(ctx, &ctx->class_proto[JS_CLASS_MV2DARRAY], js_mv2darray_proto_funcs);
    ctx->mv2darray_shape = create_2darraylike_shape(ctx, ctx->class_proto[JS_CLASS_MV2DARRAY]);
    JS_NewGlobalCConstructor(ctx, "MV2DArray", js_mv2darray_constructor, 2, ctx->class_proto[JS_CLASS_MV2DARRAY]);

    /* MV2DPtr */
    INIT_CLASS_PROTO_FUNCS(ctx, &ctx->class_proto[JS_CLASS_MV2DPTR], js_mv2darray_proto_funcs);
    ctx->mv2dptr_shape = create_2darraylike_shape(ctx, ctx->class_proto[JS_CLASS_MV2DPTR]);
    JS_NewGlobalCConstructor(ctx, "MV2DPtr", js_mv2dptr_constructor, 2, ctx->class_proto[JS_CLASS_MV2DPTR]);

    /* MV2DMask */
    INIT_CLASS_PROTO_FUNCS(ctx, &ctx->class_proto[JS_CLASS_MV2DMASK], js_mv2dmask_proto_funcs);
    ctx->mv2dmask_shape = create_2darraylike_shape(ctx, ctx->class_proto[JS_CLASS_MV2DMASK]);
    JS_NewGlobalCConstructor(ctx, "MV2DMask", js_mv2dmask_constructor, 2, ctx->class_proto[JS_CLASS_MV2DMASK]);
}
