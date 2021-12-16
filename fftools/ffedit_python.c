#include "compat/load_python.c"

/*********************************************************************/
typedef struct PythonContext {
    void *libpython_so;

    PythonFunctions pyfuncs;

    PyObject *PyBool_Type;
    PyObject *PyDict_Type;
    PyObject *PyExc_IndexError;
    PyObject *PyList_Type;
    PyObject *PyLong_Type;
    PyObject *PyUnicode_Type;
    PyObject *Py_None;
    PyObject *Py_True;

    PyObject *globals;
    PyObject *module;
    PyObject *locals;
    PyObject *builtins;
    PyObject *glitch_frame;
} PythonContext;

//---------------------------------------------------------------------
static inline PyObject *Py_None_New(PythonContext *pctx)
{
    PythonFunctions *pyfuncs = &pctx->pyfuncs;
    pyfuncs->Py_IncRef(pctx->Py_None);
    return pctx->Py_None;
}

static PyObject *ffedit_to_python(PythonContext *pctx, json_t *jso)
{
    PythonFunctions *pyfuncs = &pctx->pyfuncs;
    PyObject *val = NULL;
    size_t len;
    if ( jso == NULL )
        return Py_None_New(pctx);
    switch ( JSON_TYPE(jso->flags) )
    {
    case JSON_TYPE_OBJECT:
        len = JSON_LEN(jso->flags);
        val = pyfuncs->PyDict_New();
        for ( size_t i = 0; i < len; i++ )
        {
            PyObject *val_i = ffedit_to_python(pctx, jso->obj->values[i]);
            pyfuncs->PyDict_SetItemString(val, jso->obj->names[i], val_i);
            pyfuncs->Py_DecRef(val_i);
        }
        break;
    case JSON_TYPE_ARRAY:
        len = JSON_LEN(jso->flags);
        val = pyfuncs->PyList_New(len);
        for ( size_t i = 0; i < len; i++ )
        {
            PyObject *val_i = ffedit_to_python(pctx, jso->array[i]);
            pyfuncs->PyList_SetItem(val, i, val_i);
        }
        break;
    case JSON_TYPE_ARRAY_OF_INTS:
        len = JSON_LEN(jso->flags);
        val = pyfuncs->PyList_New(len);
        for ( size_t i = 0; i < len; i++ )
        {
            PyObject *val_i = NULL;
            if ( jso->array_of_ints[i] == JSON_NULL )
                val_i = Py_None_New(pctx);
            else
                val_i = pyfuncs->PyLong_FromLong(jso->array_of_ints[i]);
            pyfuncs->PyList_SetItem(val, i, val_i);
        }
        break;
    case JSON_TYPE_STRING:
        val = pyfuncs->PyUnicode_FromString(jso->str);
        break;
    case JSON_TYPE_NUMBER:
        if ( jso->val == JSON_NULL )
            val = Py_None_New(pctx);
        else
            val = pyfuncs->PyLong_FromLongLong(jso->val);
        break;
    case JSON_TYPE_BOOL:
        val = pyfuncs->PyBool_FromLong(jso->val);
        break;
    }
    if ( val == NULL )
        val = Py_None_New(pctx);
    return val;
}

static json_t *python_to_ffedit(json_ctx_t *jctx, PythonContext *pctx, PyObject *val)
{
    PythonFunctions *pyfuncs = &pctx->pyfuncs;
    if ( val->ob_type == pctx->PyLong_Type )
        return json_int_new(jctx, pyfuncs->PyLong_AsLongLong(val));
    if ( val->ob_type == pctx->PyUnicode_Type )
        return json_string_new(jctx, pyfuncs->PyUnicode_AsUTF8AndSize(val, NULL));
    if ( val->ob_type == pctx->PyBool_Type )
        return json_bool_new(jctx, (val == pctx->Py_True));
    if ( val->ob_type == pctx->PyList_Type )
    {
#if 0
        size_t length = pyfuncs->PyList_Size(val);
        json_t *array = json_array_new(jctx, length);
        for ( size_t i = 0; i < length; i++ )
        {
            PyObject *val_i = pyfuncs->PyList_GetItem(val, i);
            json_array_set(array, i, python_to_ffedit(jctx, pctx, val_i));
        }
        return array;
#else
        int is_array_of_ints = 0;
        size_t length = pyfuncs->PyList_Size(val);
        json_t *array;

        for ( size_t i = 0; i < length; i++ )
        {
            PyObject *val_i = pyfuncs->PyList_GetItem(val, i);
            is_array_of_ints = (val_i->ob_type == pctx->PyLong_Type);
            if ( !is_array_of_ints )
                break;
        }

        if ( is_array_of_ints )
            array = json_array_of_ints_new(jctx, length);
        else
            array = json_array_new(jctx, length);

        for ( size_t i = 0; i < length; i++ )
        {
            PyObject *val_i = pyfuncs->PyList_GetItem(val, i);
            if ( is_array_of_ints )
                json_array_set_int(jctx, array, i, pyfuncs->PyLong_AsLong(val_i));
            else
                json_array_set(array, i, python_to_ffedit(jctx, pctx, val_i));
        }

        return array;
#endif
    }
    if ( val->ob_type == pctx->PyDict_Type )
    {
        json_t *object = json_object_new(jctx);
        PyObject *key;
        PyObject *value;
        size_t pos = 0;
        while ( pyfuncs->PyDict_Next(val, &pos, &key, &value) )
        {
            const char *str = pyfuncs->PyUnicode_AsUTF8AndSize(key, NULL);
            json_object_add(object, str, python_to_ffedit(jctx, pctx, value));
        }
        json_object_done(jctx, object);
        return object;
    }
    return NULL;
}

/*********************************************************************/
static int python_init(PythonContext *pctx, FFFile *fff)
{
    PythonFunctions *pyfuncs = &pctx->pyfuncs;
    PyObject *pysrc;
    PyObject *pyret;
    int ret = 0;

    pctx->libpython_so = dlopen_python();
    if ( pctx->libpython_so == NULL )
    {
        av_log(ffe_class, AV_LOG_FATAL, "Could not find libpython in the usual places.\n");
        av_log(ffe_class, AV_LOG_FATAL, "Try specifying the path to libpython using the FFGLITCH_LIBPYTHON_PATH environment variable.\n");
        ret = AVERROR(EINVAL);
        goto end;
    }

    if ( !dlsyms_python(&pctx->pyfuncs, pctx->libpython_so) )
    {
        av_log(ffe_class, AV_LOG_FATAL, "Could not load all necessary functions from libpython.\n");
        ret = AVERROR(EINVAL);
        goto end;
    }

    pyfuncs->Py_Initialize();
    pctx->module = pyfuncs->PyImport_AddModule("__main__");
    pctx->locals = pyfuncs->PyModule_GetDict(pctx->module);
    pctx->globals = pyfuncs->PyDict_New();
    pctx->builtins = pyfuncs->PyEval_GetBuiltins();
    pyfuncs->PyDict_SetItemString(pctx->globals, "__builtins__", pctx->builtins);

    pctx->PyBool_Type      = pyfuncs->PyDict_GetItemString(pctx->builtins, "bool");
    pctx->PyDict_Type      = pyfuncs->PyDict_GetItemString(pctx->builtins, "dict");
    pctx->PyExc_IndexError = pyfuncs->PyDict_GetItemString(pctx->builtins, "IndexError");
    pctx->PyList_Type      = pyfuncs->PyDict_GetItemString(pctx->builtins, "list");
    pctx->PyLong_Type      = pyfuncs->PyDict_GetItemString(pctx->builtins, "int");
    pctx->PyUnicode_Type   = pyfuncs->PyDict_GetItemString(pctx->builtins, "str");
    pctx->Py_None          = pyfuncs->PyDict_GetItemString(pctx->builtins, "None");
    pctx->Py_True          = pyfuncs->PyDict_GetItemString(pctx->builtins, "True");

    pysrc = pyfuncs->Py_CompileString(fff->s_buf, fff->s_fname, Py_file_input);
    if ( pysrc == NULL )
    {
        av_log(ffe_class, AV_LOG_FATAL, "Could not compile script file %s\n", fff->s_fname);
        pyfuncs->PyErr_Print();
        ret = AVERROR(EINVAL);
        goto end;
    }

    pyret = pyfuncs->PyEval_EvalCode(pysrc, pctx->globals, pctx->locals);
    if ( pyret == NULL )
    {
        av_log(ffe_class, AV_LOG_FATAL, "Could not eval script file %s\n", fff->s_fname);
        pyfuncs->PyErr_Print();
        ret = AVERROR(EINVAL);
        goto end;
    }
    pyfuncs->Py_DecRef(pyret);
    pyfuncs->Py_DecRef(pysrc);

    pctx->glitch_frame = pyfuncs->PyObject_GetAttrString(pctx->module, "glitch_frame");
    if ( pctx->glitch_frame == NULL
      || !pyfuncs->PyCallable_Check(pctx->glitch_frame) )
    {
        av_log(ffe_class, AV_LOG_FATAL, "Could not find function glitch_frame() in %s\n", fff->s_fname);
        pyfuncs->PyErr_Print();
        ret = AVERROR(EINVAL);
        goto end;
    }

end:
    return ret;
}

/*********************************************************************/
static void python_uninit(PythonContext *pctx)
{
    PythonFunctions *pyfuncs = &pctx->pyfuncs;
    pyfuncs->Py_Finalize();
    dlclose(pctx->libpython_so);
}

/*********************************************************************/
static void *python_func(void *arg)
{
#define TIME_PYTHON 0
#if TIME_PYTHON
    int64_t convert1 = 0;
    int64_t call = 0;
    int64_t convert2 = 0;
    int64_t t0;
    int64_t t1;
#endif
    FFFile *fff = (FFFile *) arg;
    AVPacket ipkt;
    AVFrame iframe;

    PythonContext pctx;
    PythonFunctions *pyfuncs = &pctx.pyfuncs;

    if ( python_init(&pctx, fff) != 0 )
        exit(1);

    pthread_mutex_lock(&fff->s_mutex);
    fff->s_init = 1;
    pthread_cond_signal(&fff->s_cond);
    pthread_mutex_unlock(&fff->s_mutex);

    while ( 42 )
    {
        json_t *jargs;
        json_t *jframe;
        PyObject *pyargs;
        PyObject *pyret;

        /* get earliest packet from input queue */
        ipkt.pos = -1;
        get_from_ffedit_json_queue(fff->jq_in, &ipkt);

        /* check for poison pill */
        if ( ipkt.pos == -1 )
            break;

#if TIME_PYTHON
        t0 = av_gettime_relative();
#endif
        /* convert json to python */
        jargs = json_object_new(ipkt.jctx);
        for ( size_t i = 0; i < FFEDIT_FEAT_LAST; i++ )
            if ( ipkt.ffedit_sd[i] != NULL )
                json_object_add(jargs, ffe_feat_to_str(i), ipkt.ffedit_sd[i]);
        json_object_done(ipkt.jctx, jargs);
        pyargs = ffedit_to_python(&pctx, jargs);
        /* free jctx from ipkt, we no longer need it */
        json_ctx_free(ipkt.jctx);

#if TIME_PYTHON
        t1 = av_gettime_relative();
        convert1 += (t1 - t0);
        t0 = t1;
#endif

        /* call glitch_frame() with data */
        pyret = pyfuncs->PyObject_CallFunction(pctx.glitch_frame, "O", pyargs);
        if ( pyret == NULL )
        {
            av_log(ffe_class, AV_LOG_FATAL, "Error calling glitch_frame() function in %s\n", fff->s_fname);
            pyfuncs->PyErr_Print();
            exit(1);
        }
        pyfuncs->Py_DecRef(pyret);

#if TIME_PYTHON
        t1 = av_gettime_relative();
        call += (t1 - t0);
        t0 = t1;
#endif

        /* convert json back to ffedit */
        iframe.jctx = av_mallocz(sizeof(json_ctx_t));
        json_ctx_start(iframe.jctx);
        jframe = python_to_ffedit(iframe.jctx, &pctx, pyargs);
        for ( size_t i = 0; i < FFEDIT_FEAT_LAST; i++ )
        {
            const char *key = ffe_feat_to_str(i);
            iframe.ffedit_sd[i] = json_object_get(jframe, key);
        }
        iframe.pkt_pos = ipkt.pos;
        pyfuncs->Py_DecRef(pyargs);

#if TIME_PYTHON
        t1 = av_gettime_relative();
        convert2 += (t1 - t0);
        t0 = t1;
#endif

        /* add back to output queue */
        add_frame_to_ffedit_json_queue(fff->jq_out, &iframe);
    }

#if TIME_PYTHON
    printf("time taken convert1 %" PRId64 " call %" PRId64 " convert2 %" PRId64 "\n", convert1, call, convert2);
#endif

    python_uninit(&pctx);

    return NULL;
}
