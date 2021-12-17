#ifdef _WIN32
  #include "compat/w32dlfcn.h"
  #include "shlobj.h"
#else
  #include <dlfcn.h>
#endif

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
            "/Library/Frameworks/Python.framework/Versions/3.10/lib/libpython3.10.dylib",
            "/Library/Frameworks/Python.framework/Versions/3.9/lib/libpython3.9.dylib",
            "/Library/Frameworks/Python.framework/Versions/3.8/lib/libpython3.8.dylib",
            "/Library/Frameworks/Python.framework/Versions/3.7/lib/libpython3.7.dylib",
            "/Library/Frameworks/Python.framework/Versions/3.6/lib/libpython3.6.dylib",
            "/Library/Frameworks/Python.framework/Versions/3.5/lib/libpython3.5.dylib",
#else
            "libpython3.10m.so",
            "libpython3.9m.so",
            "libpython3.8m.so",
            "libpython3.7m.so",
            "libpython3.6m.so",
            "libpython3.5m.so",
#endif
        };
        for ( int i = 0; i < FF_ARRAY_ELEMS(paths) && (libpython_so == NULL); i++ )
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
#define Py_single_input 256
#define Py_file_input   257

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
    void        (*PyErr_Print)(void);
    void        (*PyErr_SetString)(PyObject *type, const char *message);
    PyObject   *(*PyEval_EvalCode)(PyObject *co, PyObject *globals, PyObject *locals);
    PyObject   *(*PyEval_GetBuiltins)(void);
    PyObject   *(*PyFloat_FromDouble)(double v);
    PyObject   *(*PyImport_AddModule)(const char *name);
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
    PyObject   *(*PyType_FromSpec)(PyType_Spec *spec);
    const char *(*PyUnicode_AsUTF8AndSize)(PyObject *unicode, Py_ssize_t *size);
    PyObject   *(*PyUnicode_FromString)(const char *u);
    PyObject   *(*Py_CompileString)(const char *str, const char *filename, int start);
    void        (*Py_DecRef)(PyObject *o);
    void        (*Py_Finalize)(void);
    void        (*Py_IncRef)(PyObject *o);
    void        (*Py_Initialize)(void);
} PythonFunctions;

static int dlsyms_python(PythonFunctions *pyfuncs, void *libpython_so)
{
    pyfuncs->PyBool_FromLong         = dlsym(libpython_so, "PyBool_FromLong");
    pyfuncs->PyCallable_Check        = dlsym(libpython_so, "PyCallable_Check");
    pyfuncs->PyDict_GetItemString    = dlsym(libpython_so, "PyDict_GetItemString");
    pyfuncs->PyDict_New              = dlsym(libpython_so, "PyDict_New");
    pyfuncs->PyDict_Next             = dlsym(libpython_so, "PyDict_Next");
    pyfuncs->PyDict_SetItemString    = dlsym(libpython_so, "PyDict_SetItemString");
    pyfuncs->PyErr_Print             = dlsym(libpython_so, "PyErr_Print");
    pyfuncs->PyErr_SetString         = dlsym(libpython_so, "PyErr_SetString");
    pyfuncs->PyEval_EvalCode         = dlsym(libpython_so, "PyEval_EvalCode");
    pyfuncs->PyEval_GetBuiltins      = dlsym(libpython_so, "PyEval_GetBuiltins");
    pyfuncs->PyFloat_FromDouble      = dlsym(libpython_so, "PyFloat_FromDouble");
    pyfuncs->PyImport_AddModule      = dlsym(libpython_so, "PyImport_AddModule");
    pyfuncs->PyList_GetItem          = dlsym(libpython_so, "PyList_GetItem");
    pyfuncs->PyList_New              = dlsym(libpython_so, "PyList_New");
    pyfuncs->PyList_SetItem          = dlsym(libpython_so, "PyList_SetItem");
    pyfuncs->PyList_Size             = dlsym(libpython_so, "PyList_Size");
    pyfuncs->PyLong_AsLong           = dlsym(libpython_so, "PyLong_AsLong");
    pyfuncs->PyLong_AsLongLong       = dlsym(libpython_so, "PyLong_AsLongLong");
    pyfuncs->PyLong_AsSize_t         = dlsym(libpython_so, "PyLong_AsSize_t");
    pyfuncs->PyLong_FromLong         = dlsym(libpython_so, "PyLong_FromLong");
    pyfuncs->PyLong_FromLongLong     = dlsym(libpython_so, "PyLong_FromLongLong");
    pyfuncs->PyModule_GetDict        = dlsym(libpython_so, "PyModule_GetDict");
    pyfuncs->PyObject_CallFunction   = dlsym(libpython_so, "PyObject_CallFunction");
    pyfuncs->PyObject_GetAttrString  = dlsym(libpython_so, "PyObject_GetAttrString");
    pyfuncs->PyObject_Init           = dlsym(libpython_so, "PyObject_Init");
    pyfuncs->PyObject_Malloc         = dlsym(libpython_so, "PyObject_Malloc");
    pyfuncs->PyType_FromSpec         = dlsym(libpython_so, "PyType_FromSpec");
    pyfuncs->PyUnicode_AsUTF8AndSize = dlsym(libpython_so, "PyUnicode_AsUTF8AndSize");
    pyfuncs->PyUnicode_FromString    = dlsym(libpython_so, "PyUnicode_FromString");
    pyfuncs->Py_CompileString        = dlsym(libpython_so, "Py_CompileString");
    pyfuncs->Py_DecRef               = dlsym(libpython_so, "Py_DecRef");
    pyfuncs->Py_Finalize             = dlsym(libpython_so, "Py_Finalize");
    pyfuncs->Py_IncRef               = dlsym(libpython_so, "Py_IncRef");
    pyfuncs->Py_Initialize           = dlsym(libpython_so, "Py_Initialize");
    return 1;
}
