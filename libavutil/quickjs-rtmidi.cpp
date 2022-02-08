/*
 * Copyright (C) 2022 Ramiro Polla
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

#include <math.h>

#include "libavutil/rtmidi/RtMidi.h"

extern "C" {
#include "libavutil/quickjs/quickjs-libc.h"
#include "libavutil/quickjs-rtmidi.h"
#include "internal.h"
#include "log.h"
#include "mem.h"
}

/*********************************************************************/
/* RtMidi */
static const JSCFunctionListEntry js_RtMidi_funcs[] = {
    JS_PROP_INT32_DEF("UNSPECIFIED",       RtMidi::UNSPECIFIED,  JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("MACOSX_CORE",       RtMidi::MACOSX_CORE,  JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("LINUX_ALSA",        RtMidi::LINUX_ALSA,   JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("UNIX_JACK",         RtMidi::UNIX_JACK,    JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("WINDOWS_MM",        RtMidi::WINDOWS_MM,   JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("RTMIDI_DUMMY",      RtMidi::RTMIDI_DUMMY, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("WEB_MIDI_API",      RtMidi::WEB_MIDI_API, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("NUM_APIS",          RtMidi::NUM_APIS,     JS_PROP_CONFIGURABLE),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "RtMidi", JS_PROP_CONFIGURABLE),
};

static const JSCFunctionListEntry js_RtMidi_obj[] = {
    JS_OBJECT_DEF("RtMidi", js_RtMidi_funcs, FF_ARRAY_ELEMS(js_RtMidi_funcs), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE ),
};

static void init_RtMidi(JSContext *ctx, JSValueConst global_object)
{
    JS_SetPropertyFunctionList(ctx, global_object, js_RtMidi_obj, FF_ARRAY_ELEMS(js_RtMidi_obj));
}

/*********************************************************************/
static const AVClass rtmidi_class = {
    .class_name = "quickjs-rtmidi",
    .item_name  = av_default_item_name,
    .version    = LIBAVUTIL_VERSION_INT,
};

typedef struct {
    const AVClass *klass;
    RtMidiIn *midiin;
} RtMidiInContext;

typedef struct {
    const AVClass *klass;
    RtMidiOut *midiout;
} RtMidiOutContext;

/*********************************************************************/
/* RtMidiIn */
static JSClassID js_RtMidiIn_class_id;

static JSValue RtMidiIn_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
    RtMidi::Api api = RtMidi::UNSPECIFIED;
    std::string clientName = "RtMidi Input Client";
    unsigned int queueSizeLimit = 1024;

    /* check arguments */
    if ( argc > 0 )
    {
        if ( !JS_IsInt32(argv[0]) )
        {
args_error:
            JS_ThrowTypeError(ctx, "RtMidiIn(RtMidi::Api api=UNSPECIFIED, const std::string& clientName = \"RtMidi Input Client\", unsigned int queueSizeLimit = 1024)");
            return JS_EXCEPTION;
        }
        api = (RtMidi::Api) JS_VALUE_GET_INT(argv[0]);
    }
    if ( argc > 1 )
    {
        if ( !JS_IsString(argv[1]) )
            goto args_error;
        const char *str = JS_ToCString(ctx, argv[1]);
        clientName = str;
        JS_FreeCString(ctx, str);
    }
    if ( argc > 2 )
    {
        if ( !JS_IsInt32(argv[2]) )
            goto args_error;
        queueSizeLimit = JS_VALUE_GET_INT(argv[2]);
    }

    /* using new_target to get the prototype is necessary when the
       class is extended. */
    JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if ( JS_IsException(proto) )
        return JS_EXCEPTION;
    JSValue obj = JS_NewObjectProtoClass(ctx, proto, js_RtMidiIn_class_id);
    JS_FreeValue(ctx, proto);
    if ( JS_IsException(obj) )
    {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }

    RtMidiInContext *rtmidictx = (RtMidiInContext *) av_mallocz(sizeof(RtMidiInContext));
    rtmidictx->klass = &rtmidi_class;
    rtmidictx->midiin = new RtMidiIn(api, clientName, queueSizeLimit);
    JS_SetOpaque(obj, rtmidictx);

    return obj;
}

static void js_RtMidiIn_finalizer(JSRuntime *rt, JSValue val)
{
    RtMidiInContext *rtmidictx = (RtMidiInContext *) JS_GetOpaque(val, js_RtMidiIn_class_id);
    RtMidiIn *midiin = rtmidictx->midiin;
    delete midiin;
    av_free(rtmidictx);
}

static JSValue js_RtMidiIn_getVersion(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiInContext *rtmidictx = (RtMidiInContext *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
    RtMidiIn *midiin = rtmidictx->midiin;
    if ( midiin == nullptr )
        return JS_EXCEPTION;
    std::string version = midiin->getVersion();
    return JS_NewString(ctx, version.c_str());
}

static JSValue js_RtMidiIn_getCompiledApi(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiInContext *rtmidictx = (RtMidiInContext *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
    RtMidiIn *midiin = rtmidictx->midiin;
    if ( midiin == nullptr )
        return JS_EXCEPTION;
    std::vector<RtMidi::Api> apis;
    midiin->getCompiledApi(apis);
    JSValue ret = JS_NewArray(ctx);
    for ( size_t i = 0; i < apis.size(); i++)
        JS_SetPropertyUint32(ctx, ret, i, JS_NewInt32(ctx, apis[i]));
    return ret;
}

static JSValue js_RtMidiIn_getApiName(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiInContext *rtmidictx = (RtMidiInContext *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
    RtMidiIn *midiin = rtmidictx->midiin;
    if ( midiin == nullptr )
        return JS_EXCEPTION;
    if ( argc != 1 || !JS_IsInt32(argv[0]) )
        return JS_EXCEPTION;
    RtMidi::Api api = (RtMidi::Api) JS_VALUE_GET_INT(argv[0]);
    std::string apiName = midiin->getApiName(api);
    return JS_NewString(ctx, apiName.c_str());
}

static JSValue js_RtMidiIn_getApiDisplayName(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiInContext *rtmidictx = (RtMidiInContext *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
    RtMidiIn *midiin = rtmidictx->midiin;
    if ( midiin == nullptr )
        return JS_EXCEPTION;
    if ( argc != 1 || !JS_IsInt32(argv[0]) )
        return JS_EXCEPTION;
    RtMidi::Api api = (RtMidi::Api) JS_VALUE_GET_INT(argv[0]);
    std::string apiDisplayName = midiin->getApiDisplayName(api);
    return JS_NewString(ctx, apiDisplayName.c_str());
}

static JSValue js_RtMidiIn_getCompiledApiByName(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiInContext *rtmidictx = (RtMidiInContext *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
    RtMidiIn *midiin = rtmidictx->midiin;
    if ( midiin == nullptr )
        return JS_EXCEPTION;
    if ( argc != 1 || !JS_IsString(argv[0]) )
        return JS_EXCEPTION;
    const char *str = JS_ToCString(ctx, argv[0]);
    std::string name = str;
    JS_FreeCString(ctx, str);
    RtMidi::Api api = midiin->getCompiledApiByName(name);
    return JS_NewInt32(ctx, api);
}

static JSValue js_RtMidiIn_setClientName(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiInContext *rtmidictx = (RtMidiInContext *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
    RtMidiIn *midiin = rtmidictx->midiin;
    if ( midiin == nullptr )
        return JS_EXCEPTION;
    if ( argc != 1 || !JS_IsString(argv[0]) )
        return JS_EXCEPTION;
    const char *str = JS_ToCString(ctx, argv[0]);
    std::string name = str;
    JS_FreeCString(ctx, str);
    midiin->setClientName(name);
    return JS_TRUE;
}

static JSValue js_RtMidiIn_setPortName(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiInContext *rtmidictx = (RtMidiInContext *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
    RtMidiIn *midiin = rtmidictx->midiin;
    if ( midiin == nullptr )
        return JS_EXCEPTION;
    if ( argc != 1 || !JS_IsString(argv[0]) )
        return JS_EXCEPTION;
    const char *str = JS_ToCString(ctx, argv[0]);
    std::string name = str;
    JS_FreeCString(ctx, str);
    midiin->setPortName(name);
    return JS_TRUE;
}

static JSValue js_RtMidiIn_getCurrentApi(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiInContext *rtmidictx = (RtMidiInContext *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
    RtMidiIn *midiin = rtmidictx->midiin;
    if ( midiin == nullptr )
        return JS_EXCEPTION;
    RtMidi::Api api = midiin->getCurrentApi();
    return JS_NewInt32(ctx, api);
}

static JSValue js_RtMidiIn_openPort(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiInContext *rtmidictx = (RtMidiInContext *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
    RtMidiIn *midiin = rtmidictx->midiin;
    if ( midiin == nullptr )
        return JS_EXCEPTION;
    unsigned int portNumber = 0;
    std::string portName = "RtMidi Input";
    /* check arguments */
    if ( argc > 0 )
    {
        if ( !JS_IsInt32(argv[0]) )
            return JS_EXCEPTION;
        portNumber = JS_VALUE_GET_INT(argv[0]);
    }
    if ( argc > 1 )
    {
        if ( !JS_IsString(argv[1]) )
            return JS_EXCEPTION;
        const char *str = JS_ToCString(ctx, argv[1]);
        portName = str;
        JS_FreeCString(ctx, str);
    }
    try {
        midiin->openPort(portNumber, portName);
        return JS_TRUE;
    } catch (const RtMidiError &err) {
        av_log(rtmidictx, AV_LOG_ERROR, "%s", err.what());
        return JS_NULL;
    }
}

static JSValue js_RtMidiIn_openVirtualPort(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiInContext *rtmidictx = (RtMidiInContext *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
    RtMidiIn *midiin = rtmidictx->midiin;
    if ( midiin == nullptr )
        return JS_EXCEPTION;
    std::string portName = "RtMidi Input";
    /* check arguments */
    if ( argc > 0 )
    {
        if ( !JS_IsString(argv[0]) )
            return JS_EXCEPTION;
        const char *str = JS_ToCString(ctx, argv[1]);
        portName = str;
        JS_FreeCString(ctx, str);
    }
    try {
        midiin->openVirtualPort(portName);
    } catch (const RtMidiError &err) {
        av_log(rtmidictx, AV_LOG_ERROR, "%s", err.what());
        return JS_NULL;
    }
    return JS_TRUE;
}

static JSValue js_RtMidiIn_closePort(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiInContext *rtmidictx = (RtMidiInContext *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
    RtMidiIn *midiin = rtmidictx->midiin;
    if ( midiin == nullptr )
        return JS_EXCEPTION;
    try {
        midiin->closePort();
    } catch (const RtMidiError &err) {
        av_log(rtmidictx, AV_LOG_ERROR, "%s", err.what());
        return JS_NULL;
    }
    return JS_TRUE;
}

static JSValue js_RtMidiIn_isPortOpen(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiInContext *rtmidictx = (RtMidiInContext *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
    RtMidiIn *midiin = rtmidictx->midiin;
    if ( midiin == nullptr )
        return JS_EXCEPTION;
    try {
        bool ret = midiin->isPortOpen();
        return JS_NewBool(ctx, ret);
    } catch (const RtMidiError &err) {
        av_log(rtmidictx, AV_LOG_ERROR, "%s", err.what());
        return JS_NULL;
    }
}

static JSValue js_RtMidiIn_getPortCount(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiInContext *rtmidictx = (RtMidiInContext *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
    RtMidiIn *midiin = rtmidictx->midiin;
    if ( midiin == nullptr )
        return JS_EXCEPTION;
    try {
        int ret = midiin->getPortCount();
        return JS_NewInt32(ctx, ret);
    } catch (const RtMidiError &err) {
        av_log(rtmidictx, AV_LOG_ERROR, "%s", err.what());
        return JS_NULL;
    }
}

static JSValue js_RtMidiIn_getPortName(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiInContext *rtmidictx = (RtMidiInContext *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
    RtMidiIn *midiin = rtmidictx->midiin;
    if ( midiin == nullptr )
        return JS_EXCEPTION;
    unsigned int portNumber = 0;
    /* check arguments */
    if ( argc > 0 )
    {
        if ( !JS_IsInt32(argv[0]) )
            return JS_EXCEPTION;
        portNumber = JS_VALUE_GET_INT(argv[0]);
    }
    try {
        std::string name = midiin->getPortName(portNumber);
        return JS_NewString(ctx, name.c_str());
    } catch (const RtMidiError &err) {
        av_log(rtmidictx, AV_LOG_ERROR, "%s", err.what());
        return JS_NULL;
    }
}

static JSValue js_RtMidiIn_ignoreTypes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiInContext *rtmidictx = (RtMidiInContext *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
    RtMidiIn *midiin = rtmidictx->midiin;
    if ( midiin == nullptr )
        return JS_EXCEPTION;
    bool midiSysex = true;
    bool midiTime = true;
    bool midiSense = true;
    /* check arguments */
    if ( argc > 0 )
    {
        if ( !JS_IsBool(argv[0]) )
            return JS_EXCEPTION;
        midiSysex = JS_VALUE_GET_INT(argv[0]);
    }
    if ( argc > 1 )
    {
        if ( !JS_IsBool(argv[1]) )
            return JS_EXCEPTION;
        midiTime = JS_VALUE_GET_INT(argv[1]);
    }
    if ( argc > 2 )
    {
        if ( !JS_IsBool(argv[2]) )
            return JS_EXCEPTION;
        midiSense = JS_VALUE_GET_INT(argv[2]);
    }
    try {
        midiin->ignoreTypes(midiSysex, midiTime, midiSense);
        return JS_TRUE;
    } catch (const RtMidiError &err) {
        av_log(rtmidictx, AV_LOG_ERROR, "%s", err.what());
        return JS_NULL;
    }
}

static JSValue js_RtMidiIn_getMessage(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiInContext *rtmidictx = (RtMidiInContext *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
    RtMidiIn *midiin = rtmidictx->midiin;
    if ( midiin == nullptr )
        return JS_EXCEPTION;
    try {
        std::vector<unsigned char> message;
        double delta = midiin->getMessage(&message);
        size_t length = message.size();
        JSValue ret;
        if ( length == 0 )
        {
            ret = JS_NewArray(ctx);
        }
        else
        {
            JSValue *parray;
            ret = JS_NewFastArray(ctx, &parray, length, 0);
            for ( size_t i = 0; i < length; i++ )
                parray[i] = JS_NewInt32(ctx, message[i]);
        }
        JS_SetPropertyStr(ctx, ret, "delta", JS_NewFloat64(ctx, delta));
        return ret;
    } catch (const RtMidiError &err) {
        av_log(rtmidictx, AV_LOG_ERROR, "%s", err.what());
        return JS_NULL;
    }
}

static JSClassDef js_RtMidiIn_class = {
    "RtMidiIn",
    .finalizer = js_RtMidiIn_finalizer,
};

static const JSCFunctionListEntry js_RtMidiIn_proto_funcs[] = {
    JS_CFUNC_DEF("getVersion", 0, js_RtMidiIn_getVersion),
    JS_CFUNC_DEF("getCompiledApi", 0, js_RtMidiIn_getCompiledApi),
    JS_CFUNC_DEF("getApiName", 1, js_RtMidiIn_getApiName),
    JS_CFUNC_DEF("getApiDisplayName", 1, js_RtMidiIn_getApiDisplayName),
    JS_CFUNC_DEF("getCompiledApiByName", 1, js_RtMidiIn_getCompiledApiByName),
    JS_CFUNC_DEF("setClientName", 1, js_RtMidiIn_setClientName),
    JS_CFUNC_DEF("setPortName", 1, js_RtMidiIn_setPortName),
    JS_CFUNC_DEF("getCurrentApi", 0, js_RtMidiIn_getCurrentApi),
    JS_CFUNC_DEF("openPort", 2, js_RtMidiIn_openPort),
    JS_CFUNC_DEF("openVirtualPort", 1, js_RtMidiIn_openVirtualPort),
    JS_CFUNC_DEF("closePort", 0, js_RtMidiIn_closePort),
    JS_CFUNC_DEF("isPortOpen", 0, js_RtMidiIn_isPortOpen),
    JS_CFUNC_DEF("getPortCount", 0, js_RtMidiIn_getPortCount),
    JS_CFUNC_DEF("getPortName", 1, js_RtMidiIn_getPortName),
    JS_CFUNC_DEF("ignoreTypes", 3, js_RtMidiIn_ignoreTypes),
    JS_CFUNC_DEF("getMessage", 0, js_RtMidiIn_getMessage),
};

static void init_RtMidiIn(JSContext *ctx, JSValueConst global_object)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSValue RtMidiIn_proto = JS_NewObject(ctx);
    JSValue jRtMidiIn_ctor = JS_NewCFunction2(ctx, RtMidiIn_ctor, "RtMidiIn", 3, JS_CFUNC_constructor, 0);
    JS_NewClassID(&js_RtMidiIn_class_id);
    JS_NewClass(rt, js_RtMidiIn_class_id, &js_RtMidiIn_class);
    JS_SetPropertyFunctionList(ctx, RtMidiIn_proto, js_RtMidiIn_proto_funcs, FF_ARRAY_ELEMS(js_RtMidiIn_proto_funcs));
    JS_SetConstructor(ctx, jRtMidiIn_ctor, RtMidiIn_proto);
    JS_SetClassProto(ctx, js_RtMidiIn_class_id, RtMidiIn_proto);
    JS_SetPropertyStr(ctx, global_object, "RtMidiIn", jRtMidiIn_ctor);
}

/*********************************************************************/
/* RtMidiOut */
static JSClassID js_RtMidiOut_class_id;

static JSValue RtMidiOut_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
    RtMidi::Api api = RtMidi::UNSPECIFIED;
    std::string clientName = "RtMidi Output Client";

    /* check arguments */
    if ( argc > 0 )
    {
        if ( !JS_IsInt32(argv[0]) )
        {
args_error:
            JS_ThrowTypeError(ctx, "RtMidiOut(RtMidi::Api api=UNSPECIFIED, const std::string& clientName = \"RtMidi Output Client\")");
            return JS_EXCEPTION;
        }
        api = (RtMidi::Api) JS_VALUE_GET_INT(argv[0]);
    }
    if ( argc > 1 )
    {
        if ( !JS_IsString(argv[1]) )
            goto args_error;
        const char *str = JS_ToCString(ctx, argv[1]);
        clientName = str;
        JS_FreeCString(ctx, str);
    }

    /* using new_target to get the prototype is necessary when the
       class is extended. */
    JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if ( JS_IsException(proto) )
        return JS_EXCEPTION;
    JSValue obj = JS_NewObjectProtoClass(ctx, proto, js_RtMidiOut_class_id);
    JS_FreeValue(ctx, proto);
    if ( JS_IsException(obj) )
    {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }

    RtMidiOutContext *rtmidictx = (RtMidiOutContext *) av_mallocz(sizeof(RtMidiOutContext));
    rtmidictx->klass = &rtmidi_class;
    rtmidictx->midiout = new RtMidiOut(api, clientName);
    JS_SetOpaque(obj, rtmidictx);

    return obj;
}

static void js_RtMidiOut_finalizer(JSRuntime *rt, JSValue val)
{
    RtMidiOutContext *rtmidictx = (RtMidiOutContext *) JS_GetOpaque(val, js_RtMidiOut_class_id);
    RtMidiOut *midiout = rtmidictx->midiout;
    delete midiout;
    av_free(rtmidictx);
}

static JSValue js_RtMidiOut_getCurrentApi(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiOutContext *rtmidictx = (RtMidiOutContext *) JS_GetOpaque(this_val, js_RtMidiOut_class_id);
    RtMidiOut *midiout = rtmidictx->midiout;
    if ( midiout == nullptr )
        return JS_EXCEPTION;
    RtMidi::Api api = midiout->getCurrentApi();
    return JS_NewInt32(ctx, api);
}

static JSValue js_RtMidiOut_openPort(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiOutContext *rtmidictx = (RtMidiOutContext *) JS_GetOpaque(this_val, js_RtMidiOut_class_id);
    RtMidiOut *midiout = rtmidictx->midiout;
    if ( midiout == nullptr )
        return JS_EXCEPTION;
    unsigned int portNumber = 0;
    std::string portName = "RtMidi Input";
    /* check arguments */
    if ( argc > 0 )
    {
        if ( !JS_IsInt32(argv[0]) )
            return JS_EXCEPTION;
        portNumber = JS_VALUE_GET_INT(argv[0]);
    }
    if ( argc > 1 )
    {
        if ( !JS_IsString(argv[1]) )
            return JS_EXCEPTION;
        const char *str = JS_ToCString(ctx, argv[1]);
        portName = str;
        JS_FreeCString(ctx, str);
    }
    try {
        midiout->openPort(portNumber, portName);
        return JS_TRUE;
    } catch (const RtMidiError &err) {
        av_log(rtmidictx, AV_LOG_ERROR, "%s", err.what());
        return JS_NULL;
    }
}

static JSValue js_RtMidiOut_openVirtualPort(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiOutContext *rtmidictx = (RtMidiOutContext *) JS_GetOpaque(this_val, js_RtMidiOut_class_id);
    RtMidiOut *midiout = rtmidictx->midiout;
    if ( midiout == nullptr )
        return JS_EXCEPTION;
    std::string portName = "RtMidi Input";
    /* check arguments */
    if ( argc > 0 )
    {
        if ( !JS_IsString(argv[0]) )
            return JS_EXCEPTION;
        const char *str = JS_ToCString(ctx, argv[1]);
        portName = str;
        JS_FreeCString(ctx, str);
    }
    try {
        midiout->openVirtualPort(portName);
    } catch (const RtMidiError &err) {
        av_log(rtmidictx, AV_LOG_ERROR, "%s", err.what());
        return JS_NULL;
    }
    return JS_TRUE;
}

static JSValue js_RtMidiOut_closePort(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiOutContext *rtmidictx = (RtMidiOutContext *) JS_GetOpaque(this_val, js_RtMidiOut_class_id);
    RtMidiOut *midiout = rtmidictx->midiout;
    if ( midiout == nullptr )
        return JS_EXCEPTION;
    try {
        midiout->closePort();
    } catch (const RtMidiError &err) {
        av_log(rtmidictx, AV_LOG_ERROR, "%s", err.what());
        return JS_NULL;
    }
    return JS_TRUE;
}

static JSValue js_RtMidiOut_isPortOpen(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiOutContext *rtmidictx = (RtMidiOutContext *) JS_GetOpaque(this_val, js_RtMidiOut_class_id);
    RtMidiOut *midiout = rtmidictx->midiout;
    if ( midiout == nullptr )
        return JS_EXCEPTION;
    try {
        bool ret = midiout->isPortOpen();
        return JS_NewBool(ctx, ret);
    } catch (const RtMidiError &err) {
        av_log(rtmidictx, AV_LOG_ERROR, "%s", err.what());
        return JS_NULL;
    }
}

static JSValue js_RtMidiOut_getPortCount(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiOutContext *rtmidictx = (RtMidiOutContext *) JS_GetOpaque(this_val, js_RtMidiOut_class_id);
    RtMidiOut *midiout = rtmidictx->midiout;
    if ( midiout == nullptr )
        return JS_EXCEPTION;
    try {
        int ret = midiout->getPortCount();
        return JS_NewInt32(ctx, ret);
    } catch (const RtMidiError &err) {
        av_log(rtmidictx, AV_LOG_ERROR, "%s", err.what());
        return JS_NULL;
    }
}

static JSValue js_RtMidiOut_getPortName(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiOutContext *rtmidictx = (RtMidiOutContext *) JS_GetOpaque(this_val, js_RtMidiOut_class_id);
    RtMidiOut *midiout = rtmidictx->midiout;
    if ( midiout == nullptr )
        return JS_EXCEPTION;
    unsigned int portNumber = 0;
    /* check arguments */
    if ( argc > 0 )
    {
        if ( !JS_IsInt32(argv[0]) )
            return JS_EXCEPTION;
        portNumber = JS_VALUE_GET_INT(argv[0]);
    }
    try {
        std::string name = midiout->getPortName(portNumber);
        return JS_NewString(ctx, name.c_str());
    } catch (const RtMidiError &err) {
        av_log(rtmidictx, AV_LOG_ERROR, "%s", err.what());
        return JS_NULL;
    }
}

static JSValue js_RtMidiOut_sendMessage(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
//    void sendMessage( const std::vector<unsigned char> *message );

    RtMidiOutContext *rtmidictx = (RtMidiOutContext *) JS_GetOpaque(this_val, js_RtMidiOut_class_id);
    RtMidiOut *midiout = rtmidictx->midiout;
    if ( midiout == nullptr )
        return JS_EXCEPTION;
    uint32_t length;
    JSValue *parray = NULL;
    if ( argc != 1 || !JS_GetFastArray(argv[0], &parray, &length) || length == 0 )
        return JS_EXCEPTION;
    try {
        std::vector<unsigned char> message;
        for ( size_t i = 0; i < length; i++ )
            message.push_back(JS_VALUE_GET_INT(parray[i]));
        midiout->sendMessage(&message);
    } catch (const RtMidiError &err) {
        av_log(rtmidictx, AV_LOG_ERROR, "%s", err.what());
        return JS_NULL;
    }
    return JS_TRUE;
}

static JSClassDef js_RtMidiOut_class = {
    "RtMidiOut",
    .finalizer = js_RtMidiOut_finalizer,
};

static const JSCFunctionListEntry js_RtMidiOut_proto_funcs[] = {
    JS_CFUNC_DEF("getCurrentApi", 0, js_RtMidiOut_getCurrentApi),
    JS_CFUNC_DEF("openPort", 2, js_RtMidiOut_openPort),
    JS_CFUNC_DEF("openVirtualPort", 1, js_RtMidiOut_openVirtualPort),
    JS_CFUNC_DEF("closePort", 0, js_RtMidiOut_closePort),
    JS_CFUNC_DEF("isPortOpen", 0, js_RtMidiOut_isPortOpen),
    JS_CFUNC_DEF("getPortCount", 0, js_RtMidiOut_getPortCount),
    JS_CFUNC_DEF("getPortName", 1, js_RtMidiOut_getPortName),
    JS_CFUNC_DEF("sendMessage", 1, js_RtMidiOut_sendMessage),
};

static void init_RtMidiOut(JSContext *ctx, JSValueConst global_object)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSValue RtMidiOut_proto = JS_NewObject(ctx);
    JSValue jRtMidiOut_ctor = JS_NewCFunction2(ctx, RtMidiOut_ctor, "RtMidiOut", 3, JS_CFUNC_constructor, 0);
    JS_NewClassID(&js_RtMidiOut_class_id);
    JS_NewClass(rt, js_RtMidiOut_class_id, &js_RtMidiOut_class);
    JS_SetPropertyFunctionList(ctx, RtMidiOut_proto, js_RtMidiOut_proto_funcs, FF_ARRAY_ELEMS(js_RtMidiOut_proto_funcs));
    JS_SetConstructor(ctx, jRtMidiOut_ctor, RtMidiOut_proto);
    JS_SetClassProto(ctx, js_RtMidiOut_class_id, RtMidiOut_proto);
    JS_SetPropertyStr(ctx, global_object, "RtMidiOut", jRtMidiOut_ctor);
}

/*********************************************************************/
extern "C"
void ff_quickjs_rtmidi_init(JSContext *ctx, JSValueConst global_object)
{
    init_RtMidi(ctx, global_object);
    init_RtMidiIn(ctx, global_object);
    init_RtMidiOut(ctx, global_object);
}
