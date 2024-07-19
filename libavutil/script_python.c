/*
 * Copyright (C) 2020-2024 Ramiro Polla
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*********************************************************************/
#include "libavutil/avassert.h"
#include "libavutil/bprint.h"
#include "libavutil/error.h"
#include "libavutil/mem.h"
#include "libavutil/script.h"

/*********************************************************************/
#ifdef _WIN32
  #include "compat/w32dlfcn.h"
  #include "shlobj.h"
#else
  #include <dlfcn.h>
#endif

/*********************************************************************/
static const AVClass python_class = {
    .class_name = "python",
    .item_name  = av_default_item_name,
    .version    = LIBAVUTIL_VERSION_INT,
};

/*********************************************************************/
#ifdef _WIN32
static char *dup_wchar_to_utf8(wchar_t *w)
{
    char *s = NULL;
    int l = WideCharToMultiByte(CP_UTF8, 0, w, -1, 0, 0, 0, 0);
    s = av_malloc(l);
    if (s)
        WideCharToMultiByte(CP_UTF8, 0, w, -1, s, l, 0, 0);
    return s;
}
#endif

static void *dlopen_python(void)
{
    void *libpython_so = NULL;
    char *path = getenv("FFGLITCH_LIBPYTHON_PATH");
    if ( path != NULL )
    {
        av_log(NULL, AV_LOG_VERBOSE, "Trying to load libpython \"%s\" (from FFGLITCH_LIBPYTHON_PATH environment variable)...", path);
#if defined(_WIN32)
        libpython_so = LoadLibraryExA(path, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
#else
        libpython_so = dlopen(path, RTLD_LAZY | RTLD_GLOBAL);
#endif
        if ( libpython_so == NULL )
            av_log(NULL, AV_LOG_VERBOSE, " not");
        av_log(NULL, AV_LOG_VERBOSE, " found\n");
    }
    else
    {
#if defined(_WIN32)
        static const char *paths[] = {
            "Python/Python313/python3.dll",
            "Python/Python312/python3.dll",
            "Python/Python311/python3.dll",
            "Python/Python310/python3.dll",
            "Python/Python39/python3.dll",
            "Python/Python38/python3.dll",
            "Python/Python37/python3.dll",
            "Python/Python36/python3.dll",
            "Python/Python35/python3.dll",
        };
        wchar_t *appdata;
        char *appdata_utf8;
        if ( SHGetKnownFolderPath(&FOLDERID_UserProgramFiles, KF_FLAG_DEFAULT, NULL, &appdata) != 0 )
        {
            av_log(NULL, AV_LOG_FATAL, "SHGetKnownFolderPath() failed\n");
            return NULL;
        }
        appdata_utf8 = dup_wchar_to_utf8(appdata);
        CoTaskMemFree(appdata);
        for ( int i = 0; i < FF_ARRAY_ELEMS(paths) && (libpython_so == NULL); i++ )
        {
            char path[4096];
            snprintf(path, sizeof(path), "%s/%s", appdata_utf8, paths[i]);
            av_log(NULL, AV_LOG_VERBOSE, "Trying to load libpython \"%s\" ...", path);
            libpython_so = LoadLibraryExA(path, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            if ( libpython_so == NULL )
                av_log(NULL, AV_LOG_VERBOSE, " not");
            av_log(NULL, AV_LOG_VERBOSE, " found\n");
        }
        av_freep(&appdata_utf8);
#else
        static const char *paths[] = {
#if defined(__APPLE__)
            "/Library/Frameworks/Python.framework/Versions/3.13/lib/libpython3.13.dylib",
            "/Library/Frameworks/Python.framework/Versions/3.12/lib/libpython3.12.dylib",
            "/Library/Frameworks/Python.framework/Versions/3.11/lib/libpython3.11.dylib",
            "/Library/Frameworks/Python.framework/Versions/3.10/lib/libpython3.10.dylib",
            "/Library/Frameworks/Python.framework/Versions/3.9/lib/libpython3.9.dylib",
            "/Library/Frameworks/Python.framework/Versions/3.8/lib/libpython3.8.dylib",
            "/Library/Frameworks/Python.framework/Versions/3.7/lib/libpython3.7.dylib",
            "/Library/Frameworks/Python.framework/Versions/3.6/lib/libpython3.6.dylib",
            "/Library/Frameworks/Python.framework/Versions/3.5/lib/libpython3.5.dylib",
#else
            "libpython3.13m.so.1",
            "libpython3.13.so.1",
            "libpython3.12m.so.1",
            "libpython3.12.so.1",
            "libpython3.11m.so.1",
            "libpython3.11.so.1",
            "libpython3.10m.so.1",
            "libpython3.10.so.1",
            "libpython3.9m.so.1",
            "libpython3.9.so.1",
            "libpython3.8m.so.1",
            "libpython3.8.so.1",
            "libpython3.7m.so.1",
            "libpython3.7.so.1",
            "libpython3.6m.so.1",
            "libpython3.6.so.1",
            "libpython3.5m.so.1",
            "libpython3.5.so.1",
#endif
        };
        for ( size_t i = 0; i < FF_ARRAY_ELEMS(paths) && (libpython_so == NULL); i++ )
        {
            const char *path = paths[i];
            av_log(NULL, AV_LOG_VERBOSE, "Trying to load libpython \"%s\" ...", path);
            libpython_so = dlopen(path, RTLD_LAZY | RTLD_GLOBAL);
            if ( libpython_so == NULL )
                av_log(NULL, AV_LOG_VERBOSE, " not");
            av_log(NULL, AV_LOG_VERBOSE, " found\n");
        }
#endif
    }
    return libpython_so;
}

/*********************************************************************/
static int dlsyms_python(PythonFunctions *pyfuncs, void *libpython_so)
{
    *(void **) &pyfuncs->PyBool_FromLong         = dlsym(libpython_so, "PyBool_FromLong");
    *(void **) &pyfuncs->PyCFunction_NewEx       = dlsym(libpython_so, "PyCFunction_NewEx");
    *(void **) &pyfuncs->PyCallable_Check        = dlsym(libpython_so, "PyCallable_Check");
    *(void **) &pyfuncs->PyDict_GetItemString    = dlsym(libpython_so, "PyDict_GetItemString");
    *(void **) &pyfuncs->PyDict_New              = dlsym(libpython_so, "PyDict_New");
    *(void **) &pyfuncs->PyDict_Next             = dlsym(libpython_so, "PyDict_Next");
    *(void **) &pyfuncs->PyDict_SetItemString    = dlsym(libpython_so, "PyDict_SetItemString");
    *(void **) &pyfuncs->PyErr_Clear             = dlsym(libpython_so, "PyErr_Clear");
    *(void **) &pyfuncs->PyErr_Print             = dlsym(libpython_so, "PyErr_Print");
    *(void **) &pyfuncs->PyErr_SetString         = dlsym(libpython_so, "PyErr_SetString");
    *(void **) &pyfuncs->PyEval_GetBuiltins      = dlsym(libpython_so, "PyEval_GetBuiltins");
    *(void **) &pyfuncs->PyFloat_FromDouble      = dlsym(libpython_so, "PyFloat_FromDouble");
    *(void **) &pyfuncs->PyImport_ExecCodeModule = dlsym(libpython_so, "PyImport_ExecCodeModule");
    *(void **) &pyfuncs->PyImport_ImportModule   = dlsym(libpython_so, "PyImport_ImportModule");
    *(void **) &pyfuncs->PyList_Append           = dlsym(libpython_so, "PyList_Append");
    *(void **) &pyfuncs->PyList_GetItem          = dlsym(libpython_so, "PyList_GetItem");
    *(void **) &pyfuncs->PyList_New              = dlsym(libpython_so, "PyList_New");
    *(void **) &pyfuncs->PyList_SetItem          = dlsym(libpython_so, "PyList_SetItem");
    *(void **) &pyfuncs->PyList_Size             = dlsym(libpython_so, "PyList_Size");
    *(void **) &pyfuncs->PyLong_AsLong           = dlsym(libpython_so, "PyLong_AsLong");
    *(void **) &pyfuncs->PyLong_AsLongLong       = dlsym(libpython_so, "PyLong_AsLongLong");
    *(void **) &pyfuncs->PyLong_AsSize_t         = dlsym(libpython_so, "PyLong_AsSize_t");
    *(void **) &pyfuncs->PyLong_FromLong         = dlsym(libpython_so, "PyLong_FromLong");
    *(void **) &pyfuncs->PyLong_FromLongLong     = dlsym(libpython_so, "PyLong_FromLongLong");
    *(void **) &pyfuncs->PyModule_GetDict        = dlsym(libpython_so, "PyModule_GetDict");
    *(void **) &pyfuncs->PyObject_CallFunction   = dlsym(libpython_so, "PyObject_CallFunction");
    *(void **) &pyfuncs->PyObject_GetAttrString  = dlsym(libpython_so, "PyObject_GetAttrString");
    *(void **) &pyfuncs->PyObject_Init           = dlsym(libpython_so, "PyObject_Init");
    *(void **) &pyfuncs->PyObject_Malloc         = dlsym(libpython_so, "PyObject_Malloc");
    *(void **) &pyfuncs->PyObject_Type           = dlsym(libpython_so, "PyObject_Type");
    *(void **) &pyfuncs->PyTuple_GetItem         = dlsym(libpython_so, "PyTuple_GetItem");
    *(void **) &pyfuncs->PyTuple_New             = dlsym(libpython_so, "PyTuple_New");
    *(void **) &pyfuncs->PyTuple_SetItem         = dlsym(libpython_so, "PyTuple_SetItem");
    *(void **) &pyfuncs->PyTuple_Size            = dlsym(libpython_so, "PyTuple_Size");
    *(void **) &pyfuncs->PyType_FromSpec         = dlsym(libpython_so, "PyType_FromSpec");
    *(void **) &pyfuncs->PyUnicode_AsUTF8AndSize = dlsym(libpython_so, "PyUnicode_AsUTF8AndSize");
    *(void **) &pyfuncs->PyUnicode_FromString    = dlsym(libpython_so, "PyUnicode_FromString");
    *(void **) &pyfuncs->Py_CompileString        = dlsym(libpython_so, "Py_CompileString");
    *(void **) &pyfuncs->Py_DecRef               = dlsym(libpython_so, "Py_DecRef");
    *(void **) &pyfuncs->Py_Finalize             = dlsym(libpython_so, "Py_Finalize");
    *(void **) &pyfuncs->Py_IncRef               = dlsym(libpython_so, "Py_IncRef");
    *(void **) &pyfuncs->Py_Initialize           = dlsym(libpython_so, "Py_Initialize");
    return 1;
}

/*********************************************************************/
typedef struct {
    PyObject ob_base;
    FFPythonContext *py_ctx;
    size_t len;
    int32_t ptr[];
} py_ArrayOfInts;

static size_t array_of_ints_len(PyObject *self)
{
    py_ArrayOfInts *_self = (py_ArrayOfInts *) self;
    return _self->len;
}

static PyObject *array_of_ints_subscript(PyObject *self, PyObject *key)
{
    py_ArrayOfInts *_self = (py_ArrayOfInts *) self;
    FFPythonContext *py_ctx = _self->py_ctx;
    PythonFunctions *pyfuncs = &py_ctx->pyfuncs;
    size_t i = pyfuncs->PyLong_AsSize_t(key);
    if ( i == (size_t) -1 )
        return NULL;
    if ( i > _self->len )
    {
        pyfuncs->PyErr_SetString(py_ctx->PyExc_IndexError, "index out of range");
        return NULL;
    }
    return pyfuncs->PyLong_FromLong(_self->ptr[i]);
}

static int array_of_ints_ass_subscript(PyObject *self, PyObject *key, PyObject *value)
{
    py_ArrayOfInts *_self = (py_ArrayOfInts *) self;
    FFPythonContext *py_ctx = _self->py_ctx;
    PythonFunctions *pyfuncs = &py_ctx->pyfuncs;
    size_t i;
    int val;
    i = pyfuncs->PyLong_AsSize_t(key);
    if ( i == (size_t) -1 )
        return -1;
    if ( i > _self->len )
    {
        pyfuncs->PyErr_SetString(py_ctx->PyExc_IndexError, "index out of range");
        return -1;
    }
    val = pyfuncs->PyLong_AsLong(value);
    _self->ptr[i] = val;
    return 0;
}

static PyObject *array_of_ints_repr(PyObject *self)
{
    py_ArrayOfInts *_self = (py_ArrayOfInts *) self;
    FFPythonContext *py_ctx = _self->py_ctx;
    PythonFunctions *pyfuncs = &py_ctx->pyfuncs;
    AVBPrint bp;
    PyObject *ret;
    size_t len = _self->len;
    int32_t *ptr = _self->ptr;
    av_bprint_init(&bp, 1, AV_BPRINT_SIZE_UNLIMITED);
    av_bprint_chars(&bp, '[', 1);
    for ( size_t i = 0; i < len; i++ )
    {
        if ( i != 0 )
            av_bprint_chars(&bp, ',', 1);
        av_bprintf(&bp, "%d", ptr[i]);
    }
    av_bprint_chars(&bp, ']', 1);
    ret = pyfuncs->PyUnicode_FromString(bp.str);
    av_bprint_finalize(&bp, NULL);
    return ret;
}

static PyType_Slot py_ArrayOfInts_slots[] = {
    { Py_mp_ass_subscript, array_of_ints_ass_subscript },
    { Py_mp_length, array_of_ints_len },
    { Py_mp_subscript, array_of_ints_subscript },
    { Py_tp_repr, array_of_ints_repr },
    { 0, 0 },
};

static PyObject *new_ArrayOfInts(FFPythonContext *py_ctx, int32_t *ptr, size_t len)
{
    PythonFunctions *pyfuncs = &py_ctx->pyfuncs;
    void *obj = pyfuncs->PyObject_Malloc(sizeof(py_ArrayOfInts) + (len * sizeof(int32_t)));
    PyObject *py_obj = pyfuncs->PyObject_Init(obj, py_ctx->ArrayOfInts);
    py_ArrayOfInts *py_arr = (py_ArrayOfInts *) py_obj;
    py_arr->py_ctx = py_ctx;
    py_arr->len = len;
    memcpy(py_arr->ptr, ptr, len * sizeof(int32_t));
    return py_obj;
}

static PyType_Spec ArrayOfInts_spec = {
    "ArrayOfInts",
    sizeof(py_ArrayOfInts),
    0,
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    py_ArrayOfInts_slots
};

/*********************************************************************/
static size_t uint8carray_len(PyObject *self)
{
    py_Uint8FFPtr *_self = (py_Uint8FFPtr *) self;
    return _self->len;
}

static PyObject *uint8carray_subscript(PyObject *self, PyObject *key)
{
    py_Uint8FFPtr *_self = (py_Uint8FFPtr *) self;
    FFPythonContext *py_ctx = _self->py_ctx;
    PythonFunctions *pyfuncs = &py_ctx->pyfuncs;
    size_t i = pyfuncs->PyLong_AsSize_t(key);
    if ( i == (size_t) -1 )
        return NULL;
    if ( i > _self->len )
    {
        pyfuncs->PyErr_SetString(py_ctx->PyExc_IndexError, "index out of range");
        return NULL;
    }
    return pyfuncs->PyLong_FromLong(_self->ptr[i]);
}

static int uint8carray_ass_subscript(PyObject *self, PyObject *key, PyObject *value)
{
    py_Uint8FFPtr *_self = (py_Uint8FFPtr *) self;
    FFPythonContext *py_ctx = _self->py_ctx;
    PythonFunctions *pyfuncs = &py_ctx->pyfuncs;
    size_t i;
    size_t val;
    i = pyfuncs->PyLong_AsSize_t(key);
    if ( i == (size_t) -1 )
        return -1;
    if ( i > _self->len )
    {
        pyfuncs->PyErr_SetString(py_ctx->PyExc_IndexError, "index out of range");
        return -1;
    }
    val = pyfuncs->PyLong_AsSize_t(value);
    if ( val == (size_t) -1 )
        return -1;
    _self->ptr[i] = val;
    return 0;
}

static PyType_Slot py_Uint8FFPtr_slots[] = {
    { Py_mp_ass_subscript, (void *) uint8carray_ass_subscript },
    { Py_mp_length, (void *) uint8carray_len },
    { Py_mp_subscript, (void *) uint8carray_subscript },
    { 0, 0 },
};

static PyObject *new_Uint8FFPtr(FFPythonContext *py_ctx, size_t len, void *ptr)
{
    PythonFunctions *pyfuncs = &py_ctx->pyfuncs;
    PyObject *obj = (PyObject *) pyfuncs->PyObject_Malloc(sizeof(py_Uint8FFPtr));
    PyObject *py_obj = pyfuncs->PyObject_Init(obj, py_ctx->Uint8FFPtr);
    py_Uint8FFPtr *py_arr = (py_Uint8FFPtr *) py_obj;
    py_arr->py_ctx = py_ctx;
    py_arr->len = len;
    py_arr->ptr = (uint8_t *) ptr;
    return py_obj;
}

static PyType_Spec Uint8FFPtr_spec = {
    "Uint8FFPtr",
    sizeof(py_Uint8FFPtr),
    0,
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    py_Uint8FFPtr_slots
};

/*********************************************************************/
static PyType_Slot py_Opaque_slots[] = {
    { 0, 0 },
};

static PyObject *new_Opaque(FFPythonContext *py_ctx, void *ptr)
{
    PythonFunctions *pyfuncs = &py_ctx->pyfuncs;
    PyObject *obj = (PyObject *) pyfuncs->PyObject_Malloc(sizeof(py_Opaque));
    PyObject *py_obj = pyfuncs->PyObject_Init(obj, py_ctx->Opaque);
    py_Uint8FFPtr *py_arr = (py_Uint8FFPtr *) py_obj;
    py_arr->py_ctx = py_ctx;
    py_arr->ptr = ptr;
    return py_obj;
}

static PyType_Spec Opaque_spec = {
    "Opaque",
    sizeof(py_Opaque),
    0,
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    py_Opaque_slots
};

/*********************************************************************/
static PyObject *py_aoi_cache_get(FFPythonContext *py_ctx, int32_t *array_of_ints, size_t len)
{
    PythonFunctions *pyfuncs = &py_ctx->pyfuncs;
    PyObject *py_obj;
    if ( py_ctx->aoi_cache_offset[len] == py_ctx->aoi_cache_len[len] )
    {
        GROW_ARRAY(py_ctx->aoi_cache[len], py_ctx->aoi_cache_len[len]);
        py_obj = new_ArrayOfInts(py_ctx, array_of_ints, len);
        py_ctx->aoi_cache[len][py_ctx->aoi_cache_offset[len]++] = py_obj;
    }
    else
    {
        py_ArrayOfInts *py_arr;
        int32_t *ptr;
        py_obj = py_ctx->aoi_cache[len][py_ctx->aoi_cache_offset[len]++];
        py_arr = (py_ArrayOfInts *) py_obj;
        ptr = py_arr->ptr;
        for ( size_t i = 0; i < len; i++ )
            ptr[i] = array_of_ints[i];
    }
    pyfuncs->Py_IncRef(py_obj);
    return py_obj;
}

/*********************************************************************/
int ff_python_call_func(FFPythonContext *py_ctx, FFPythonObject **ppy_ret, FFPythonObject *py_func, va_list vl)
{
    PythonFunctions *pyfuncs = &py_ctx->pyfuncs;
    PyObject *py_args;
    PyObject *pyret;
    FFPythonObject **argv = NULL;
    int argc = 0;
    int ret = 0;
    while ( 42 )
    {
        FFPythonObject *arg = va_arg(vl, FFPythonObject *);
        if ( arg == NULL )
            break;
        GROW_ARRAY(argv, argc);
        argv[argc-1] = arg;
    }
    py_args = pyfuncs->PyTuple_New(argc);
    for ( size_t i = 0; i < argc; i++ )
    {
        pyfuncs->Py_IncRef(argv[i]);
        pyfuncs->PyTuple_SetItem(py_args, i, argv[i]);
    }
    pyret = pyfuncs->PyObject_CallFunction((PyObject *) py_func, "O", py_args);
    pyfuncs->Py_DecRef(py_args);
    if ( pyret == NULL )
    {
        pyfuncs->PyErr_Print();
        ret = -1;
    }
    else
    {
        if ( ppy_ret != NULL )
            *ppy_ret = (FFPythonObject *) pyret;
        else
            pyfuncs->Py_DecRef(pyret);
    }
    av_free(argv);
    return ret;
}

/*********************************************************************/
static PyObject *python_from_json(FFPythonContext *py_ctx, json_t *jso)
{
    PythonFunctions *pyfuncs = &py_ctx->pyfuncs;
    PyObject *val = NULL;
    if ( jso == NULL )
        return Py_None_New(py_ctx);
    switch ( JSON_TYPE(jso->flags) )
    {
    case JSON_TYPE_OBJECT:
        {
            size_t len = json_object_length(jso);
            json_kvp_t *kvps = jso->obj->kvps;
            val = pyfuncs->PyDict_New();
            for ( size_t i = 0; i < len; i++ )
            {
                PyObject *val_i = python_from_json(py_ctx, kvps[i].value);
                pyfuncs->PyDict_SetItemString(val, kvps[i].key, val_i);
                pyfuncs->Py_DecRef(val_i);
            }
        }
        break;
    case JSON_TYPE_ARRAY:
        {
            size_t len = json_array_length(jso);
            json_t **data = jso->arr->data;
            val = pyfuncs->PyList_New(len);
            for ( size_t i = 0; i < len; i++ )
            {
                PyObject *val_i = python_from_json(py_ctx, data[i]);
                pyfuncs->PyList_SetItem(val, i, val_i);
            }
        }
        break;
    case JSON_TYPE_ARRAY_OF_INTS:
        {
            size_t len = json_array_length(jso);
            int32_t *array_of_ints = jso->array_of_ints;
            if ( len < MAX_AOI_CACHE_LEN && (py_ctx->flags & FFSCRIPT_FLAGS_AOI_CACHE) != 0 )
                val = py_aoi_cache_get(py_ctx, array_of_ints, len);
            else
                val = new_ArrayOfInts(py_ctx, array_of_ints, len);
        }
        break;
    case JSON_TYPE_MV_2DARRAY:
        {
            json_mv2darray_t *mv2d = jso->mv2darray;
            const size_t width = mv2d->width;
            const size_t height = mv2d->height;
            const uint8_t *nb_blocks_array = mv2d->nb_blocks_array;
            val = pyfuncs->PyList_New(height);
            for ( size_t i = 0; i < height; i++ )
            {
                PyObject *val_i = pyfuncs->PyList_New(width);
                for ( size_t j = 0; j < width; j++ )
                {
                    size_t idx = i * width + j;
                    uint8_t nb_blocks = nb_blocks_array[idx];
                    PyObject *val_j;
                    if ( nb_blocks == 0 )
                    {
                        val_j = Py_None_New(py_ctx);
                    }
                    else if ( nb_blocks == 1 )
                    {
                        int32_t *mv = &mv2d->mvs[0][idx << 1];
                        val_j = py_aoi_cache_get(py_ctx, mv, 2);
                    }
                    else
                    {
                        val_j = pyfuncs->PyList_New(nb_blocks);
                        for ( size_t k = 0; k < nb_blocks; k++ )
                        {
                            int32_t *mv = &mv2d->mvs[k][idx << 1];
                            PyObject *val_k = py_aoi_cache_get(py_ctx, mv, 2);
                            pyfuncs->PyList_SetItem(val_j, k, val_k);
                        }
                    }
                    pyfuncs->PyList_SetItem(val_i, j, val_j);
                }
                pyfuncs->PyList_SetItem(val, i, val_i);
            }
        }
        break;
    case JSON_TYPE_STRING:
        val = pyfuncs->PyUnicode_FromString(jso->str);
        break;
    case JSON_TYPE_NUMBER:
        if ( jso->val == JSON_NULL )
            val = Py_None_New(py_ctx);
        else
            val = pyfuncs->PyLong_FromLongLong(jso->val);
        break;
    case JSON_TYPE_BOOL:
        val = pyfuncs->PyBool_FromLong(jso->val);
        break;
    }
    if ( val == NULL )
        val = Py_None_New(py_ctx);
    return val;
}

/*********************************************************************/
static json_t *python_to_json(json_ctx_t *jctx, FFPythonContext *py_ctx, PyObject *val)
{
    PythonFunctions *pyfuncs = &py_ctx->pyfuncs;
    if ( val->ob_type == py_ctx->PyLong_Type )
        return json_int_new(jctx, pyfuncs->PyLong_AsLongLong(val));
    if ( val->ob_type == py_ctx->PyUnicode_Type )
        return json_string_new(jctx, pyfuncs->PyUnicode_AsUTF8AndSize(val, NULL));
    if ( val->ob_type == py_ctx->PyBool_Type )
        return json_bool_new(jctx, (val == py_ctx->Py_True));
    if ( val->ob_type == py_ctx->ArrayOfInts )
    {
        py_ArrayOfInts *array_of_ints = (py_ArrayOfInts *) val;
        size_t len = array_of_ints->len;
        json_t *array;

        array = json_array_of_ints_new(jctx, len);
        memcpy(array->array_of_ints, array_of_ints->ptr, len * sizeof(int32_t));

        return array;
    }
    if ( val->ob_type == py_ctx->PyList_Type )
    {
        int is_array_of_ints = 0;
        size_t length = pyfuncs->PyList_Size(val);
        json_t *array;

        for ( size_t i = 0; i < length; i++ )
        {
            PyObject *val_i = pyfuncs->PyList_GetItem(val, i);
            is_array_of_ints = (val_i->ob_type == py_ctx->PyLong_Type);
            if ( !is_array_of_ints )
                break;
        }

        if ( is_array_of_ints )
        {
            int32_t *array_of_ints;
            array = json_array_of_ints_new(jctx, length);
            array_of_ints = array->array_of_ints;
            for ( size_t i = 0; i < length; i++ )
            {
                PyObject *val_i = pyfuncs->PyList_GetItem(val, i);
                array_of_ints[i] = pyfuncs->PyLong_AsLong(val_i);
            }
        }
        else
        {
            json_t **data;
            array = json_array_new_uninit(jctx, length);
            data = array->arr->data;
            for ( size_t i = 0; i < length; i++ )
            {
                PyObject *val_i = pyfuncs->PyList_GetItem(val, i);
                data[i] = python_to_json(jctx, py_ctx, val_i);
            }
        }

        return array;
    }
    if ( val->ob_type == py_ctx->PyDict_Type )
    {
        json_t *object = json_object_new(jctx);
        PyObject *key;
        PyObject *value;
        size_t pos = 0;
        while ( pyfuncs->PyDict_Next(val, &pos, &key, &value) )
        {
            const char *str = pyfuncs->PyUnicode_AsUTF8AndSize(key, NULL);
            json_object_add(object, str, python_to_json(jctx, py_ctx, value));
        }
        json_object_done(jctx, object);
        return object;
    }
    return NULL;
}

/*********************************************************************/
FFPythonContext *ff_python_init(const char *script_fname, int flags)
{
    FFPythonContext *py_ctx = av_mallocz(sizeof(FFPythonContext));
    PythonFunctions *pyfuncs = &py_ctx->pyfuncs;
    PyObject *pysrc;
    PyObject *builtins;
    uint8_t *buf = NULL;
    size_t len;
    int ret = AVERROR(EINVAL);

    /* init context */
    py_ctx->class = &python_class;
    py_ctx->script_fname = script_fname;
    py_ctx->script_is_py = 1;
    py_ctx->flags = flags;

    py_ctx->libpython_so = dlopen_python();
    if ( py_ctx->libpython_so == NULL )
    {
        av_log(py_ctx, AV_LOG_FATAL, "Could not find libpython in the usual places.\n");
        av_log(py_ctx, AV_LOG_FATAL, "Try specifying the path to libpython using the FFGLITCH_LIBPYTHON_PATH environment variable.\n");
        goto end;
    }

    if ( !dlsyms_python(&py_ctx->pyfuncs, py_ctx->libpython_so) )
    {
        av_log(py_ctx, AV_LOG_FATAL, "Could not load all necessary functions from libpython.\n");
        goto end;
    }

    /* load file */
    buf = ff_script_read_file(script_fname, &len);
    if ( buf == NULL )
    {
        av_log(py_ctx, AV_LOG_FATAL, "Could not open script file %s\n", script_fname);
        goto end;
    }

    pyfuncs->Py_Initialize();

    pysrc = pyfuncs->Py_CompileString((const char *) buf, script_fname, Py_file_input);
    if ( pysrc == NULL )
    {
        av_log(py_ctx, AV_LOG_FATAL, "Could not compile script file %s\n", script_fname);
        pyfuncs->PyErr_Print();
        goto end;
    }

    py_ctx->numpy = pyfuncs->PyImport_ImportModule("numpy");
    if ( py_ctx->numpy == NULL )
    {
        av_log(py_ctx, AV_LOG_FATAL, "Could not import numpy\n");
        pyfuncs->PyErr_Print();
        goto end;
    }

    py_ctx->module = pyfuncs->PyImport_ExecCodeModule("ffglitch", pysrc);
    if ( py_ctx->module == NULL )
    {
        av_log(py_ctx, AV_LOG_FATAL, "Could not eval script file %s\n", script_fname);
        pyfuncs->PyErr_Print();
        goto end;
    }
    pyfuncs->Py_DecRef(pysrc);

    /* custom ffglitch types */
    py_ctx->ArrayOfInts    = pyfuncs->PyType_FromSpec(&ArrayOfInts_spec);
    py_ctx->Opaque         = pyfuncs->PyType_FromSpec(&Opaque_spec);
    py_ctx->Uint8FFPtr     = pyfuncs->PyType_FromSpec(&Uint8FFPtr_spec);
    py_ctx->new_Opaque     = new_Opaque;
    py_ctx->new_Uint8FFPtr = new_Uint8FFPtr;

    builtins = pyfuncs->PyEval_GetBuiltins();
    py_ctx->PyBool_Type      = pyfuncs->PyDict_GetItemString(builtins, "bool");
    py_ctx->PyDict_Type      = pyfuncs->PyDict_GetItemString(builtins, "dict");
    py_ctx->PyExc_IndexError = pyfuncs->PyDict_GetItemString(builtins, "IndexError");
    py_ctx->PyExc_TypeError  = pyfuncs->PyDict_GetItemString(builtins, "TypeError");
    py_ctx->PyList_Type      = pyfuncs->PyDict_GetItemString(builtins, "list");
    py_ctx->PyLong_Type      = pyfuncs->PyDict_GetItemString(builtins, "int");
    py_ctx->PyUnicode_Type   = pyfuncs->PyDict_GetItemString(builtins, "str");
    py_ctx->Py_None          = pyfuncs->PyDict_GetItemString(builtins, "None");
    py_ctx->Py_True          = pyfuncs->PyDict_GetItemString(builtins, "True");
    pyfuncs->Py_DecRef(builtins);

    py_ctx->PyNone_Type      = pyfuncs->PyObject_Type(py_ctx->Py_None);

    ret = 0;

end:
    av_free(buf);

    if ( ret != 0 )
        av_freep(&py_ctx);

    return py_ctx;
}

/*********************************************************************/
void ff_python_uninit(FFPythonContext **ppy_ctx)
{
    FFPythonContext *py_ctx = *ppy_ctx;
    PythonFunctions *pyfuncs = &py_ctx->pyfuncs;

    /* free array_of_ints cache */
    for ( size_t i = 0; i < MAX_AOI_CACHE_LEN; i++ )
    {
        for ( size_t j = 0; j < py_ctx->aoi_cache_len[i]; j++ )
            pyfuncs->Py_DecRef(py_ctx->aoi_cache[i][j]);
        av_freep(&py_ctx->aoi_cache[i]);
    }

    /* TODO atomic count */
    pyfuncs->Py_Finalize();
    dlclose(py_ctx->libpython_so);
    av_freep(ppy_ctx);
}

/*********************************************************************/
FFPythonObject *ff_python_get_func(FFPythonContext *py_ctx, const char *func_name, int required)
{
    PythonFunctions *pyfuncs = &py_ctx->pyfuncs;
    PyObject *py_func = pyfuncs->PyObject_GetAttrString(py_ctx->module, func_name);
    if ( py_func == NULL
      || !pyfuncs->PyCallable_Check(py_func) )
    {
        if ( required )
        {
            av_log(py_ctx, AV_LOG_FATAL, "Could not find function %s() in %s\n", func_name, py_ctx->script_fname);
            pyfuncs->PyErr_Print();
        }
        else
        {
            pyfuncs->PyErr_Clear();
        }
        if ( py_func != NULL )
            pyfuncs->Py_DecRef(py_func);
        py_func = NULL;
    }
    return (FFPythonObject *) py_func;
}

/*********************************************************************/
FFPythonObject *ff_python_from_json(FFPythonContext *py_ctx, json_t *jso)
{
    PyObject *py_obj;
    if ( (py_ctx->flags & FFSCRIPT_FLAGS_AOI_CACHE) != 0 )
        memset(py_ctx->aoi_cache_offset, 0x00, sizeof(py_ctx->aoi_cache_offset));
    py_obj = python_from_json(py_ctx, jso);
    return (FFPythonObject *) py_obj;
}

/*********************************************************************/
json_t *ff_python_to_json(json_ctx_t *jctx, FFPythonContext *py_ctx, FFPythonObject *val)
{
    return python_to_json(jctx, py_ctx, (PyObject *) val);
}

/*********************************************************************/
void ff_python_free_obj(FFPythonContext *py_ctx, FFPythonObject *py_obj)
{
    PythonFunctions *pyfuncs = &py_ctx->pyfuncs;
    pyfuncs->Py_DecRef((PyObject *) py_obj);
}
