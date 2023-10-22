/*
 * Copyright (C) 2023 Ramiro Polla
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
#include <string.h>
#include <zmq.h>

/*********************************************************************/
#include "libavutil/quickjs/quickjs-libc.h"
#include "libavutil/quickjs-zmq.h"
#include "internal.h"
#include "log.h"
#include "mem.h"

/*********************************************************************/
static const AVClass zmq_class = {
    .class_name = "quickjs-zmq",
    .item_name  = av_default_item_name,
    .version    = LIBAVUTIL_VERSION_INT,
};

/*********************************************************************/
typedef struct zmq_ctx_t zmq_ctx_t;
typedef struct zmq_socket_t zmq_socket_t;

typedef struct {
    const AVClass *class;
    zmq_ctx_t *zctx;
    zmq_socket_t **zsockets;
    int         nb_zsockets;
} ZMQContext;

typedef struct {
    const AVClass *class;
    zmq_socket_t *zsocket;
} ZMQSocketContext;

/*********************************************************************/
static JSClassID js_ZMQ_class_id;
static JSClassID js_ZMQSocket_class_id;

/*********************************************************************/
static JSValue ZMQ_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
    static int warned = 0;
    JSValue obj = JS_NULL;
    ZMQContext *zmqctx;

    /* using new_target to get the prototype is necessary when the
       class is extended. */
    JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if ( JS_IsException(proto) )
        return JS_EXCEPTION;
    obj = JS_NewObjectProtoClass(ctx, proto, js_ZMQ_class_id);
    JS_FreeValue(ctx, proto);
    if ( JS_IsException(obj) )
    {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }

    zmqctx = (ZMQContext *) av_mallocz(sizeof(ZMQContext));
    zmqctx->class = &zmq_class;
    JS_SetOpaque(obj, zmqctx);

    zmqctx->zctx = zmq_ctx_new();

    if ( !warned )
    {
        av_log(zmqctx, AV_LOG_WARNING, "The ZMQ interface is still experimental.\n");
        warned = 1;
    }

    return obj;
}

static void js_ZMQ_finalizer(JSRuntime *rt, JSValue val)
{
    ZMQContext *zmqctx = (ZMQContext *) JS_GetOpaque(val, js_ZMQ_class_id);
    for ( size_t i = 0; i < zmqctx->nb_zsockets; i++ )
    {
        int zero = 0;
        zmq_socket_t *zsocket = zmqctx->zsockets[i];
        zmq_setsockopt(zsocket, ZMQ_LINGER, &zero, sizeof(zero));
        zmq_close(zsocket);
    }
    av_free(zmqctx->zsockets);
    zmq_ctx_term(zmqctx->zctx);
    av_free(zmqctx);
}

static JSClassDef js_ZMQ_class = {
    "ZMQ",
    .finalizer = js_ZMQ_finalizer,
};

/*********************************************************************/
static void js_ZMQSocket_finalizer(JSRuntime *rt, JSValue val)
{
    ZMQSocketContext *zsocketctx = (ZMQSocketContext *) JS_GetOpaque(val, js_ZMQSocket_class_id);
    av_free(zsocketctx);
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
    if ( !JS_IsString(jcapability) )
    {
        JS_ThrowTypeError(ctx, "int ZMQ.has(const char *capability)");
        return JS_EXCEPTION;
    }
    capability = JS_ToCString(ctx, jcapability);
    return JS_NewInt32(ctx, zmq_has(capability));
}

/*********************************************************************/
static JSValue
ZMQError(JSContext *ctx)
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
js_ZMQ_socket(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQContext *zmqctx = (ZMQContext *) JS_GetOpaque(this_val, js_ZMQ_class_id);
    JSValue jtype = argv[0];
    int32_t type;
    zmq_socket_t *zsocket;
    JSValue jzsocket;
    ZMQSocketContext *zsocketctx;
    int socket_idx;

    if ( !JS_IsInt32(jtype) )
    {
        JS_ThrowTypeError(ctx, "ZMQSocket ZMQ.socket(int type)");
        return JS_EXCEPTION;
    }
    type = JS_VALUE_GET_INT(jtype);
    zsocket = zmq_socket(zmqctx->zctx, type);
    if ( zsocket == NULL )
        return ZMQError(ctx);

    jzsocket = JS_NewObjectClass(ctx, js_ZMQSocket_class_id);
    zsocketctx = (ZMQSocketContext *) av_mallocz(sizeof(ZMQSocketContext));
    zsocketctx->class = &zmq_class;
    zsocketctx->zsocket = zsocket;
    JS_SetOpaque(jzsocket, zsocketctx);

    /* keep track of socket in ZMQContext */
    socket_idx = zmqctx->nb_zsockets++;
    av_reallocp_array(&zmqctx->zsockets, zmqctx->nb_zsockets, sizeof(zmq_socket_t *));
    zmqctx->zsockets[socket_idx] = zsocket;

    return jzsocket;
}

static JSValue
js_ZMQ_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQContext *zmqctx = (ZMQContext *) JS_GetOpaque(this_val, js_ZMQ_class_id);
    JSValue joption = argv[0];
    JSValue joptval = argv[1];
    int32_t option;
    int32_t optval;
    int ret;
    if ( !JS_IsInt32(joption) || !JS_IsInt32(joptval) )
    {
        JS_ThrowTypeError(ctx, "int ZMQ.set(int option, int optval)");
        return JS_EXCEPTION;
    }
    option = JS_VALUE_GET_INT(joption);
    optval = JS_VALUE_GET_INT(joptval);
    ret = zmq_ctx_set(zmqctx->zctx, option, optval);
    if ( ret < 0 )
        return ZMQError(ctx);
    return JS_NewInt32(ctx, ret);
}

static JSValue
js_ZMQ_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQContext *zmqctx = (ZMQContext *) JS_GetOpaque(this_val, js_ZMQ_class_id);
    JSValue joption = argv[0];
    int32_t option;
    int ret;
    if ( !JS_IsInt32(joption) )
    {
        JS_ThrowTypeError(ctx, "int ZMQ.get(int option)");
        return JS_EXCEPTION;
    }
    option = JS_VALUE_GET_INT(joption);
    ret = zmq_ctx_get(zmqctx->zctx, option);
    if ( ret < 0 )
        return ZMQError(ctx);
    return JS_NewInt32(ctx, ret);
}

static JSValue
js_ZMQ_shutdown(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQContext *zmqctx = (ZMQContext *) JS_GetOpaque(this_val, js_ZMQ_class_id);
    int ret = zmq_ctx_shutdown(zmqctx->zctx);
    if ( ret < 0 )
        return ZMQError(ctx);
    return JS_NewInt32(ctx, ret);
}

/*********************************************************************/
static JSValue
js_ZMQSocket_bind(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQSocketContext *zsocketctx = (ZMQSocketContext *) JS_GetOpaque(this_val, js_ZMQSocket_class_id);
    JSValue jaddr = argv[0];
    const char *addr;
    int ret;
    if ( !JS_IsString(jaddr) )
    {
        JS_ThrowTypeError(ctx, "int ZMQ.bind(const char *addr)");
        return JS_EXCEPTION;
    }
    addr = JS_ToCString(ctx, jaddr);
    ret = zmq_bind(zsocketctx->zsocket, addr);
    if ( ret < 0 )
        return ZMQError(ctx);
    return JS_NewInt32(ctx, ret);
}

static JSValue
js_ZMQSocket_connect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQSocketContext *zsocketctx = (ZMQSocketContext *) JS_GetOpaque(this_val, js_ZMQSocket_class_id);
    JSValue jaddr = argv[0];
    const char *addr;
    int ret;
    if ( !JS_IsString(jaddr) )
    {
        JS_ThrowTypeError(ctx, "int ZMQ.connect(const char *addr)");
        return JS_EXCEPTION;
    }
    addr = JS_ToCString(ctx, jaddr);
    ret = zmq_connect(zsocketctx->zsocket, addr);
    if ( ret < 0 )
        return ZMQError(ctx);
    return JS_NewInt32(ctx, ret);
}

static JSValue
js_ZMQSocket_unbind(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQSocketContext *zsocketctx = (ZMQSocketContext *) JS_GetOpaque(this_val, js_ZMQSocket_class_id);
    JSValue jaddr = argv[0];
    const char *addr;
    int ret;
    if ( !JS_IsString(jaddr) )
    {
        JS_ThrowTypeError(ctx, "int ZMQ.unbind(const char *addr)");
        return JS_EXCEPTION;
    }
    addr = JS_ToCString(ctx, jaddr);
    ret = zmq_unbind(zsocketctx->zsocket, addr);
    if ( ret < 0 )
        return ZMQError(ctx);
    return JS_NewInt32(ctx, ret);
}

static JSValue
js_ZMQSocket_disconnect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQSocketContext *zsocketctx = (ZMQSocketContext *) JS_GetOpaque(this_val, js_ZMQSocket_class_id);
    JSValue jaddr = argv[0];
    const char *addr;
    int ret;
    if ( !JS_IsString(jaddr) )
    {
        JS_ThrowTypeError(ctx, "int ZMQ.disconnect(const char *addr)");
        return JS_EXCEPTION;
    }
    addr = JS_ToCString(ctx, jaddr);
    ret = zmq_disconnect(zsocketctx->zsocket, addr);
    if ( ret < 0 )
        return ZMQError(ctx);
    return JS_NewInt32(ctx, ret);
}

static JSValue
js_ZMQSocket_send(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQSocketContext *zsocketctx = (ZMQSocketContext *) JS_GetOpaque(this_val, js_ZMQSocket_class_id);
    JSValue jdata = argv[0];
    JSValue jflags = argv[1];
    uint8_t *data;
    uint32_t len;
    int32_t flags = ZMQ_DONTWAIT;
    int ret;
    if ( !JS_GetUint8FFArray(jdata, &data, &len) )
    {
        JS_ThrowTypeError(ctx, "int ZMQ.send(Uint8FFArray data, int flags)");
        return JS_EXCEPTION;
    }
    if ( JS_IsInt32(jflags) )
      flags = JS_VALUE_GET_INT(jflags);
    ret = zmq_send(zsocketctx->zsocket, data, len, flags);
    if ( ret < 0 )
    {
        if ( zmq_errno() == EAGAIN )
          return JS_UNDEFINED;
        return ZMQError(ctx);
    }
    return JS_NewInt32(ctx, ret);
}

static JSValue
js_ZMQSocket_recv(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ZMQSocketContext *zsocketctx = (ZMQSocketContext *) JS_GetOpaque(this_val, js_ZMQSocket_class_id);
    JSValue jflags = argv[0];
    int32_t flags = ZMQ_DONTWAIT;
    zmq_msg_t msg;
    int ret;
    void *msg_data;
    size_t msg_size;
    uint8_t *newbuf;
    JSValue jret;
    if ( JS_IsInt32(jflags) )
      flags = JS_VALUE_GET_INT(jflags);
    zmq_msg_init(&msg);
    ret = zmq_msg_recv(&msg, zsocketctx->zsocket, flags);
    if ( ret < 0 )
    {
        zmq_msg_close(&msg);
        if ( zmq_errno() == EAGAIN )
          return JS_UNDEFINED;
        return ZMQError(ctx);
    }
    msg_data = zmq_msg_data(&msg);
    msg_size = zmq_msg_size(&msg);
    jret = JS_NewUint8FFArray(ctx, &newbuf, msg_size, 0);
    if ( !JS_IsException(jret) )
        memcpy(newbuf, msg_data, msg_size);
    zmq_msg_close(&msg);
    return jret;
}

/*********************************************************************/
static const JSCFunctionListEntry js_ZMQ_proto_funcs[] = {
    JS_CFUNC_DEF("socket", 1, js_ZMQ_socket),
    JS_CFUNC_DEF("set", 2, js_ZMQ_set),
    JS_CFUNC_DEF("get", 1, js_ZMQ_get),
    JS_CFUNC_DEF("shutdown", 0, js_ZMQ_shutdown),
};

static const JSCFunctionListEntry js_ZMQ_obj[] = {
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
    JS_PROP_INT32_DEF("ZMQ_IO_THREADS", ZMQ_IO_THREADS, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_MAX_SOCKETS", ZMQ_MAX_SOCKETS, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_SOCKET_LIMIT", ZMQ_SOCKET_LIMIT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_THREAD_PRIORITY", ZMQ_THREAD_PRIORITY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_THREAD_SCHED_POLICY", ZMQ_THREAD_SCHED_POLICY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_MAX_MSGSZ", ZMQ_MAX_MSGSZ, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_MSG_T_SIZE", ZMQ_MSG_T_SIZE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_THREAD_AFFINITY_CPU_ADD", ZMQ_THREAD_AFFINITY_CPU_ADD, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_THREAD_AFFINITY_CPU_REMOVE", ZMQ_THREAD_AFFINITY_CPU_REMOVE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_THREAD_NAME_PREFIX", ZMQ_THREAD_NAME_PREFIX, JS_PROP_CONFIGURABLE),
    /* Default for new contexts */
    JS_PROP_INT32_DEF("ZMQ_IO_THREADS_DFLT", ZMQ_IO_THREADS_DFLT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_MAX_SOCKETS_DFLT", ZMQ_MAX_SOCKETS_DFLT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_THREAD_PRIORITY_DFLT", ZMQ_THREAD_PRIORITY_DFLT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_THREAD_SCHED_POLICY_DFLT", ZMQ_THREAD_SCHED_POLICY_DFLT, JS_PROP_CONFIGURABLE),
    /* Socket types */
    JS_PROP_INT32_DEF("ZMQ_PAIR", ZMQ_PAIR, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_PUB", ZMQ_PUB, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_SUB", ZMQ_SUB, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_REQ", ZMQ_REQ, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_REP", ZMQ_REP, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_DEALER", ZMQ_DEALER, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_ROUTER", ZMQ_ROUTER, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_PULL", ZMQ_PULL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_PUSH", ZMQ_PUSH, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_XPUB", ZMQ_XPUB, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_XSUB", ZMQ_XSUB, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_STREAM", ZMQ_STREAM, JS_PROP_CONFIGURABLE),
    /* Socket options */
    JS_PROP_INT32_DEF("ZMQ_AFFINITY", ZMQ_AFFINITY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_ROUTING_ID", ZMQ_ROUTING_ID, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_SUBSCRIBE", ZMQ_SUBSCRIBE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_UNSUBSCRIBE", ZMQ_UNSUBSCRIBE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_RATE", ZMQ_RATE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_RECOVERY_IVL", ZMQ_RECOVERY_IVL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_SNDBUF", ZMQ_SNDBUF, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_RCVBUF", ZMQ_RCVBUF, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_RCVMORE", ZMQ_RCVMORE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_FD", ZMQ_FD, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_EVENTS", ZMQ_EVENTS, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_TYPE", ZMQ_TYPE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_LINGER", ZMQ_LINGER, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_RECONNECT_IVL", ZMQ_RECONNECT_IVL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_BACKLOG", ZMQ_BACKLOG, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_RECONNECT_IVL_MAX", ZMQ_RECONNECT_IVL_MAX, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_MAXMSGSIZE", ZMQ_MAXMSGSIZE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_SNDHWM", ZMQ_SNDHWM, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_RCVHWM", ZMQ_RCVHWM, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_MULTICAST_HOPS", ZMQ_MULTICAST_HOPS, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_RCVTIMEO", ZMQ_RCVTIMEO, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_SNDTIMEO", ZMQ_SNDTIMEO, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_LAST_ENDPOINT", ZMQ_LAST_ENDPOINT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_ROUTER_MANDATORY", ZMQ_ROUTER_MANDATORY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_TCP_KEEPALIVE", ZMQ_TCP_KEEPALIVE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_TCP_KEEPALIVE_CNT", ZMQ_TCP_KEEPALIVE_CNT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_TCP_KEEPALIVE_IDLE", ZMQ_TCP_KEEPALIVE_IDLE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_TCP_KEEPALIVE_INTVL", ZMQ_TCP_KEEPALIVE_INTVL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_IMMEDIATE", ZMQ_IMMEDIATE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_XPUB_VERBOSE", ZMQ_XPUB_VERBOSE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_ROUTER_RAW", ZMQ_ROUTER_RAW, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_IPV6", ZMQ_IPV6, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_MECHANISM", ZMQ_MECHANISM, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_PLAIN_SERVER", ZMQ_PLAIN_SERVER, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_PLAIN_USERNAME", ZMQ_PLAIN_USERNAME, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_PLAIN_PASSWORD", ZMQ_PLAIN_PASSWORD, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_CURVE_SERVER", ZMQ_CURVE_SERVER, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_CURVE_PUBLICKEY", ZMQ_CURVE_PUBLICKEY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_CURVE_SECRETKEY", ZMQ_CURVE_SECRETKEY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_CURVE_SERVERKEY", ZMQ_CURVE_SERVERKEY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_PROBE_ROUTER", ZMQ_PROBE_ROUTER, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_REQ_CORRELATE", ZMQ_REQ_CORRELATE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_REQ_RELAXED", ZMQ_REQ_RELAXED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_CONFLATE", ZMQ_CONFLATE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_ZAP_DOMAIN", ZMQ_ZAP_DOMAIN, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_ROUTER_HANDOVER", ZMQ_ROUTER_HANDOVER, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_TOS", ZMQ_TOS, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_CONNECT_ROUTING_ID", ZMQ_CONNECT_ROUTING_ID, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_GSSAPI_SERVER", ZMQ_GSSAPI_SERVER, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_GSSAPI_PRINCIPAL", ZMQ_GSSAPI_PRINCIPAL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_GSSAPI_SERVICE_PRINCIPAL", ZMQ_GSSAPI_SERVICE_PRINCIPAL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_GSSAPI_PLAINTEXT", ZMQ_GSSAPI_PLAINTEXT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_HANDSHAKE_IVL", ZMQ_HANDSHAKE_IVL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_SOCKS_PROXY", ZMQ_SOCKS_PROXY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_XPUB_NODROP", ZMQ_XPUB_NODROP, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_BLOCKY", ZMQ_BLOCKY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_XPUB_MANUAL", ZMQ_XPUB_MANUAL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_XPUB_WELCOME_MSG", ZMQ_XPUB_WELCOME_MSG, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_STREAM_NOTIFY", ZMQ_STREAM_NOTIFY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_INVERT_MATCHING", ZMQ_INVERT_MATCHING, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_HEARTBEAT_IVL", ZMQ_HEARTBEAT_IVL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_HEARTBEAT_TTL", ZMQ_HEARTBEAT_TTL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_HEARTBEAT_TIMEOUT", ZMQ_HEARTBEAT_TIMEOUT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_XPUB_VERBOSER", ZMQ_XPUB_VERBOSER, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_CONNECT_TIMEOUT", ZMQ_CONNECT_TIMEOUT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_TCP_MAXRT", ZMQ_TCP_MAXRT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_THREAD_SAFE", ZMQ_THREAD_SAFE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_MULTICAST_MAXTPDU", ZMQ_MULTICAST_MAXTPDU, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_VMCI_BUFFER_SIZE", ZMQ_VMCI_BUFFER_SIZE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_VMCI_BUFFER_MIN_SIZE", ZMQ_VMCI_BUFFER_MIN_SIZE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_VMCI_BUFFER_MAX_SIZE", ZMQ_VMCI_BUFFER_MAX_SIZE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_VMCI_CONNECT_TIMEOUT", ZMQ_VMCI_CONNECT_TIMEOUT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_USE_FD", ZMQ_USE_FD, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_GSSAPI_PRINCIPAL_NAMETYPE", ZMQ_GSSAPI_PRINCIPAL_NAMETYPE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_GSSAPI_SERVICE_PRINCIPAL_NAMETYPE", ZMQ_GSSAPI_SERVICE_PRINCIPAL_NAMETYPE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_BINDTODEVICE", ZMQ_BINDTODEVICE, JS_PROP_CONFIGURABLE),
    /* Message options */
    JS_PROP_INT32_DEF("ZMQ_MORE", ZMQ_MORE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_SHARED", ZMQ_SHARED, JS_PROP_CONFIGURABLE),
    /* Send/recv options */
    JS_PROP_INT32_DEF("ZMQ_DONTWAIT", ZMQ_DONTWAIT, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_SNDMORE", ZMQ_SNDMORE, JS_PROP_CONFIGURABLE),
    /* Security mechanisms */
    JS_PROP_INT32_DEF("ZMQ_NULL", ZMQ_NULL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_PLAIN", ZMQ_PLAIN, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_CURVE", ZMQ_CURVE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_GSSAPI", ZMQ_GSSAPI, JS_PROP_CONFIGURABLE),
    /* RADIO-DISH protocol */
    JS_PROP_INT32_DEF("ZMQ_GROUP_MAX_LENGTH", ZMQ_GROUP_MAX_LENGTH, JS_PROP_CONFIGURABLE),
    /* GSSAPI principal name types */
    JS_PROP_INT32_DEF("ZMQ_GSSAPI_NT_HOSTBASED", ZMQ_GSSAPI_NT_HOSTBASED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_GSSAPI_NT_USER_NAME", ZMQ_GSSAPI_NT_USER_NAME, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_GSSAPI_NT_KRB5_PRINCIPAL", ZMQ_GSSAPI_NT_KRB5_PRINCIPAL, JS_PROP_CONFIGURABLE),
    /* Socket transport events (TCP, IPC and TIPC only) */
    JS_PROP_INT32_DEF("ZMQ_EVENT_CONNECTED", ZMQ_EVENT_CONNECTED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_EVENT_CONNECT_DELAYED", ZMQ_EVENT_CONNECT_DELAYED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_EVENT_CONNECT_RETRIED", ZMQ_EVENT_CONNECT_RETRIED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_EVENT_LISTENING", ZMQ_EVENT_LISTENING, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_EVENT_BIND_FAILED", ZMQ_EVENT_BIND_FAILED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_EVENT_ACCEPTED", ZMQ_EVENT_ACCEPTED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_EVENT_ACCEPT_FAILED", ZMQ_EVENT_ACCEPT_FAILED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_EVENT_CLOSED", ZMQ_EVENT_CLOSED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_EVENT_CLOSE_FAILED", ZMQ_EVENT_CLOSE_FAILED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_EVENT_DISCONNECTED", ZMQ_EVENT_DISCONNECTED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_EVENT_MONITOR_STOPPED", ZMQ_EVENT_MONITOR_STOPPED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_EVENT_ALL", ZMQ_EVENT_ALL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_EVENT_HANDSHAKE_FAILED_NO_DETAIL", ZMQ_EVENT_HANDSHAKE_FAILED_NO_DETAIL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_EVENT_HANDSHAKE_SUCCEEDED", ZMQ_EVENT_HANDSHAKE_SUCCEEDED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_EVENT_HANDSHAKE_FAILED_PROTOCOL", ZMQ_EVENT_HANDSHAKE_FAILED_PROTOCOL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_EVENT_HANDSHAKE_FAILED_AUTH", ZMQ_EVENT_HANDSHAKE_FAILED_AUTH, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_PROTOCOL_ERROR_ZMTP_UNSPECIFIED", ZMQ_PROTOCOL_ERROR_ZMTP_UNSPECIFIED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_PROTOCOL_ERROR_ZMTP_UNEXPECTED_COMMAND", ZMQ_PROTOCOL_ERROR_ZMTP_UNEXPECTED_COMMAND, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_PROTOCOL_ERROR_ZMTP_INVALID_SEQUENCE", ZMQ_PROTOCOL_ERROR_ZMTP_INVALID_SEQUENCE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_PROTOCOL_ERROR_ZMTP_KEY_EXCHANGE", ZMQ_PROTOCOL_ERROR_ZMTP_KEY_EXCHANGE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_UNSPECIFIED", ZMQ_PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_UNSPECIFIED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_MESSAGE", ZMQ_PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_MESSAGE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_HELLO", ZMQ_PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_HELLO, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_INITIATE", ZMQ_PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_INITIATE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_ERROR", ZMQ_PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_ERROR, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_READY", ZMQ_PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_READY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_WELCOME", ZMQ_PROTOCOL_ERROR_ZMTP_MALFORMED_COMMAND_WELCOME, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_PROTOCOL_ERROR_ZMTP_INVALID_METADATA", ZMQ_PROTOCOL_ERROR_ZMTP_INVALID_METADATA, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_PROTOCOL_ERROR_ZMTP_CRYPTOGRAPHIC", ZMQ_PROTOCOL_ERROR_ZMTP_CRYPTOGRAPHIC, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_PROTOCOL_ERROR_ZMTP_MECHANISM_MISMATCH", ZMQ_PROTOCOL_ERROR_ZMTP_MECHANISM_MISMATCH, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_PROTOCOL_ERROR_ZAP_UNSPECIFIED", ZMQ_PROTOCOL_ERROR_ZAP_UNSPECIFIED, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_PROTOCOL_ERROR_ZAP_MALFORMED_REPLY", ZMQ_PROTOCOL_ERROR_ZAP_MALFORMED_REPLY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_PROTOCOL_ERROR_ZAP_BAD_REQUEST_ID", ZMQ_PROTOCOL_ERROR_ZAP_BAD_REQUEST_ID, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_PROTOCOL_ERROR_ZAP_BAD_VERSION", ZMQ_PROTOCOL_ERROR_ZAP_BAD_VERSION, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_PROTOCOL_ERROR_ZAP_INVALID_STATUS_CODE", ZMQ_PROTOCOL_ERROR_ZAP_INVALID_STATUS_CODE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("ZMQ_PROTOCOL_ERROR_ZAP_INVALID_METADATA", ZMQ_PROTOCOL_ERROR_ZAP_INVALID_METADATA, JS_PROP_CONFIGURABLE),
#ifdef ZMQ_PROTOCOL_ERROR_WS_UNSPECIFIED
    JS_PROP_INT32_DEF("ZMQ_PROTOCOL_ERROR_WS_UNSPECIFIED", ZMQ_PROTOCOL_ERROR_WS_UNSPECIFIED, JS_PROP_CONFIGURABLE),
#endif
};

static const JSCFunctionListEntry js_ZMQSocket_proto_funcs[] = {
    JS_CFUNC_DEF("bind", 1, js_ZMQSocket_bind),
    JS_CFUNC_DEF("connect", 1, js_ZMQSocket_connect),
    JS_CFUNC_DEF("unbind", 1, js_ZMQSocket_unbind),
    JS_CFUNC_DEF("disconnect", 1, js_ZMQSocket_disconnect),
    JS_CFUNC_DEF("send", 2, js_ZMQSocket_send),
    JS_CFUNC_DEF("recv", 1, js_ZMQSocket_recv),
    // int zmq_setsockopt(void *socket, int option, const void *optval, size_t optvallen);
    // int zmq_getsockopt(void *socket, int option, void *optval, size_t *optvallen);
};

/*********************************************************************/
static void init_ZMQ(JSContext *ctx, JSValueConst global_object)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSValue ZMQ_proto = JS_NewObject(ctx);
    JSValue jZMQ_ctor = JS_NewCFunction2(ctx, ZMQ_ctor, "ZMQ", 0, JS_CFUNC_constructor, 0);
    JS_NewClassID(&js_ZMQ_class_id);
    JS_NewClass(rt, js_ZMQ_class_id, &js_ZMQ_class);
    JS_SetPropertyFunctionList(ctx, ZMQ_proto, js_ZMQ_proto_funcs, FF_ARRAY_ELEMS(js_ZMQ_proto_funcs));
    JS_SetConstructor(ctx, jZMQ_ctor, ZMQ_proto);
    JS_SetPropertyFunctionList(ctx, jZMQ_ctor, js_ZMQ_obj, FF_ARRAY_ELEMS(js_ZMQ_obj));
    JS_SetClassProto(ctx, js_ZMQ_class_id, ZMQ_proto);
    JS_SetPropertyStr(ctx, global_object, "ZMQ", jZMQ_ctor);
}

static void init_ZMQSocket(JSContext *ctx)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSValue ZMQSocket_proto = JS_NewObject(ctx);
    JS_NewClassID(&js_ZMQSocket_class_id);
    JS_NewClass(rt, js_ZMQSocket_class_id, &js_ZMQSocket_class);
    JS_SetPropertyFunctionList(ctx, ZMQSocket_proto, js_ZMQSocket_proto_funcs, FF_ARRAY_ELEMS(js_ZMQSocket_proto_funcs));
    JS_SetClassProto(ctx, js_ZMQSocket_class_id, ZMQSocket_proto);
}

/*********************************************************************/
void ff_quickjs_zmq_init(JSContext *ctx, JSValueConst global_object)
{
    init_ZMQ(ctx, global_object);
    init_ZMQSocket(ctx);
}
