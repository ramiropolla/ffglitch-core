//---------------------------------------------------------------------
static int64_t __JS_ToInt64(JSContext *ctx, JSValue val)
{
    int64_t i64;
    JS_ToInt64(ctx, &i64, val);
    return i64;
}

static inline JS_BOOL JS_IsInt32(JSValueConst v)
{
    int tag = JS_VALUE_GET_TAG(v);
    return tag == JS_TAG_INT;
}

static inline JS_BOOL JS_IsFloat64(JSValueConst v)
{
    int tag = JS_VALUE_GET_TAG(v);
    return JS_TAG_IS_FLOAT64(tag);
}

//---------------------------------------------------------------------
static JSValue ffedit_to_quickjs(JSContext *ctx, json_t *jso)
{
    JSValue val = JS_NULL;
    JSValue *parray;
    if ( jso == NULL )
        return JS_NULL;
    switch ( JSON_TYPE(jso->flags) )
    {
    case JSON_TYPE_OBJECT:
        val = JS_NewObject(ctx);
        for ( size_t i = 0; i < JSON_LEN(jso->flags); i++ )
            JS_SetPropertyStr(ctx, val, jso->obj->names[i], ffedit_to_quickjs(ctx, jso->obj->values[i]));
        break;
    case JSON_TYPE_ARRAY:
        val = JS_NewFastArray(ctx, &parray, JSON_LEN(jso->flags));
        for ( size_t i = 0; i < JSON_LEN(jso->flags); i++ )
            parray[i] = ffedit_to_quickjs(ctx, jso->array[i]);
        break;
    case JSON_TYPE_ARRAY_OF_INTS:
        val = JS_NewFastArray(ctx, &parray, JSON_LEN(jso->flags));
        for ( size_t i = 0; i < JSON_LEN(jso->flags); i++ )
        {
            if ( jso->array_of_ints[i] == JSON_NULL )
                parray[i] = JS_NULL;
            else
                parray[i] = JS_NewInt32(ctx, jso->array_of_ints[i]);
        }
        break;
    case JSON_TYPE_STRING:
        val = JS_NewString(ctx, jso->str);
        break;
    case JSON_TYPE_NUMBER:
        if ( jso->val == JSON_NULL )
            val = JS_NULL;
        else
            val = JS_NewInt64(ctx, jso->val);
        break;
    case JSON_TYPE_BOOL:
        val = JS_NewBool(ctx, jso->val);
        break;
    }
    return val;
}

static json_t *quickjs_to_ffedit(json_ctx_t *jctx, JSContext *ctx, JSValue val)
{
    if ( JS_IsInt32(val) )
        return json_int_new(jctx, JS_VALUE_GET_INT(val));
    if ( JS_IsFloat64(val) )
        return json_int_new(jctx, JS_VALUE_GET_FLOAT64(val));
    if ( JS_IsString(val) )
        return json_string_new(jctx, JS_ToCString(ctx, val));
    if ( JS_IsBool(val) )
        return json_bool_new(jctx, JS_ToBool(ctx, val));
    if ( JS_IsArray(ctx, val) )
    {
        int is_array_of_ints = 0;
        uint32_t length;
        json_t *array;

        JSValue *parray = NULL;

        JS_GetFastArray(val, &parray, &length);

        if ( parray == NULL )
        {
            JSValue length_val = JS_GetPropertyStr(ctx, val, "length");
            JS_ToUint32(ctx, &length, length_val);
            JS_FreeValue(ctx, length_val);

            for ( size_t i = 0; i < length; i++ )
            {
                JSValue val_i = JS_GetPropertyUint32(ctx, val, i);
                is_array_of_ints = JS_IsNumber(val_i);
                JS_FreeValue(ctx, val_i);
                if ( !is_array_of_ints )
                    break;
            }
        }
        else
        {
            for ( size_t i = 0; i < length; i++ )
            {
                is_array_of_ints = JS_IsNumber(parray[i]);
                if ( !is_array_of_ints )
                    break;
            }
        }

        if ( is_array_of_ints )
            array = json_array_of_ints_new(jctx, length);
        else
            array = json_array_new(jctx, length);

        if ( parray == NULL )
        {
            for ( size_t i = 0; i < length; i++ )
            {
                JSValue val_i = JS_GetPropertyUint32(ctx, val, i);
                if ( is_array_of_ints )
                    json_array_set_int(jctx, array, i, __JS_ToInt64(ctx, val_i));
                else
                    json_array_set(array, i, quickjs_to_ffedit(jctx, ctx, val_i));
                JS_FreeValue(ctx, val_i);
            }
        }
        else
        {
            for ( size_t i = 0; i < length; i++ )
            {
                if ( is_array_of_ints )
                    json_array_set_int(jctx, array, i, __JS_ToInt64(ctx, parray[i]));
                else
                    json_array_set(array, i, quickjs_to_ffedit(jctx, ctx, parray[i]));
            }
        }

        return array;
    }
    if ( JS_IsObject(val) )
    {
        json_t *object = json_object_new(jctx);
        JSPropertyEnum *tab;
        uint32_t length;
        JS_GetOwnPropertyNames(ctx, &tab, &length, val, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY);
        for ( size_t i = 0; i < length; i++ )
        {
            const char *str = JS_AtomToCString(ctx, tab[i].atom);
            JSValue val_i = JS_GetProperty(ctx, val, tab[i].atom);
            json_object_add(object, str, quickjs_to_ffedit(jctx, ctx, val_i));
            JS_FreeValue(ctx, val_i);
            JS_FreeAtom(ctx, tab[i].atom);
        }
        js_free(ctx, tab);
        json_object_done(jctx, object);
        return object;
    }
    return NULL;
}

static void *quickjs_func(void *arg)
{
#define TIME_QUICKJS 0
#if TIME_QUICKJS
    int64_t convert1 = 0;
    int64_t call = 0;
    int64_t convert2 = 0;
    int64_t t0;
    int64_t t1;
#endif
    FFFile *fff = (FFFile *) arg;
    AVPacket ipkt;
    AVFrame iframe;
    JSValue global_object;
    JSValue glitch_frame;
    JSValue val;

    /* TODO check for errors */
    fff->qjs_rt = JS_NewRuntime();
    js_std_init_handlers(fff->qjs_rt);
    fff->qjs_ctx = JS_NewContext(fff->qjs_rt);

    /* loader for ES6 modules */
    JS_SetModuleLoaderFunc(fff->qjs_rt, NULL, js_module_loader, NULL);
    js_std_add_helpers(fff->qjs_ctx, 0, NULL);

    /* system modules */
    js_init_module_std(fff->qjs_ctx, "std");
    js_init_module_os(fff->qjs_ctx, "os");

    val = JS_Eval(fff->qjs_ctx, fff->qjs_buf, fff->qjs_size, fff->s_fname, JS_EVAL_TYPE_GLOBAL);
    if ( JS_IsException(val) )
    {
        js_std_dump_error(fff->qjs_ctx);
        exit(1);
    }
    JS_FreeValue(fff->qjs_ctx, val);

    global_object = JS_GetGlobalObject(fff->qjs_ctx);
    glitch_frame = JS_GetPropertyStr(fff->qjs_ctx, global_object, "glitch_frame");
    if ( JS_IsUndefined(glitch_frame) )
    {
        av_log(NULL, AV_LOG_FATAL, "function glitch_frame() not found in %s\n", fff->s_fname);
        exit(1);
    }

    pthread_mutex_lock(&fff->qjs_mutex);
    fff->qjs_init = 1;
    pthread_cond_signal(&fff->qjs_cond);
    pthread_mutex_unlock(&fff->qjs_mutex);

    while ( 42 )
    {
        json_t *jargs;
        json_t *jframe;
        JSValue args;
        JSValue val;

        /* get earliest packet from input queue */
        ipkt.pos = -1;
        get_from_ffedit_json_queue(fff->jq_in, &ipkt);

        /* check for poison pill */
        if ( ipkt.pos == -1 )
            break;

#if TIME_QUICKJS
        t0 = av_gettime_relative();
#endif
        /* convert json to quickjs */
        jargs = json_object_new(ipkt.jctx);
        for ( size_t i = 0; i < FFEDIT_FEAT_LAST; i++ )
            if ( ipkt.ffedit_sd[i] != NULL )
                json_object_add(jargs, ffe_feat_to_str(i), ipkt.ffedit_sd[i]);
        json_object_done(ipkt.jctx, jargs);
        args = ffedit_to_quickjs(fff->qjs_ctx, jargs);
        /* free jctx from ipkt, we no longer need it */
        json_ctx_free(ipkt.jctx);

#if TIME_QUICKJS
        t1 = av_gettime_relative();
        convert1 += (t1 - t0);
        t0 = t1;
#endif

        /* call glitch_frame() with data */
        val = JS_Call(fff->qjs_ctx, glitch_frame, JS_UNDEFINED, 1, &args);
        if ( JS_IsException(val) )
        {
            js_std_dump_error(fff->qjs_ctx);
            exit(1);
        }
        JS_FreeValue(fff->qjs_ctx, val);

#if TIME_QUICKJS
        t1 = av_gettime_relative();
        call += (t1 - t0);
        t0 = t1;
#endif

        /* convert json back to ffedit */
        iframe.jctx = av_mallocz(sizeof(json_ctx_t));
        json_ctx_start(iframe.jctx);
        jframe = quickjs_to_ffedit(iframe.jctx, fff->qjs_ctx, args);
        for ( size_t i = 0; i < FFEDIT_FEAT_LAST; i++ )
        {
            const char *key = ffe_feat_to_str(i);
            iframe.ffedit_sd[i] = json_object_get(jframe, key);
        }
        iframe.pkt_pos = ipkt.pos;
        JS_FreeValue(fff->qjs_ctx, args);

#if TIME_QUICKJS
        t1 = av_gettime_relative();
        convert2 += (t1 - t0);
        t0 = t1;
#endif

        /* add back to output queue */
        add_frame_to_ffedit_json_queue(fff->jq_out, &iframe);
    }

#if TIME_QUICKJS
    printf("time taken convert1 %" PRId64 " call %" PRId64 " convert2 %" PRId64 "\n", convert1, call, convert2);
#endif

    JS_FreeValue(fff->qjs_ctx, glitch_frame);
    JS_FreeValue(fff->qjs_ctx, global_object);

    /* free quickjs */
    js_std_free_handlers(fff->qjs_rt);
    JS_FreeContext(fff->qjs_ctx);
    JS_FreeRuntime(fff->qjs_rt);

    return NULL;
}
