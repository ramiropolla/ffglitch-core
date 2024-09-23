/*
 * Copyright (C) 2022-2024 Ramiro Polla
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
#include "RtMidi.h"

/*********************************************************************/
#include "quickjs-rtmidi.h"
#include "cutils.h"

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
};

/*********************************************************************/
static JSValue
ThrowRtMidiError(JSContext *ctx, const RtMidiError &err)
{
    JSValue ret = JS_NewError(ctx);
    JS_DefinePropertyValueStr(ctx, ret, "message",
                              JS_NewString(ctx, err.what()),
                              JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
    if ( JS_IsException(ret) )
        ret = JS_NULL;
    return JS_Throw(ctx, ret);
}

/*********************************************************************/
/* RtMidiIn */
static JSClassID js_RtMidiIn_class_id;

static JSValue js_RtMidiIn_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
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

    RtMidiIn *midiin = new RtMidiIn(api, clientName, queueSizeLimit);
    JS_SetOpaque(obj, midiin);

    return obj;
}

static void js_RtMidiIn_finalizer(JSRuntime *rt, JSValue val)
{
    RtMidiIn *midiin = (RtMidiIn *) JS_GetOpaque(val, js_RtMidiIn_class_id);
    delete midiin;
}

static JSValue js_RtMidiIn_getVersion(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiIn *midiin = (RtMidiIn *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
    if ( midiin == nullptr )
        return JS_EXCEPTION;
    std::string version = midiin->getVersion();
    return JS_NewString(ctx, version.c_str());
}

static JSValue js_RtMidiIn_getCompiledApi(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiIn *midiin = (RtMidiIn *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
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
    RtMidiIn *midiin = (RtMidiIn *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
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
    RtMidiIn *midiin = (RtMidiIn *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
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
    RtMidiIn *midiin = (RtMidiIn *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
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
    RtMidiIn *midiin = (RtMidiIn *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
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
    RtMidiIn *midiin = (RtMidiIn *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
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
    RtMidiIn *midiin = (RtMidiIn *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
    if ( midiin == nullptr )
        return JS_EXCEPTION;
    RtMidi::Api api = midiin->getCurrentApi();
    return JS_NewInt32(ctx, api);
}

static JSValue js_RtMidiIn_openPort(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiIn *midiin = (RtMidiIn *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
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
        return ThrowRtMidiError(ctx, err);
    }
}

static JSValue js_RtMidiIn_openVirtualPort(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiIn *midiin = (RtMidiIn *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
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
        return ThrowRtMidiError(ctx, err);
    }
    return JS_TRUE;
}

static JSValue js_RtMidiIn_closePort(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiIn *midiin = (RtMidiIn *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
    if ( midiin == nullptr )
        return JS_EXCEPTION;
    try {
        midiin->closePort();
    } catch (const RtMidiError &err) {
        return ThrowRtMidiError(ctx, err);
    }
    return JS_TRUE;
}

static JSValue js_RtMidiIn_isPortOpen(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiIn *midiin = (RtMidiIn *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
    if ( midiin == nullptr )
        return JS_EXCEPTION;
    try {
        bool ret = midiin->isPortOpen();
        return JS_NewBool(ctx, ret);
    } catch (const RtMidiError &err) {
        return ThrowRtMidiError(ctx, err);
    }
}

static JSValue js_RtMidiIn_getPortCount(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiIn *midiin = (RtMidiIn *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
    if ( midiin == nullptr )
        return JS_EXCEPTION;
    try {
        int ret = midiin->getPortCount();
        return JS_NewInt32(ctx, ret);
    } catch (const RtMidiError &err) {
        return ThrowRtMidiError(ctx, err);
    }
}

static JSValue js_RtMidiIn_getPortName(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiIn *midiin = (RtMidiIn *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
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
        return ThrowRtMidiError(ctx, err);
    }
}

static JSValue js_RtMidiIn_ignoreTypes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiIn *midiin = (RtMidiIn *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
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
        return ThrowRtMidiError(ctx, err);
    }
}

static JSValue js_RtMidiIn_getMessage(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiIn *midiin = (RtMidiIn *) JS_GetOpaque(this_val, js_RtMidiIn_class_id);
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
        return ThrowRtMidiError(ctx, err);
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

/*********************************************************************/
/* RtMidiOut */
static JSClassID js_RtMidiOut_class_id;

static JSValue js_RtMidiOut_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
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

    RtMidiOut *midiout = new RtMidiOut(api, clientName);
    JS_SetOpaque(obj, midiout);

    return obj;
}

static void js_RtMidiOut_finalizer(JSRuntime *rt, JSValue val)
{
    RtMidiOut *midiout = (RtMidiOut *) JS_GetOpaque(val, js_RtMidiOut_class_id);
    delete midiout;
}

static JSValue js_RtMidiOut_getCurrentApi(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiOut *midiout = (RtMidiOut *) JS_GetOpaque(this_val, js_RtMidiOut_class_id);
    if ( midiout == nullptr )
        return JS_EXCEPTION;
    RtMidi::Api api = midiout->getCurrentApi();
    return JS_NewInt32(ctx, api);
}

static JSValue js_RtMidiOut_openPort(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiOut *midiout = (RtMidiOut *) JS_GetOpaque(this_val, js_RtMidiOut_class_id);
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
        return ThrowRtMidiError(ctx, err);
    }
}

static JSValue js_RtMidiOut_openVirtualPort(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiOut *midiout = (RtMidiOut *) JS_GetOpaque(this_val, js_RtMidiOut_class_id);
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
        return ThrowRtMidiError(ctx, err);
    }
    return JS_TRUE;
}

static JSValue js_RtMidiOut_closePort(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiOut *midiout = (RtMidiOut *) JS_GetOpaque(this_val, js_RtMidiOut_class_id);
    if ( midiout == nullptr )
        return JS_EXCEPTION;
    try {
        midiout->closePort();
    } catch (const RtMidiError &err) {
        return ThrowRtMidiError(ctx, err);
    }
    return JS_TRUE;
}

static JSValue js_RtMidiOut_isPortOpen(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiOut *midiout = (RtMidiOut *) JS_GetOpaque(this_val, js_RtMidiOut_class_id);
    if ( midiout == nullptr )
        return JS_EXCEPTION;
    try {
        bool ret = midiout->isPortOpen();
        return JS_NewBool(ctx, ret);
    } catch (const RtMidiError &err) {
        return ThrowRtMidiError(ctx, err);
    }
}

static JSValue js_RtMidiOut_getPortCount(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiOut *midiout = (RtMidiOut *) JS_GetOpaque(this_val, js_RtMidiOut_class_id);
    if ( midiout == nullptr )
        return JS_EXCEPTION;
    try {
        int ret = midiout->getPortCount();
        return JS_NewInt32(ctx, ret);
    } catch (const RtMidiError &err) {
        return ThrowRtMidiError(ctx, err);
    }
}

static JSValue js_RtMidiOut_getPortName(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    RtMidiOut *midiout = (RtMidiOut *) JS_GetOpaque(this_val, js_RtMidiOut_class_id);
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
        return ThrowRtMidiError(ctx, err);
    }
}

static JSValue js_RtMidiOut_sendMessage(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
//    void sendMessage( const std::vector<unsigned char> *message );

    RtMidiOut *midiout = (RtMidiOut *) JS_GetOpaque(this_val, js_RtMidiOut_class_id);
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
        return ThrowRtMidiError(ctx, err);
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

/*********************************************************************/
static int js_RtMidiIn_init(JSContext *ctx, JSModuleDef *m)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSValue RtMidiIn_proto = JS_NewObject(ctx);
    JSValue jRtMidiIn_ctor = JS_NewCFunction2(ctx, js_RtMidiIn_ctor, "In", 0, JS_CFUNC_constructor, 0);
    JS_NewClassID(&js_RtMidiIn_class_id);
    JS_NewClass(rt, js_RtMidiIn_class_id, &js_RtMidiIn_class);
    JS_SetPropertyFunctionList(ctx, RtMidiIn_proto, js_RtMidiIn_proto_funcs, countof(js_RtMidiIn_proto_funcs));
    JS_SetConstructor(ctx, jRtMidiIn_ctor, RtMidiIn_proto);
    JS_SetClassProto(ctx, js_RtMidiIn_class_id, RtMidiIn_proto);
    JS_SetModuleExport(ctx, m, "In", jRtMidiIn_ctor);
    return 0;
}

static int js_RtMidiOut_init(JSContext *ctx, JSModuleDef *m)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSValue RtMidiOut_proto = JS_NewObject(ctx);
    JSValue jRtMidiOut_ctor = JS_NewCFunction2(ctx, js_RtMidiOut_ctor, "Out", 3, JS_CFUNC_constructor, 0);
    JS_NewClassID(&js_RtMidiOut_class_id);
    JS_NewClass(rt, js_RtMidiOut_class_id, &js_RtMidiOut_class);
    JS_SetPropertyFunctionList(ctx, RtMidiOut_proto, js_RtMidiOut_proto_funcs, countof(js_RtMidiOut_proto_funcs));
    JS_SetConstructor(ctx, jRtMidiOut_ctor, RtMidiOut_proto);
    JS_SetClassProto(ctx, js_RtMidiOut_class_id, RtMidiOut_proto);
    JS_SetModuleExport(ctx, m, "Out", jRtMidiOut_ctor);
    return 0;
}

static int js_rtmidi_init(JSContext *ctx, JSModuleDef *m)
{
    JS_SetModuleExportList(ctx, m, js_RtMidi_funcs, countof(js_RtMidi_funcs));
    js_RtMidiIn_init(ctx, m);
    js_RtMidiOut_init(ctx, m);
    return 0;
}

extern "C"
JSModuleDef *js_init_module_rtmidi(JSContext *ctx, const char *module_name)
{
    JSModuleDef *m = JS_NewCModule(ctx, module_name, js_rtmidi_init);
    if ( m == NULL )
        return NULL;
    JS_AddModuleExportList(ctx, m, js_RtMidi_funcs, countof(js_RtMidi_funcs));
    JS_AddModuleExport(ctx, m, "In");
    JS_AddModuleExport(ctx, m, "Out");
    return m;
}
