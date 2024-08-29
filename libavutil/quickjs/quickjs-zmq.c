/*
 * Copyright (C) 2023-2024 Ramiro Polla
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*********************************************************************/
#include <string.h>
#include <limits.h>
#include <assert.h>
#define ZMQ_BUILD_DRAFT_API 1
#include "zmq.h"

/*********************************************************************/
#include "quickjs-zmq.h"
#include "cutils.h"

/*********************************************************************/
typedef struct zmq_ctx_t zmq_ctx_t;
typedef struct zmq_socket_t zmq_socket_t;

/*********************************************************************/
typedef struct ZMQContext ZMQContext;
typedef struct ZMQSocket ZMQSocket;
typedef struct ZMQPoller ZMQPoller;

struct ZMQSocket
{
    zmq_socket_t *zsocket;
    ZMQContext *zmqcontext;
    int ref_count;
    int closed;
    int index;
};

struct ZMQContext
{
    zmq_ctx_t *zctx;
    ZMQSocket **zmqsockets;
    int      nb_zmqsockets;
    int ref_count;
    int terminated;
};

typedef struct PollerEntry PollerEntry;
struct PollerEntry
{
    JSValue socket;
    JSValue user_data;
    zmq_socket_t *zsocket;
};

struct ZMQPoller
{
    void *zpoller;
    PollerEntry **entries;
    int        nb_entries;
};

/*********************************************************************/
static void js_ZMQContext_decref(JSRuntime *rt, ZMQContext *zmqcontext, int free_sockets);
static void js_ZMQSocket_decref(JSRuntime *rt, ZMQSocket *zmqsocket);

static void
js_ZMQContext_decref(JSRuntime *rt, ZMQContext *zmqcontext, int free_sockets)
{
    if ( --zmqcontext->ref_count == 0 )
    {
        // free all sockets if requested
        if ( free_sockets )
            for ( int i = 0; i < zmqcontext->nb_zmqsockets; i++ )
                js_ZMQSocket_decref(rt, zmqcontext->zmqsockets[i]);
        // terminate the context
        if ( !zmqcontext->terminated )
            zmq_ctx_term(zmqcontext->zctx);
        js_free_rt(rt, zmqcontext->zmqsockets);
        js_free_rt(rt, zmqcontext);
    }
}

static void
js_ZMQSocket_decref(JSRuntime *rt, ZMQSocket *zmqsocket)
{
    if ( zmqsocket != NULL && --zmqsocket->ref_count == 0 )
    {
        ZMQContext *zmqcontext = zmqsocket->zmqcontext;
        // close socket if it hasn't been closed yet
        if ( zmqsocket->closed == 0 )
        {
            int zero = 0;
            zmq_socket_t *zsocket = zmqsocket->zsocket;
            zmq_setsockopt(zsocket, ZMQ_LINGER, &zero, sizeof(zero));
            zmq_close(zsocket);
        }
        zmqcontext->zmqsockets[zmqsocket->index] = NULL;
        js_ZMQContext_decref(rt, zmqcontext, 0);
        js_free_rt(rt, zmqsocket);
    }
}

/*********************************************************************/
static JSClassID js_ZMQContext_class_id;
static JSClassID js_ZMQPoller_class_id;
static JSClassID js_ZMQSocket_class_id;

/*********************************************************************/
static JSValue
js_ZMQContext_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
    JSValue proto;
    JSValue obj;
    JSRuntime *rt;
    ZMQContext *zmqcontext;

    /* using new_target to get the prototype is necessary when the
       class is extended. */
    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if ( JS_IsException(proto) )
        return proto;
    obj = JS_NewObjectProtoClass(ctx, proto, js_ZMQContext_class_id);
    JS_FreeValue(ctx, proto);
    if ( JS_IsException(obj) )
        return obj;

    rt = JS_GetRuntime(ctx);
    zmqcontext = (ZMQContext *) js_mallocz_rt(rt, sizeof(ZMQContext));
    if ( zmqcontext == NULL )
        return JS_ThrowOutOfMemory(ctx);
    zmqcontext->zctx = zmq_ctx_new();
    zmqcontext->ref_count = 1;
    JS_SetOpaque(obj, zmqcontext);

    return obj;
}

static void
js_ZMQContext_finalizer(JSRuntime *rt, JSValue val)
{
    ZMQContext *zmqcontext = (ZMQContext *) JS_GetOpaque(val, js_ZMQContext_class_id);
    js_ZMQContext_decref(rt, zmqcontext, 1);
}

static JSClassDef js_ZMQContext_class = {
    "ZMQContext",
    .finalizer = js_ZMQContext_finalizer,
};

/*********************************************************************/
static JSValue
js_ZMQPoller_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
    JSValue proto;
    JSValue obj;
    JSRuntime *rt;
    ZMQPoller *zmqpoller;

    /* using new_target to get the prototype is necessary when the
       class is extended. */
    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if ( JS_IsException(proto) )
        return proto;
    obj = JS_NewObjectProtoClass(ctx, proto, js_ZMQPoller_class_id);
    JS_FreeValue(ctx, proto);
    if ( JS_IsException(obj) )
        return obj;

    rt = JS_GetRuntime(ctx);
    zmqpoller = (ZMQPoller *) js_mallocz_rt(rt, sizeof(ZMQPoller));
    if ( zmqpoller == NULL )
        return JS_ThrowOutOfMemory(ctx);
    zmqpoller->zpoller = zmq_poller_new();
    JS_SetOpaque(obj, zmqpoller);

    return obj;
}

static void
free_PollerEntry(JSRuntime *rt, PollerEntry **pentry, zmq_socket_t *zsocket)
{
    PollerEntry *entry = *pentry;
    if ( entry != NULL
      && (zsocket == NULL || zsocket == entry->zsocket) )
    {
        JS_FreeValueRT(rt, entry->socket);
        JS_FreeValueRT(rt, entry->user_data);
        js_free_rt(rt, entry);
        *pentry = NULL;
    }
}

static void
js_ZMQPoller_finalizer(JSRuntime *rt, JSValue val)
{
    ZMQPoller *zmqpoller = (ZMQPoller *) JS_GetOpaque(val, js_ZMQPoller_class_id);
    zmq_poller_destroy(&zmqpoller->zpoller);
    for ( int i = 0; i < zmqpoller->nb_entries; i++ )
        free_PollerEntry(rt, &zmqpoller->entries[i], NULL);
    js_free_rt(rt, zmqpoller->entries);
    js_free_rt(rt, zmqpoller);
}

static void
js_ZMQPoller_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func)
{
    ZMQPoller *zmqpoller = (ZMQPoller *) JS_GetOpaque(val, js_ZMQPoller_class_id);
    for ( int i = 0; i < zmqpoller->nb_entries; i++ )
    {
        PollerEntry *entry = zmqpoller->entries[i];
        if ( entry != NULL )
        {
            JS_MarkValue(rt, entry->socket, mark_func);
            JS_MarkValue(rt, entry->user_data, mark_func);
        }
    }
}

static JSClassDef js_ZMQPoller_class = {
    "ZMQPoller",
    .finalizer = js_ZMQPoller_finalizer,
    .gc_mark = js_ZMQPoller_mark,
};

/*********************************************************************/
static void
js_ZMQSocket_finalizer(JSRuntime *rt, JSValue val)
{
    ZMQSocket *zmqsocket = (ZMQSocket *) JS_GetOpaque(val, js_ZMQSocket_class_id);
    js_ZMQSocket_decref(rt, zmqsocket);
}

static JSClassDef js_ZMQSocket_class = {
    "ZMQSocket",
    .finalizer = js_ZMQSocket_finalizer,
};

/*********************************************************************/
static JSValue
js_ZMQ_version(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSValue ret;
    int32_t *ptr;
    ret = JS_NewInt32FFArray(ctx, &ptr, 3, 0);
    zmq_version(&ptr[0], &ptr[1], &ptr[2]);
    return ret;
}

static JSValue
js_ZMQ_has(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSValue jcapability = argv[0];
    const char *capability;
    JSValue ret;
    if ( !JS_IsString(jcapability) )
        return JS_ThrowTypeError(ctx, "int ZMQ.has(const char *capability)");
    capability = JS_ToCString(ctx, jcapability);
    ret = JS_NewInt32(ctx, zmq_has(capability));
    JS_FreeCString(ctx, capability);
    return ret;
}

/*********************************************************************/
static JSValue
ThrowZMQError(JSContext *ctx)
{
    JSValue ret = JS_NewError(ctx);
    JS_DefinePropertyValueStr(ctx, ret, "message",
                              JS_NewString(ctx, zmq_strerror(zmq_errno())),
                              JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
    if ( JS_IsException(ret) )
        ret = JS_NULL;
    return JS_Throw(ctx, ret);
}

static JSValue
js_ZMQContext_socket(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQContext *zmqcontext = (ZMQContext *) JS_GetOpaque(this_val, js_ZMQContext_class_id);
    JSValue jtype = argv[0];
    int32_t type;
    JSValue obj = JS_NULL;
    zmq_socket_t *zsocket = NULL;
    JSRuntime *rt = JS_GetRuntime(ctx);
    ZMQSocket *zmqsocket = NULL;
    int zmqsocket_index;
    ZMQSocket **new_zmqsockets;

    // parse arguments
    if ( !JS_IsInt32(jtype) )
        return JS_ThrowTypeError(ctx, "ZMQSocket ZMQContext.socket(int type)");
    type = JS_VALUE_GET_INT(jtype);

    // create new object
    obj = JS_NewObjectClass(ctx, js_ZMQSocket_class_id);
    if ( JS_IsException(obj) )
        return obj;

    // create ZMQ socket
    zsocket = zmq_socket(zmqcontext->zctx, type);
    if ( zsocket == NULL )
    {
        JS_FreeValue(ctx, obj);
        return ThrowZMQError(ctx);
    }

    // create context and set opaque
    zmqsocket = (ZMQSocket *) js_mallocz_rt(rt, sizeof(ZMQSocket));
    if ( zmqsocket == NULL )
        goto out_of_memory;
    JS_SetOpaque(obj, zmqsocket);

    // keep track of socket in Context object
    zmqsocket_index = zmqcontext->nb_zmqsockets++;
    new_zmqsockets = js_realloc_rt(rt, zmqcontext->zmqsockets, zmqcontext->nb_zmqsockets * sizeof(ZMQSocket *));
    if ( new_zmqsockets == NULL )
        goto out_of_memory;
    zmqcontext->zmqsockets = new_zmqsockets;
    zmqcontext->zmqsockets[zmqsocket_index] = zmqsocket;
    zmqcontext->ref_count++;

    // fill socket context
    zmqsocket->zsocket = zsocket;
    zmqsocket->zmqcontext = zmqcontext;
    zmqsocket->ref_count = 1;
    zmqsocket->closed = 0;
    zmqsocket->index = zmqsocket_index;

    return obj;

out_of_memory:
    if ( zmqsocket != NULL )
        js_free_rt(rt, zmqsocket);
    if ( zsocket != NULL )
        zmq_close(zsocket);
    if ( !JS_IsNull(obj) )
        JS_FreeValue(ctx, obj);
    return JS_ThrowOutOfMemory(ctx);
}

static JSValue
js_ZMQContext_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQContext *zmqcontext = (ZMQContext *) JS_GetOpaque(this_val, js_ZMQContext_class_id);
    JSValue joption = argv[0];
    JSValue joptval = argv[1];
    int32_t option;
    int32_t optval;
    int ret;
    if ( !JS_IsInt32(joption) || !JS_IsInt32(joptval) )
        return JS_ThrowTypeError(ctx, "int ZMQContext.set(int option, int optval)");
    option = JS_VALUE_GET_INT(joption);
    optval = JS_VALUE_GET_INT(joptval);
    ret = zmq_ctx_set(zmqcontext->zctx, option, optval);
    if ( ret < 0 )
        return ThrowZMQError(ctx);
    return JS_NewInt32(ctx, ret);
}

static JSValue
js_ZMQContext_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQContext *zmqcontext = (ZMQContext *) JS_GetOpaque(this_val, js_ZMQContext_class_id);
    JSValue joption = argv[0];
    int32_t option;
    int ret;
    if ( !JS_IsInt32(joption) )
        return JS_ThrowTypeError(ctx, "int ZMQContext.get(int option)");
    option = JS_VALUE_GET_INT(joption);
    ret = zmq_ctx_get(zmqcontext->zctx, option);
    if ( ret < 0 )
        return ThrowZMQError(ctx);
    return JS_NewInt32(ctx, ret);
}

static JSValue
js_ZMQContext_shutdown(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQContext *zmqcontext = (ZMQContext *) JS_GetOpaque(this_val, js_ZMQContext_class_id);
    int ret = zmq_ctx_shutdown(zmqcontext->zctx);
    if ( ret < 0 )
        return ThrowZMQError(ctx);
    return JS_NewInt32(ctx, ret);
}

static JSValue
js_ZMQContext_term(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQContext *zmqcontext = (ZMQContext *) JS_GetOpaque(this_val, js_ZMQContext_class_id);
    int ret = zmq_ctx_term(zmqcontext->zctx);
    if ( ret < 0 )
        return ThrowZMQError(ctx);
    zmqcontext->terminated = 1;
    return JS_NewInt32(ctx, ret);
}

/*********************************************************************/
static JSValue
js_ZMQPoller_size(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQPoller *zmqpoller = (ZMQPoller *) JS_GetOpaque(this_val, js_ZMQPoller_class_id);
    int ret = zmq_poller_size(zmqpoller->zpoller);
    return JS_NewInt32(ctx, ret);
}

static JSValue
js_ZMQPoller_add(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQPoller *zmqpoller = (ZMQPoller *) JS_GetOpaque(this_val, js_ZMQPoller_class_id);
    JSValue jsocket = argv[0];
    JSValue jevents = argv[1];
    JSValue juser_data = argv[2];
    ZMQSocket *zmqsocket;
    zmq_socket_t *zsocket;
    int events;
    JSRuntime *rt;
    PollerEntry *entry;
    int entry_index;
    int ret;

    // parse arguments
    zmqsocket = (ZMQSocket *) JS_GetOpaque(jsocket, js_ZMQSocket_class_id);
    if ( zmqsocket == NULL || !JS_IsInt32(jevents) )
        return JS_ThrowTypeError(ctx, "int ZMQPoller.add(ZMQSocket socket, int events, [user_data])");
    zsocket = zmqsocket->zsocket;
    events = JS_VALUE_GET_INT(jevents);
    rt = JS_GetRuntime(ctx);
    entry = (PollerEntry *) js_mallocz_rt(rt, sizeof(PollerEntry));
    if ( entry == NULL )
        return JS_ThrowOutOfMemory(ctx);

    ret = zmq_poller_add(zmqpoller->zpoller, zsocket, entry, events);
    if ( ret < 0 )
    {
        js_free_rt(rt, entry);
        return ThrowZMQError(ctx);
    }

    entry_index = zmqpoller->nb_entries++;
    zmqpoller->entries = js_realloc_rt(rt, zmqpoller->entries, zmqpoller->nb_entries * sizeof(PollerEntry *));
    zmqpoller->entries[entry_index] = entry;

    entry->zsocket = zsocket;
    entry->socket = JS_DupValue(ctx, jsocket);
    entry->user_data = JS_DupValue(ctx, juser_data);

    return JS_NewInt32(ctx, ret);
}

static JSValue
js_ZMQPoller_modify(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQPoller *zmqpoller = (ZMQPoller *) JS_GetOpaque(this_val, js_ZMQPoller_class_id);
    JSValue jsocket = argv[0];
    JSValue jevents = argv[1];
    ZMQSocket *zmqsocket;
    int events;
    int ret;
    zmqsocket = (ZMQSocket *) JS_GetOpaque(jsocket, js_ZMQSocket_class_id);
    if ( zmqsocket == NULL || !JS_IsInt32(jevents) )
        return JS_ThrowTypeError(ctx, "int ZMQPoller.modify(ZMQSocket socket, int events)");
    events = JS_VALUE_GET_INT(jevents);
    ret = zmq_poller_modify(zmqpoller->zpoller, zmqsocket->zsocket, events);
    if ( ret < 0 )
        return ThrowZMQError(ctx);
    return JS_NewInt32(ctx, ret);
}

static JSValue
js_ZMQPoller_remove(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQPoller *zmqpoller = (ZMQPoller *) JS_GetOpaque(this_val, js_ZMQPoller_class_id);
    JSValue jsocket = argv[0];
    ZMQSocket *zmqsocket;
    int ret;
    JSRuntime *rt;
    zmq_socket_t *zsocket;
    zmqsocket = (ZMQSocket *) JS_GetOpaque(jsocket, js_ZMQSocket_class_id);
    if ( zmqsocket == NULL )
        return JS_ThrowTypeError(ctx, "int ZMQPoller.remove(ZMQSocket socket)");
    zsocket = zmqsocket->zsocket;
    ret = zmq_poller_remove(zmqpoller->zpoller, zsocket);
    if ( ret < 0 )
        return ThrowZMQError(ctx);
    rt = JS_GetRuntime(ctx);
    for ( int i = 0; i < zmqpoller->nb_entries; i++ )
        free_PollerEntry(rt, &zmqpoller->entries[i], zsocket);
    return JS_NewInt32(ctx, ret);
}

static JSValue
new_PollerEvent(JSContext *ctx, zmq_poller_event_t *event)
{
    PollerEntry *entry = (PollerEntry *) event->user_data;
    JSValue obj = JS_NewObject(ctx);
    /* TODO entry->socket should be the same we set in user_data */
    JS_SetPropertyStr(ctx, obj, "socket", JS_DupValue(ctx, entry->socket));
    JS_SetPropertyStr(ctx, obj, "user_data", JS_DupValue(ctx, entry->user_data));
    JS_SetPropertyStr(ctx, obj, "events", JS_NewInt32(ctx, event->events));
    return obj;
}

static JSValue
ZMQPoller_wait(JSContext *ctx, ZMQPoller *zmqpoller, int timeout)
{
    zmq_poller_event_t event;
    int ret = zmq_poller_wait(zmqpoller->zpoller, &event, timeout);
    if ( ret < 0 )
    {
        if ( zmq_errno() == EAGAIN )
            return JS_NULL;
        return ThrowZMQError(ctx);
    }
    return new_PollerEvent(ctx, &event);
}

static JSValue
js_ZMQPoller_wait(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQPoller *zmqpoller = (ZMQPoller *) JS_GetOpaque(this_val, js_ZMQPoller_class_id);
    JSValue jtimeout = argv[0];
    int timeout; /* nobody will wait over 4 billion milliseconds */

    // parse arguments
    timeout = JS_VALUE_GET_INT(jtimeout);
    if ( !JS_IsInt32(jtimeout) )
        return JS_ThrowTypeError(ctx, "(ZMQEvent or null) ZMQPoller.wait(int timeout)");

    return ZMQPoller_wait(ctx, zmqpoller, timeout);
}

static JSValue
js_ZMQPoller_poll(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQPoller *zmqpoller = (ZMQPoller *) JS_GetOpaque(this_val, js_ZMQPoller_class_id);
    return ZMQPoller_wait(ctx, zmqpoller, 0);
}

static JSValue
ZMQPoller_wait_all(JSContext *ctx, ZMQPoller *zmqpoller, int n_events, int timeout)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    zmq_poller_event_t *events;
    JSValue *parray;
    JSValue jret;
    int ret;

    // allocate events for zmq_poller_wait_all()
    events = (zmq_poller_event_t *) js_mallocz_rt(rt, n_events * sizeof(zmq_poller_event_t));
    if ( events == NULL )
        return JS_ThrowOutOfMemory(ctx);

    // call zmq_poller_wait_all()
    ret = zmq_poller_wait_all(zmqpoller->zpoller, events, n_events, timeout);
    if ( ret < 0 )
    {
        js_free_rt(rt, events);
        if ( zmq_errno() == EAGAIN )
            return JS_NULL;
        return ThrowZMQError(ctx);
    }
    // It never returns 0.
    jret = JS_NewFastArray(ctx, &parray, ret, 1);
    if ( !JS_IsException(jret) )
        for ( int i = 0; i < ret; i++ )
            parray[i] = new_PollerEvent(ctx, &events[i]);
    js_free_rt(rt, events);
    return jret;
}

static JSValue
js_ZMQPoller_wait_all(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQPoller *zmqpoller = (ZMQPoller *) JS_GetOpaque(this_val, js_ZMQPoller_class_id);
    JSValue jn_events = argv[0];
    JSValue jtimeout = argv[1];
    int n_events;
    int timeout; /* nobody will wait over 4 billion milliseconds */

    // parse arguments
    if ( !JS_IsInt32(jn_events) || !JS_IsInt32(jtimeout) )
    {
usage:
        return JS_ThrowTypeError(ctx, "(ZMQEvent[] or null) ZMQPoller.wait_all(int n_events, int timeout)");
    }
    n_events = JS_VALUE_GET_INT(jn_events);
    timeout = JS_VALUE_GET_INT(jtimeout);
    if ( n_events <= 0 )
        goto usage;

    return ZMQPoller_wait_all(ctx, zmqpoller, n_events, timeout);
}

static JSValue
js_ZMQPoller_poll_all(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQPoller *zmqpoller = (ZMQPoller *) JS_GetOpaque(this_val, js_ZMQPoller_class_id);
    JSValue jn_events = argv[0];
    int n_events;

    // parse arguments
    n_events = JS_VALUE_GET_INT(jn_events);
    if ( !JS_IsInt32(jn_events) || n_events <= 0 )
        return JS_ThrowTypeError(ctx, "(ZMQEvent[] or null) ZMQPoller.poll_all(int n_events)");

    return ZMQPoller_wait_all(ctx, zmqpoller, n_events, 0);
}

/*********************************************************************/
static JSValue
js_ZMQSocket_close(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQSocket *zmqsocket = (ZMQSocket *) JS_GetOpaque(this_val, js_ZMQSocket_class_id);
    int ret = zmq_close(zmqsocket->zsocket);
    if ( ret < 0 )
        return ThrowZMQError(ctx);
    zmqsocket->closed = 1;
    return JS_NewInt32(ctx, ret);
}

static JSValue
js_ZMQSocket_bind(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQSocket *zmqsocket = (ZMQSocket *) JS_GetOpaque(this_val, js_ZMQSocket_class_id);
    JSValue jaddr = argv[0];
    const char *addr;
    int ret;
    if ( !JS_IsString(jaddr) )
        return JS_ThrowTypeError(ctx, "int ZMQSocket.bind(const char *addr)");
    addr = JS_ToCString(ctx, jaddr);
    ret = zmq_bind(zmqsocket->zsocket, addr);
    JS_FreeCString(ctx, addr);
    if ( ret < 0 )
        return ThrowZMQError(ctx);
    return JS_NewInt32(ctx, ret);
}

static JSValue
js_ZMQSocket_connect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQSocket *zmqsocket = (ZMQSocket *) JS_GetOpaque(this_val, js_ZMQSocket_class_id);
    JSValue jaddr = argv[0];
    const char *addr;
    int ret;
    if ( !JS_IsString(jaddr) )
        return JS_ThrowTypeError(ctx, "int ZMQSocket.connect(const char *addr)");
    addr = JS_ToCString(ctx, jaddr);
    ret = zmq_connect(zmqsocket->zsocket, addr);
    JS_FreeCString(ctx, addr);
    if ( ret < 0 )
        return ThrowZMQError(ctx);
    return JS_NewInt32(ctx, ret);
}

static JSValue
js_ZMQSocket_unbind(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQSocket *zmqsocket = (ZMQSocket *) JS_GetOpaque(this_val, js_ZMQSocket_class_id);
    JSValue jaddr = argv[0];
    const char *addr;
    int ret;
    if ( !JS_IsString(jaddr) )
        return JS_ThrowTypeError(ctx, "int ZMQSocket.unbind(const char *addr)");
    addr = JS_ToCString(ctx, jaddr);
    ret = zmq_unbind(zmqsocket->zsocket, addr);
    JS_FreeCString(ctx, addr);
    if ( ret < 0 )
        return ThrowZMQError(ctx);
    return JS_NewInt32(ctx, ret);
}

static JSValue
js_ZMQSocket_disconnect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQSocket *zmqsocket = (ZMQSocket *) JS_GetOpaque(this_val, js_ZMQSocket_class_id);
    JSValue jaddr = argv[0];
    const char *addr;
    int ret;
    if ( !JS_IsString(jaddr) )
        return JS_ThrowTypeError(ctx, "int ZMQSocket.disconnect(const char *addr)");
    addr = JS_ToCString(ctx, jaddr);
    ret = zmq_disconnect(zmqsocket->zsocket, addr);
    JS_FreeCString(ctx, addr);
    if ( ret < 0 )
        return ThrowZMQError(ctx);
    return JS_NewInt32(ctx, ret);
}

static JSValue
js_ZMQSocket_send(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQSocket *zmqsocket = (ZMQSocket *) JS_GetOpaque(this_val, js_ZMQSocket_class_id);
    JSValue jdata = argv[0];
    JSValue jflags = argv[1];
    int32_t flags = 0;
    uint8_t *u8data = NULL;
    uint32_t len32 = 0;
    int ret;

    // parse arguments
    if ( JS_IsInt32(jflags) )
        flags = JS_VALUE_GET_INT(jflags);

    if ( JS_IsNull(jdata) )
    {
        ret = zmq_send(zmqsocket->zsocket, NULL, 0, flags);
    }
    else if ( JS_IsInt32(jdata) || JS_IsBool(jdata) )
    {
        int data = JS_VALUE_GET_INT(jdata);
        size_t len = sizeof(data);
        ret = zmq_send(zmqsocket->zsocket, &data, len, flags);
    }
    else if ( JS_IsFloat64(jdata) || JS_IsBigInt(ctx, jdata) )
    {
        uint64_t data = 0;
        size_t len = sizeof(data);
        JS_ToInt64Ext(ctx, &data, jdata);
        ret = zmq_send(zmqsocket->zsocket, &data, len, flags);
    }
    else if ( JS_IsString(jdata) )
    {
        const char *data = NULL;
        size_t len = 0;
        data = JS_ToCStringLen(ctx, &len, jdata);
        ret = zmq_send(zmqsocket->zsocket, data, len, flags);
        JS_FreeCString(ctx, data);
    }
    else if ( JS_GetUint8FFArray(jdata, &u8data, &len32) )
    {
        size_t len = len32;
        ret = zmq_send(zmqsocket->zsocket, u8data, len, flags);
    }
    else
    {
        return JS_ThrowTypeError(ctx, "int ZMQSocket.send((string, Uint8FFArray, Float64, BigInt, int, bool, or null) data, [int flags])");
    }
    if ( ret < 0 )
    {
        if ( zmq_errno() == EAGAIN )
            return JS_UNDEFINED;
        return ThrowZMQError(ctx);
    }
    return JS_NewInt32(ctx, ret);
}

enum {
    IN_TYPE_INT             = 0x0001,
    IN_TYPE_BIGINT          = 0x0002,
    IN_TYPE_STRING          = 0x0004,
    IN_TYPE_UINT8FFARRAY    = 0x0008,
};

// remove extra trailing '\0' if we have a string
static size_t remove_extra_trailing_nul(const char *data, size_t len)
{
    int i;
    for ( i = 0; i < len; i++ )
        if ( data[i] == '\0' )
            break;
    if ( len == i + 1 )
        len = i;
    return len;
}

static JSValue
js_ZMQSocket_recv(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic)
{
    ZMQSocket *zmqsocket = (ZMQSocket *) JS_GetOpaque(this_val, js_ZMQSocket_class_id);
    JSValue jflags = argv[0];
    int32_t flags = 0;
    zmq_msg_t msg;
    int ret;
    void *msg_data;
    size_t msg_size;
    size_t requested_size;
    JSValue jret;
    if ( JS_IsInt32(jflags) )
      flags = JS_VALUE_GET_INT(jflags);
    zmq_msg_init(&msg);
    ret = zmq_msg_recv(&msg, zmqsocket->zsocket, flags);
    if ( ret < 0 )
    {
        zmq_msg_close(&msg);
        if ( zmq_errno() == EAGAIN )
            return JS_UNDEFINED;
        return ThrowZMQError(ctx);
    }
    msg_data = zmq_msg_data(&msg);
    msg_size = zmq_msg_size(&msg);
    if ( magic == IN_TYPE_INT )
    {
        requested_size = sizeof(int);
        if ( msg_size != requested_size )
        {
bad_size:
            jret = JS_ThrowTypeError(ctx, "ZMQSocket.recv() requested size %zu, but received %zu bytes. Message lost!", requested_size, msg_size);
            goto the_end;
        }
        jret = JS_NewInt32(ctx, *(int *)msg_data);
    }
    else if ( magic == IN_TYPE_BIGINT )
    {
        requested_size = sizeof(uint64_t);
        if ( msg_size != requested_size )
            goto bad_size;
        jret = JS_NewBigInt64(ctx, *(uint64_t *)msg_data);
    }
    else if ( magic == IN_TYPE_STRING )
    {
        msg_size = remove_extra_trailing_nul(msg_data, msg_size);
        jret = JS_NewStringLen(ctx, msg_data, msg_size);
    }
    else if ( magic == IN_TYPE_UINT8FFARRAY )
    {
        uint8_t *newbuf;
        jret = JS_NewUint8FFArray(ctx, &newbuf, msg_size, 0);
        if ( !JS_IsException(jret) )
            memcpy(newbuf, msg_data, msg_size);
    }
    else
    {
        // should never happen
        assert(0);
    }
the_end:
    zmq_msg_close(&msg);
    return jret;
}

static JSValue
js_ZMQSocket_setsockopt(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQSocket *zmqsocket = (ZMQSocket *) JS_GetOpaque(this_val, js_ZMQSocket_class_id);
    JSValue joption_name = argv[0];
    JSValue joption_value = argv[1];
    int option_name;
    uint8_t *u8data = NULL;
    uint32_t len32 = 0;
    int ret;

    // parse arguments
    if ( !JS_IsInt32(joption_name) )
    {
usage:
        return JS_ThrowTypeError(ctx, "int ZMQSocket.setsockopt(int option_name, (string, Uint8FFArray, Float64, BigInt, int, bool, or null) option_value)");
    }
    option_name = JS_VALUE_GET_INT(joption_name);

    if ( JS_IsNull(joption_value) )
    {
        void *option_value = NULL;
        size_t option_len = 0;
        ret = zmq_setsockopt(zmqsocket->zsocket, option_name, option_value, option_len);
    }
    else if ( JS_IsInt32(joption_value) || JS_IsBool(joption_value) )
    {
        int option_value = JS_VALUE_GET_INT(joption_value);
        size_t option_len = sizeof(option_value);
        ret = zmq_setsockopt(zmqsocket->zsocket, option_name, &option_value, option_len);
    }
    else if ( JS_IsFloat64(joption_value) || JS_IsBigInt(ctx, joption_value) )
    {
        uint64_t option_value = 0;
        size_t option_len = sizeof(option_value);
        JS_ToInt64Ext(ctx, &option_value, joption_value);
        ret = zmq_setsockopt(zmqsocket->zsocket, option_name, &option_value, option_len);
    }
    else if ( JS_IsString(joption_value) )
    {
        const char *option_value = NULL;
        size_t option_len = 0;
        option_value = JS_ToCStringLen(ctx, &option_len, joption_value);
        if ( option_len > UCHAR_MAX )
            option_len = UCHAR_MAX;
        ret = zmq_setsockopt(zmqsocket->zsocket, option_name, option_value, option_len);
        JS_FreeCString(ctx, option_value);
    }
    else if ( JS_GetUint8FFArray(joption_value, &u8data, &len32) )
    {
        const char *option_value = u8data;
        size_t option_len = len32;
        ret = zmq_setsockopt(zmqsocket->zsocket, option_name, &option_value, option_len);
    }
    else
    {
        goto usage;
    }
    if ( ret < 0 )
        return ThrowZMQError(ctx);
    return JS_NewInt32(ctx, ret);
}

static JSValue
js_ZMQSocket_getsockopt(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic)
{
    ZMQSocket *zmqsocket = (ZMQSocket *) JS_GetOpaque(this_val, js_ZMQSocket_class_id);
    JSValue joption_name = argv[0];
    int option_name;
    int ret = -1;

    // parse arguments
    if ( !JS_IsInt32(joption_name) )
        return JS_ThrowTypeError(ctx, "(string, int, or BigInt) ZMQSocket.getsockopt(int option_name)");
    option_name = JS_VALUE_GET_INT(joption_name);

    // start with string in case the option value is NULL
    if ( (magic & IN_TYPE_STRING) != 0 )
    {
        char option_value[UCHAR_MAX] = { 0 };
        size_t option_len = sizeof(option_value);
        ret = zmq_getsockopt(zmqsocket->zsocket, option_name, option_value, &option_len);
        if ( ret == 0 )
        {
            if ( magic != IN_TYPE_STRING )
            {
                // if no specific type requested, try int and bigint
                if ( option_len == sizeof(int) )
                    return JS_NewInt32(ctx, *(int *) option_value);
                else if ( option_len == sizeof(uint64_t) )
                    return JS_NewBigInt64(ctx, *(uint64_t *) option_value);
            }
            option_len = remove_extra_trailing_nul(option_value, option_len);
            return JS_NewStringLen(ctx, option_value, option_len);
        }
    }
    if ( (magic & IN_TYPE_INT) != 0 )
    {
        int option_value = 0;
        size_t option_len = sizeof(option_value);
        ret = zmq_getsockopt(zmqsocket->zsocket, option_name, &option_value, &option_len);
        if ( ret == 0 )
            return JS_NewInt32(ctx, option_value);
    }
    if ( (magic & IN_TYPE_BIGINT) != 0 )
    {
        uint64_t option_value = 0;
        size_t option_len = sizeof(option_value);
        ret = zmq_getsockopt(zmqsocket->zsocket, option_name, &option_value, &option_len);
        if ( ret == 0 )
            return JS_NewBigInt64(ctx, option_value);
    }
    if ( (magic & IN_TYPE_UINT8FFARRAY) != 0 )
    {
        char option_value[UCHAR_MAX] = { 0 };
        size_t option_len = sizeof(option_value);
        ret = zmq_getsockopt(zmqsocket->zsocket, option_name, option_value, &option_len);
        if ( ret == 0 )
        {
            uint8_t *newbuf;
            JSValue jret = JS_NewUint8FFArray(ctx, &newbuf, option_len, 0);
            if ( !JS_IsException(jret) )
                memcpy(newbuf, option_value, option_len);
            return jret;
        }
    }
    if ( ret < 0 )
        return ThrowZMQError(ctx);
    return JS_UNDEFINED;
}

/*********************************************************************/
static const JSCFunctionListEntry js_ZMQ_funcs[] = {
    JS_CFUNC_DEF("version", 0, js_ZMQ_version),
    JS_CFUNC_DEF("has", 1, js_ZMQ_has),
    /* 0MQ errors */
    JS_PROP_INT32_DEF("ENOTSUP", ENOTSUP, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("EPROTONOSUPPORT", EPROTONOSUPPORT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ENOBUFS", ENOBUFS, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ENETDOWN", ENETDOWN, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("EADDRINUSE", EADDRINUSE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("EADDRNOTAVAIL", EADDRNOTAVAIL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ECONNREFUSED", ECONNREFUSED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("EINPROGRESS", EINPROGRESS, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ENOTSOCK", ENOTSOCK, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("EMSGSIZE", EMSGSIZE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("EAFNOSUPPORT", EAFNOSUPPORT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ENETUNREACH", ENETUNREACH, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ECONNABORTED", ECONNABORTED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ECONNRESET", ECONNRESET, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ENOTCONN", ENOTCONN, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ETIMEDOUT", ETIMEDOUT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("EHOSTUNREACH", EHOSTUNREACH, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ENETRESET", ENETRESET, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("EFSM", EFSM, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ENOCOMPATPROTO", ENOCOMPATPROTO, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ETERM", ETERM, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("EMTHREAD", EMTHREAD, JS_PROP_CONFIGURABLE),
    /* Context options */
    JS_PROP_INT32_DEF("IO_THREADS", ZMQ_IO_THREADS, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("MAX_SOCKETS", ZMQ_MAX_SOCKETS, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("SOCKET_LIMIT", ZMQ_SOCKET_LIMIT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("THREAD_PRIORITY", ZMQ_THREAD_PRIORITY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("THREAD_SCHED_POLICY", ZMQ_THREAD_SCHED_POLICY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("MAX_MSGSZ", ZMQ_MAX_MSGSZ, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("MSG_T_SIZE", ZMQ_MSG_T_SIZE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("THREAD_AFFINITY_CPU_ADD", ZMQ_THREAD_AFFINITY_CPU_ADD, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("THREAD_AFFINITY_CPU_REMOVE", ZMQ_THREAD_AFFINITY_CPU_REMOVE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("THREAD_NAME_PREFIX", ZMQ_THREAD_NAME_PREFIX, JS_PROP_CONFIGURABLE),
    /* Default for new contexts */
    JS_PROP_INT32_DEF("IO_THREADS_DFLT", ZMQ_IO_THREADS_DFLT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("MAX_SOCKETS_DFLT", ZMQ_MAX_SOCKETS_DFLT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("THREAD_PRIORITY_DFLT", ZMQ_THREAD_PRIORITY_DFLT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("THREAD_SCHED_POLICY_DFLT", ZMQ_THREAD_SCHED_POLICY_DFLT, JS_PROP_CONFIGURABLE),
    /* Socket types */
    JS_PROP_INT32_DEF("PAIR", ZMQ_PAIR, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PUB", ZMQ_PUB, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("SUB", ZMQ_SUB, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("REQ", ZMQ_REQ, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("REP", ZMQ_REP, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("DEALER", ZMQ_DEALER, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ROUTER", ZMQ_ROUTER, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PULL", ZMQ_PULL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PUSH", ZMQ_PUSH, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("XPUB", ZMQ_XPUB, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("XSUB", ZMQ_XSUB, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("STREAM", ZMQ_STREAM, JS_PROP_CONFIGURABLE),
    /* Socket options */
    JS_PROP_INT32_DEF("AFFINITY", ZMQ_AFFINITY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ROUTING_ID", ZMQ_ROUTING_ID, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("SUBSCRIBE", ZMQ_SUBSCRIBE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("UNSUBSCRIBE", ZMQ_UNSUBSCRIBE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("RATE", ZMQ_RATE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("RECOVERY_IVL", ZMQ_RECOVERY_IVL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("SNDBUF", ZMQ_SNDBUF, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("RCVBUF", ZMQ_RCVBUF, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("RCVMORE", ZMQ_RCVMORE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("FD", ZMQ_FD, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("EVENTS", ZMQ_EVENTS, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("TYPE", ZMQ_TYPE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("LINGER", ZMQ_LINGER, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("RECONNECT_IVL", ZMQ_RECONNECT_IVL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("BACKLOG", ZMQ_BACKLOG, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("RECONNECT_IVL_MAX", ZMQ_RECONNECT_IVL_MAX, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("MAXMSGSIZE", ZMQ_MAXMSGSIZE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("SNDHWM", ZMQ_SNDHWM, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("RCVHWM", ZMQ_RCVHWM, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("MULTICAST_HOPS", ZMQ_MULTICAST_HOPS, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("RCVTIMEO", ZMQ_RCVTIMEO, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("SNDTIMEO", ZMQ_SNDTIMEO, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("LAST_ENDPOINT", ZMQ_LAST_ENDPOINT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ROUTER_MANDATORY", ZMQ_ROUTER_MANDATORY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("TCP_KEEPALIVE", ZMQ_TCP_KEEPALIVE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("TCP_KEEPALIVE_CNT", ZMQ_TCP_KEEPALIVE_CNT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("TCP_KEEPALIVE_IDLE", ZMQ_TCP_KEEPALIVE_IDLE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("TCP_KEEPALIVE_INTVL", ZMQ_TCP_KEEPALIVE_INTVL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("IMMEDIATE", ZMQ_IMMEDIATE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("XPUB_VERBOSE", ZMQ_XPUB_VERBOSE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ROUTER_RAW", ZMQ_ROUTER_RAW, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("IPV6", ZMQ_IPV6, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("MECHANISM", ZMQ_MECHANISM, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PLAIN_SERVER", ZMQ_PLAIN_SERVER, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PLAIN_USERNAME", ZMQ_PLAIN_USERNAME, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PLAIN_PASSWORD", ZMQ_PLAIN_PASSWORD, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("CURVE_SERVER", ZMQ_CURVE_SERVER, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("CURVE_PUBLICKEY", ZMQ_CURVE_PUBLICKEY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("CURVE_SECRETKEY", ZMQ_CURVE_SECRETKEY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("CURVE_SERVERKEY", ZMQ_CURVE_SERVERKEY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PROBE_ROUTER", ZMQ_PROBE_ROUTER, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("REQ_CORRELATE", ZMQ_REQ_CORRELATE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("REQ_RELAXED", ZMQ_REQ_RELAXED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("CONFLATE", ZMQ_CONFLATE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZAP_DOMAIN", ZMQ_ZAP_DOMAIN, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ROUTER_HANDOVER", ZMQ_ROUTER_HANDOVER, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("TOS", ZMQ_TOS, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("CONNECT_ROUTING_ID", ZMQ_CONNECT_ROUTING_ID, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("GSSAPI_SERVER", ZMQ_GSSAPI_SERVER, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("GSSAPI_PRINCIPAL", ZMQ_GSSAPI_PRINCIPAL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("GSSAPI_SERVICE_PRINCIPAL", ZMQ_GSSAPI_SERVICE_PRINCIPAL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("GSSAPI_PLAINTEXT", ZMQ_GSSAPI_PLAINTEXT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("HANDSHAKE_IVL", ZMQ_HANDSHAKE_IVL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("SOCKS_PROXY", ZMQ_SOCKS_PROXY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("XPUB_NODROP", ZMQ_XPUB_NODROP, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("BLOCKY", ZMQ_BLOCKY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("XPUB_MANUAL", ZMQ_XPUB_MANUAL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("XPUB_WELCOME_MSG", ZMQ_XPUB_WELCOME_MSG, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("STREAM_NOTIFY", ZMQ_STREAM_NOTIFY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("INVERT_MATCHING", ZMQ_INVERT_MATCHING, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("HEARTBEAT_IVL", ZMQ_HEARTBEAT_IVL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("HEARTBEAT_TTL", ZMQ_HEARTBEAT_TTL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("HEARTBEAT_TIMEOUT", ZMQ_HEARTBEAT_TIMEOUT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("XPUB_VERBOSER", ZMQ_XPUB_VERBOSER, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("CONNECT_TIMEOUT", ZMQ_CONNECT_TIMEOUT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("TCP_MAXRT", ZMQ_TCP_MAXRT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("THREAD_SAFE", ZMQ_THREAD_SAFE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("MULTICAST_MAXTPDU", ZMQ_MULTICAST_MAXTPDU, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("VMCI_BUFFER_SIZE", ZMQ_VMCI_BUFFER_SIZE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("VMCI_BUFFER_MIN_SIZE", ZMQ_VMCI_BUFFER_MIN_SIZE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("VMCI_BUFFER_MAX_SIZE", ZMQ_VMCI_BUFFER_MAX_SIZE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("VMCI_CONNECT_TIMEOUT", ZMQ_VMCI_CONNECT_TIMEOUT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("USE_FD", ZMQ_USE_FD, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("GSSAPI_PRINCIPAL_NAMETYPE", ZMQ_GSSAPI_PRINCIPAL_NAMETYPE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("GSSAPI_SERVICE_PRINCIPAL_NAMETYPE", ZMQ_GSSAPI_SERVICE_PRINCIPAL_NAMETYPE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("BINDTODEVICE", ZMQ_BINDTODEVICE, JS_PROP_CONFIGURABLE),
    /*  Deprecated options and aliases */
    JS_PROP_INT32_DEF("IDENTITY", ZMQ_IDENTITY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("CONNECT_RID", ZMQ_CONNECT_RID, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("TCP_ACCEPT_FILTER", ZMQ_TCP_ACCEPT_FILTER, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("IPC_FILTER_PID", ZMQ_IPC_FILTER_PID, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("IPC_FILTER_UID", ZMQ_IPC_FILTER_UID, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("IPC_FILTER_GID", ZMQ_IPC_FILTER_GID, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("IPV4ONLY", ZMQ_IPV4ONLY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("DELAY_ATTACH_ON_CONNECT", ZMQ_DELAY_ATTACH_ON_CONNECT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("NOBLOCK", ZMQ_NOBLOCK, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("FAIL_UNROUTABLE", ZMQ_FAIL_UNROUTABLE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ROUTER_BEHAVIOR", ZMQ_ROUTER_BEHAVIOR, JS_PROP_CONFIGURABLE),
    /*  DRAFT Socket options. */
    JS_PROP_INT32_DEF("ZAP_ENFORCE_DOMAIN", ZMQ_ZAP_ENFORCE_DOMAIN, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("LOOPBACK_FASTPATH", ZMQ_LOOPBACK_FASTPATH, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("METADATA", ZMQ_METADATA, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("MULTICAST_LOOP", ZMQ_MULTICAST_LOOP, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ROUTER_NOTIFY", ZMQ_ROUTER_NOTIFY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("XPUB_MANUAL_LAST_VALUE", ZMQ_XPUB_MANUAL_LAST_VALUE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("SOCKS_USERNAME", ZMQ_SOCKS_USERNAME, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("SOCKS_PASSWORD", ZMQ_SOCKS_PASSWORD, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("IN_BATCH_SIZE", ZMQ_IN_BATCH_SIZE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("OUT_BATCH_SIZE", ZMQ_OUT_BATCH_SIZE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("WSS_KEY_PEM", ZMQ_WSS_KEY_PEM, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("WSS_CERT_PEM", ZMQ_WSS_CERT_PEM, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("WSS_TRUST_PEM", ZMQ_WSS_TRUST_PEM, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("WSS_HOSTNAME", ZMQ_WSS_HOSTNAME, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("WSS_TRUST_SYSTEM", ZMQ_WSS_TRUST_SYSTEM, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ONLY_FIRST_SUBSCRIBE", ZMQ_ONLY_FIRST_SUBSCRIBE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("RECONNECT_STOP", ZMQ_RECONNECT_STOP, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("HELLO_MSG", ZMQ_HELLO_MSG, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("DISCONNECT_MSG", ZMQ_DISCONNECT_MSG, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PRIORITY", ZMQ_PRIORITY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("BUSY_POLL", ZMQ_BUSY_POLL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("HICCUP_MSG", ZMQ_HICCUP_MSG, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("XSUB_VERBOSE_UNSUBSCRIBE", ZMQ_XSUB_VERBOSE_UNSUBSCRIBE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("TOPICS_COUNT", ZMQ_TOPICS_COUNT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("NORM_MODE", ZMQ_NORM_MODE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("NORM_UNICAST_NACK", ZMQ_NORM_UNICAST_NACK, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("NORM_BUFFER_SIZE", ZMQ_NORM_BUFFER_SIZE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("NORM_SEGMENT_SIZE", ZMQ_NORM_SEGMENT_SIZE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("NORM_BLOCK_SIZE", ZMQ_NORM_BLOCK_SIZE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("NORM_NUM_PARITY", ZMQ_NORM_NUM_PARITY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("NORM_NUM_AUTOPARITY", ZMQ_NORM_NUM_AUTOPARITY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("NORM_PUSH", ZMQ_NORM_PUSH, JS_PROP_CONFIGURABLE),
    /* Message options */
    JS_PROP_INT32_DEF("MORE", ZMQ_MORE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("SHARED", ZMQ_SHARED, JS_PROP_CONFIGURABLE),
    /* Send/recv options */
    JS_PROP_INT32_DEF("DONTWAIT", ZMQ_DONTWAIT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("SNDMORE", ZMQ_SNDMORE, JS_PROP_CONFIGURABLE),
    /* Security mechanisms */
    JS_PROP_INT32_DEF("NULL", ZMQ_NULL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PLAIN", ZMQ_PLAIN, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("CURVE", ZMQ_CURVE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("GSSAPI", ZMQ_GSSAPI, JS_PROP_CONFIGURABLE),
    /* RADIO-DISH protocol */
    JS_PROP_INT32_DEF("GROUP_MAX_LENGTH", ZMQ_GROUP_MAX_LENGTH, JS_PROP_CONFIGURABLE),
    /* GSSAPI principal name types */
    JS_PROP_INT32_DEF("GSSAPI_NT_HOSTBASED", ZMQ_GSSAPI_NT_HOSTBASED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("GSSAPI_NT_USER_NAME", ZMQ_GSSAPI_NT_USER_NAME, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("GSSAPI_NT_KRB5_PRINCIPAL", ZMQ_GSSAPI_NT_KRB5_PRINCIPAL, JS_PROP_CONFIGURABLE),
    /* Socket transport events (TCP, IPC and TIPC only) */
    JS_PROP_INT32_DEF("EVENT_CONNECTED", ZMQ_EVENT_CONNECTED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("EVENT_CONNECT_DELAYED", ZMQ_EVENT_CONNECT_DELAYED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("EVENT_CONNECT_RETRIED", ZMQ_EVENT_CONNECT_RETRIED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("EVENT_LISTENING", ZMQ_EVENT_LISTENING, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("EVENT_BIND_FAILED", ZMQ_EVENT_BIND_FAILED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("EVENT_ACCEPTED", ZMQ_EVENT_ACCEPTED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("EVENT_ACCEPT_FAILED", ZMQ_EVENT_ACCEPT_FAILED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("EVENT_CLOSED", ZMQ_EVENT_CLOSED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("EVENT_CLOSE_FAILED", ZMQ_EVENT_CLOSE_FAILED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("EVENT_DISCONNECTED", ZMQ_EVENT_DISCONNECTED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("EVENT_MONITOR_STOPPED", ZMQ_EVENT_MONITOR_STOPPED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("EVENT_ALL", ZMQ_EVENT_ALL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("EVENT_HANDSHAKE_FAILED_NO_DETAIL", ZMQ_EVENT_HANDSHAKE_FAILED_NO_DETAIL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("EVENT_HANDSHAKE_SUCCEEDED", ZMQ_EVENT_HANDSHAKE_SUCCEEDED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("EVENT_HANDSHAKE_FAILED_PROTOCOL", ZMQ_EVENT_HANDSHAKE_FAILED_PROTOCOL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("EVENT_HANDSHAKE_FAILED_AUTH", ZMQ_EVENT_HANDSHAKE_FAILED_AUTH, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PROTOCOL_ERROR_ZMTP_UNSPECIFIED", ZMQ_PROTOCOL_ERROR_ZMTP_UNSPECIFIED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PROTOCOL_ERROR_ZMTP_UNEXPECTED_COMMAND", ZMQ_PROTOCOL_ERROR_ZMTP_UNEXPECTED_COMMAND, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PROTOCOL_ERROR_ZMTP_INVALID_SEQUENCE", ZMQ_PROTOCOL_ERROR_ZMTP_INVALID_SEQUENCE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PROTOCOL_ERROR_ZMTP_KEY_EXCHANGE", ZMQ_PROTOCOL_ERROR_ZMTP_KEY_EXCHANGE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_UNSPECIFIED", ZMQ_PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_UNSPECIFIED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_MESSAGE", ZMQ_PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_MESSAGE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_HELLO", ZMQ_PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_HELLO, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_INITIATE", ZMQ_PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_INITIATE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_ERROR", ZMQ_PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_ERROR, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_READY", ZMQ_PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_READY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_WELCOME", ZMQ_PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_WELCOME, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PROTOCOL_ERROR_ZMTP_INVALID_METADATA", ZMQ_PROTOCOL_ERROR_ZMTP_INVALID_METADATA, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PROTOCOL_ERROR_ZMTP_CRYPTOGRAPHIC", ZMQ_PROTOCOL_ERROR_ZMTP_CRYPTOGRAPHIC, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PROTOCOL_ERROR_ZMTP_MECHANISM_MISMATCH", ZMQ_PROTOCOL_ERROR_ZMTP_MECHANISM_MISMATCH, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PROTOCOL_ERROR_ZAP_UNSPECIFIED", ZMQ_PROTOCOL_ERROR_ZAP_UNSPECIFIED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PROTOCOL_ERROR_ZAP_MALFORMED_REPLY", ZMQ_PROTOCOL_ERROR_ZAP_MALFORMED_REPLY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PROTOCOL_ERROR_ZAP_BAD_REQUEST_ID", ZMQ_PROTOCOL_ERROR_ZAP_BAD_REQUEST_ID, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PROTOCOL_ERROR_ZAP_BAD_VERSION", ZMQ_PROTOCOL_ERROR_ZAP_BAD_VERSION, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PROTOCOL_ERROR_ZAP_INVALID_STATUS_CODE", ZMQ_PROTOCOL_ERROR_ZAP_INVALID_STATUS_CODE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PROTOCOL_ERROR_ZAP_INVALID_METADATA", ZMQ_PROTOCOL_ERROR_ZAP_INVALID_METADATA, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("PROTOCOL_ERROR_WS_UNSPECIFIED", ZMQ_PROTOCOL_ERROR_WS_UNSPECIFIED, JS_PROP_CONFIGURABLE),
    /* zmq_poller events */
    JS_PROP_INT32_DEF("POLLIN", ZMQ_POLLIN, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("POLLOUT", ZMQ_POLLOUT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("POLLERR", ZMQ_POLLERR, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("POLLPRI", ZMQ_POLLPRI, JS_PROP_CONFIGURABLE),
};

/*********************************************************************/
static const JSCFunctionListEntry js_ZMQSocket_proto_funcs[] = {
    JS_CFUNC_DEF("close", 0, js_ZMQSocket_close),
    JS_CFUNC_DEF("bind", 1, js_ZMQSocket_bind),
    JS_CFUNC_DEF("connect", 1, js_ZMQSocket_connect),
    JS_CFUNC_DEF("unbind", 1, js_ZMQSocket_unbind),
    JS_CFUNC_DEF("disconnect", 1, js_ZMQSocket_disconnect),
    JS_CFUNC_DEF("send", 2, js_ZMQSocket_send),
    JS_CFUNC_MAGIC_DEF("recv", 1, js_ZMQSocket_recv, IN_TYPE_UINT8FFARRAY),
    JS_CFUNC_MAGIC_DEF("recv_int", 1, js_ZMQSocket_recv, IN_TYPE_INT), // FFglitch extra
    JS_CFUNC_MAGIC_DEF("recv_bigint", 1, js_ZMQSocket_recv, IN_TYPE_BIGINT), // FFglitch extra
    JS_CFUNC_MAGIC_DEF("recv_str", 1, js_ZMQSocket_recv, IN_TYPE_STRING), // FFglitch extra
    JS_CFUNC_MAGIC_DEF("recv_uint8ffarray", 1, js_ZMQSocket_recv, IN_TYPE_UINT8FFARRAY), // FFglitch extra
    JS_CFUNC_DEF("setsockopt", 2, js_ZMQSocket_setsockopt),
    JS_CFUNC_MAGIC_DEF("getsockopt", 1, js_ZMQSocket_getsockopt, IN_TYPE_INT | IN_TYPE_BIGINT | IN_TYPE_STRING),
    JS_CFUNC_MAGIC_DEF("getsockopt_int", 1, js_ZMQSocket_getsockopt, IN_TYPE_INT), // FFglitch extra
    JS_CFUNC_MAGIC_DEF("getsockopt_bigint", 1, js_ZMQSocket_getsockopt, IN_TYPE_BIGINT), // FFglitch extra
    JS_CFUNC_MAGIC_DEF("getsockopt_str", 1, js_ZMQSocket_getsockopt, IN_TYPE_STRING), // FFglitch extra
    JS_CFUNC_MAGIC_DEF("getsockopt_uint8ffarray", 1, js_ZMQSocket_getsockopt, IN_TYPE_UINT8FFARRAY), // FFglitch extra
    // TODO zmq_socket_monitor!!!
};

/*********************************************************************/
static const JSCFunctionListEntry js_ZMQContext_proto_funcs[] = {
    JS_CFUNC_DEF("socket", 1, js_ZMQContext_socket),
    JS_CFUNC_DEF("set", 2, js_ZMQContext_set),
    JS_CFUNC_DEF("get", 1, js_ZMQContext_get),
    JS_CFUNC_DEF("shutdown", 0, js_ZMQContext_shutdown),
    JS_CFUNC_DEF("term", 0, js_ZMQContext_term),
};

/*********************************************************************/
static const JSCFunctionListEntry js_ZMQPoller_proto_funcs[] = {
    JS_CFUNC_DEF("size", 0, js_ZMQPoller_size),
    JS_CFUNC_DEF("add", 3, js_ZMQPoller_add),
    JS_CFUNC_DEF("modify", 2, js_ZMQPoller_modify),
    JS_CFUNC_DEF("remove", 1, js_ZMQPoller_remove),
    JS_CFUNC_DEF("wait", 1, js_ZMQPoller_wait),
    JS_CFUNC_DEF("poll", 0, js_ZMQPoller_poll), // FFglitch extra
    JS_CFUNC_DEF("wait_all", 2, js_ZMQPoller_wait_all),
    JS_CFUNC_DEF("poll_all", 1, js_ZMQPoller_poll_all), // FFglitch extra
};

/*********************************************************************/
static int js_ZMQContext_init(JSContext *ctx, JSModuleDef *m)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSValue ZMQContext_proto = JS_NewObject(ctx);
    JSValue jZMQContext_ctor = JS_NewCFunction2(ctx, js_ZMQContext_ctor, "Context", 0, JS_CFUNC_constructor, 0);
    JS_NewClassID(&js_ZMQContext_class_id);
    JS_NewClass(rt, js_ZMQContext_class_id, &js_ZMQContext_class);
    JS_SetPropertyFunctionList(ctx, ZMQContext_proto, js_ZMQContext_proto_funcs, countof(js_ZMQContext_proto_funcs));
    JS_SetConstructor(ctx, jZMQContext_ctor, ZMQContext_proto);
    JS_SetClassProto(ctx, js_ZMQContext_class_id, ZMQContext_proto);
    JS_SetModuleExport(ctx, m, "Context", jZMQContext_ctor);
    return 0;
}

static int js_ZMQPoller_init(JSContext *ctx, JSModuleDef *m)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSValue ZMQPoller_proto = JS_NewObject(ctx);
    JSValue jZMQPoller_ctor = JS_NewCFunction2(ctx, js_ZMQPoller_ctor, "Poller", 0, JS_CFUNC_constructor, 0);
    JS_NewClassID(&js_ZMQPoller_class_id);
    JS_NewClass(rt, js_ZMQPoller_class_id, &js_ZMQPoller_class);
    JS_SetPropertyFunctionList(ctx, ZMQPoller_proto, js_ZMQPoller_proto_funcs, countof(js_ZMQPoller_proto_funcs));
    JS_SetConstructor(ctx, jZMQPoller_ctor, ZMQPoller_proto);
    JS_SetClassProto(ctx, js_ZMQPoller_class_id, ZMQPoller_proto);
    JS_SetModuleExport(ctx, m, "Poller", jZMQPoller_ctor);
    return 0;
}

static int js_ZMQSocket_init(JSContext *ctx)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSValue ZMQSocket_proto = JS_NewObject(ctx);
    JS_NewClassID(&js_ZMQSocket_class_id);
    JS_NewClass(rt, js_ZMQSocket_class_id, &js_ZMQSocket_class);
    JS_SetPropertyFunctionList(ctx, ZMQSocket_proto, js_ZMQSocket_proto_funcs, countof(js_ZMQSocket_proto_funcs));
    JS_SetClassProto(ctx, js_ZMQSocket_class_id, ZMQSocket_proto);
    return 0;
}

static int js_zmq_init(JSContext *ctx, JSModuleDef *m)
{
    JS_SetModuleExportList(ctx, m, js_ZMQ_funcs, countof(js_ZMQ_funcs));
    js_ZMQContext_init(ctx, m);
    js_ZMQPoller_init(ctx, m);
    js_ZMQSocket_init(ctx);
    return 0;
}

JSModuleDef *js_init_module_zmq(JSContext *ctx, const char *module_name)
{
    JSModuleDef *m = JS_NewCModule(ctx, module_name, js_zmq_init);
    if ( m == NULL )
        return NULL;
    JS_AddModuleExportList(ctx, m, js_ZMQ_funcs, countof(js_ZMQ_funcs));
    JS_AddModuleExport(ctx, m, "Context");
    JS_AddModuleExport(ctx, m, "Poller");
    return m;
}
