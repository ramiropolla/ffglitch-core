/*
 * Copyright (C) 2020-2022 Ramiro Polla
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

#ifndef AVUTIL_SCRIPT_PYTHON_H
#define AVUTIL_SCRIPT_PYTHON_H

/*********************************************************************/
#include <stdarg.h>

/*********************************************************************/
#include "libavutil/json.h"
#include "libavutil/log.h"

/*********************************************************************/
#define Py_single_input 256
#define Py_file_input   257

#define Py_mp_ass_subscript 3
#define Py_mp_length 4
#define Py_mp_subscript 5
#define Py_tp_repr 66

#define Py_TPFLAGS_BASETYPE         (1UL << 10)
#define Py_TPFLAGS_HAVE_VERSION_TAG (1UL << 18)
#define Py_TPFLAGS_DEFAULT          Py_TPFLAGS_HAVE_VERSION_TAG

typedef struct PyObject {
  size_t ob_refcnt;
  void *ob_type;
} PyObject;

typedef size_t Py_ssize_t;

typedef struct {
    int slot;    /* slot id, see below */
    void *pfunc; /* function pointer */
} PyType_Slot;

typedef struct {
    const char *name;
    int basicsize;
    int itemsize;
    unsigned int flags;
    PyType_Slot *slots; /* terminated by slot==0. */
} PyType_Spec;

typedef struct PythonFunctions {
    PyObject   *(*PyBool_FromLong)(long v);
    int         (*PyCallable_Check)(PyObject *o);
    PyObject   *(*PyDict_GetItemString)(PyObject *p, const char *key);
    PyObject   *(*PyDict_New)(void);
    int         (*PyDict_Next)(PyObject *p, size_t *ppos, PyObject **pkey, PyObject **pvalue);
    int         (*PyDict_SetItemString)(PyObject *p, const char *key, PyObject *val);
    void        (*PyErr_Clear)(void);
    void        (*PyErr_Print)(void);
    void        (*PyErr_SetString)(PyObject *type, const char *message);
    PyObject   *(*PyEval_GetBuiltins)(void);
    PyObject   *(*PyFloat_FromDouble)(double v);
    PyObject   *(*PyImport_ExecCodeModule)(const char *name, PyObject *co);
    PyObject   *(*PyImport_ImportModule)(const char *name);
    int         (*PyList_Append)(PyObject *list, PyObject *item);
    PyObject   *(*PyList_GetItem)(PyObject *list, Py_ssize_t index);
    PyObject   *(*PyList_New)(Py_ssize_t len);
    int         (*PyList_SetItem)(PyObject *list, Py_ssize_t index, PyObject *item);
    size_t      (*PyList_Size)(PyObject *list);
    long        (*PyLong_AsLong)(PyObject *obj);
    long long   (*PyLong_AsLongLong)(PyObject *obj);
    size_t      (*PyLong_AsSize_t)(PyObject *obj);
    PyObject   *(*PyLong_FromLong)(long v);
    PyObject   *(*PyLong_FromLongLong)(long long v);
    PyObject   *(*PyModule_GetDict)(PyObject *module);
    PyObject   *(*PyObject_CallFunction)(PyObject *callable, const char *format, ...);
    PyObject   *(*PyObject_GetAttrString)(PyObject *o, const char *attr_name);
    PyObject   *(*PyObject_Init)(PyObject *op, PyObject *type);
    void       *(*PyObject_Malloc)(size_t n);
    PyObject   *(*PyObject_Type)(PyObject *o);
    PyObject   *(*PyTuple_GetItem)(PyObject *p, Py_ssize_t pos);
    PyObject   *(*PyTuple_New)(Py_ssize_t len);
    int        *(*PyTuple_SetItem)(PyObject *p, Py_ssize_t pos, PyObject *o);
    Py_ssize_t  (*PyTuple_Size)(PyObject *p);
    PyObject   *(*PyType_FromSpec)(PyType_Spec *spec);
    const char *(*PyUnicode_AsUTF8AndSize)(PyObject *unicode, Py_ssize_t *size);
    PyObject   *(*PyUnicode_FromString)(const char *u);
    PyObject   *(*Py_CompileString)(const char *str, const char *filename, int start);
    void        (*Py_DecRef)(PyObject *o);
    void        (*Py_Finalize)(void);
    void        (*Py_IncRef)(PyObject *o);
    void        (*Py_Initialize)(void);
} PythonFunctions;

/*********************************************************************/
typedef struct FFPythonContext {
    /* common from FFScriptContext */
    const AVClass *class;
    const char *script_fname;
    int script_is_py;
    int script_is_js;
    int flags;

    /* python */
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

    PyObject *PyNone_Type;

    PyObject *ArrayOfInts;
    PyObject *Opaque;
    PyObject *Uint8FFPtr;
    PyObject *(*new_Uint8FFPtr)(struct FFPythonContext *, size_t, void *);
    PyObject *(*new_Opaque)(struct FFPythonContext *, void *);

    PyObject *module;
    PyObject *numpy;

    /* array_of_ints cache */
#define MAX_AOI_CACHE_LEN 256
    PyObject **aoi_cache[MAX_AOI_CACHE_LEN];
    int aoi_cache_len[MAX_AOI_CACHE_LEN];
    int aoi_cache_offset[MAX_AOI_CACHE_LEN];
} FFPythonContext;

typedef PyObject FFPythonObject;

/*********************************************************************/
typedef struct {
    PyObject ob_base;
    FFPythonContext *py_ctx;
    uint8_t *ptr;
    size_t len;
} py_Uint8FFPtr;

/*********************************************************************/
typedef struct {
    PyObject ob_base;
    FFPythonContext *py_ctx;
    void *ptr;
} py_Opaque;

/*********************************************************************/
FFPythonContext *ff_python_init(const char *script_fname, int flags);
void ff_python_uninit(FFPythonContext **ppy_ctx);

FFPythonObject *ff_python_get_func(FFPythonContext *py_ctx, const char *func_name, int required);
int ff_python_call_func(FFPythonContext *py_ctx, FFPythonObject **ppy_ret, FFPythonObject *py_func, va_list vl);

FFPythonObject *ff_python_from_json(FFPythonContext *py_ctx, json_t *jso);
json_t *ff_python_to_json(json_ctx_t *jctx, FFPythonContext *py_ctx, FFPythonObject *val);
void ff_python_free_obj(FFPythonContext *py_ctx, FFPythonObject *py_obj);

/*********************************************************************/
static inline PyObject *Py_None_New(FFPythonContext *py_ctx)
{
    PythonFunctions *pyfuncs = &py_ctx->pyfuncs;
    pyfuncs->Py_IncRef(py_ctx->Py_None);
    return py_ctx->Py_None;
}

#endif
