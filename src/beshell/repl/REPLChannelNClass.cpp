#include "REPLChannelNClass.hpp"
#include "REPLChannel.hpp"
#include "BeShell.class.hpp"
#include <cstdint>

using namespace std ;

namespace be {

    /**
     * REPL 通道类
     * 
     * REPLChannel 是 REPL 通信的抽象通道，继承自 EventEmitter。
     * 用于创建自定义的 REPL 通信通道，处理 REPL 协议数据包。
     * 
     * ## 事件
     * 
     * | 事件名 | 说明 | 回调参数 |
     * |--------|------|----------|
     * | `output-stream` | 输出数据流 | `data:ArrayBuffer` |
     * | `output` | 输出事件（需启用） | `data:ArrayBuffer` |
     * 
     * ## 使用场景
     * 
     * 通常用于创建自定义传输层的 REPL 通道，如：
     * - WebSocket REPL
     * - MQTT REPL
     * - 自定义串口协议
     * 
     * 示例：
     * ```javascript
     * import * as repl from "repl"
     * import * as mg from "mg"
     * 
     * // 创建 WebSocket REPL 通道
     * const channel = new repl.REPLChannel()
     * 
     * // 连接到 WebSocket 服务器
     * const conn = mg.connect("ws://192.168.1.100:8080", (ev, data, isBinary) => {
     *     if (ev == "ws.msg") {
     *         // 将收到的数据传递给 REPL 通道处理
     *         channel.process(data)
     *     }
     * })
     * 
     * // 将 REPL 输出发送到 WebSocket
     * channel.on("output-stream", (data) => {
     *     if (conn.isConnected()) {
     *         conn.send(data)
     *     }
     * })
     * 
     * // 启用输出事件（可选）
     * channel.enableEventOutput()
     * ```
     * 
     * @class REPLChannel
     * @module repl
     * @extends EventEmitter
     */
    DEFINE_NCLASS_META_NAME(REPLChannelNClass, EventEmitter, "REPLChannel")
    std::vector<JSCFunctionListEntry> REPLChannelNClass::methods = {
        JS_CFUNC_DEF("process", 0, REPLChannelNClass::process),
        JS_CFUNC_DEF("enableEventOutput", 0, REPLChannelNClass::enableEventOutput),
    } ;

    REPLChannelNClass::REPLChannelNClass(JSContext * ctx, JSValue _jsobj)
        : EventEmitter(ctx,build(ctx,_jsobj))
        , REPLChannel(JSEngine::fromJSContext(ctx)->beshell->repl)
        , parser([this](std::unique_ptr<Package> pkg, void * opaque) {
            assert(repl) ;
            repl->onReceived(this, std::move(pkg)) ;
        })
    {
        repl->addChannel((REPLChannel*)this) ;
    }

    REPLChannelNClass::~REPLChannelNClass(){
        repl->removeChannel((REPLChannel*)this) ;
    }

    JSValue REPLChannelNClass::constructor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        auto obj = new REPLChannelNClass(ctx) ;
        obj->shared() ;
        return obj->jsobj ;
    }

    /**
     * 处理 REPL 协议数据
     * 
     * 将接收到的数据传递给 REPL 协议解析器处理。
     * 支持字符串或 ArrayBuffer 格式的数据。
     * 
     * 解析后的命令会通过 REPL 系统执行，执行结果通过 `output-stream` 事件返回。
     * 
     * 示例：
     * ```javascript
     * import * as repl from "repl"
     * 
     * const channel = new repl.REPLChannel()
     * 
     * // 处理字符串数据
     * channel.process("console.log('hello')")
     * 
     * // 处理二进制数据（ArrayBuffer）
     * const buffer = new ArrayBuffer(100)
     * // ... 填充数据
     * channel.process(buffer)
     * ```
     * 
     * @module repl
     * @class REPLChannel
     * @method process
     * @param data:string|ArrayBuffer 要处理的 REPL 协议数据
     * @return undefined
     * @throws 参数必须是字符串或 ArrayBuffer
     */
    JSValue REPLChannelNClass::process(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        CHECK_ARGC(1)
        THIS_NCLASS(REPLChannelNClass, self)
        ARGV_TO_UINT8_OPT(1, cmd, Cmd::CALL)
        ARGV_TO_UINT8_OPT(2, pkgid, -1)

        if(JS_IsString(argv[0])) {
            size_t len = 0 ;
            const char * cstr = JS_ToCStringLen(ctx, &len, argv[0]) ;
            if(cstr == nullptr || len == 0) {
                JS_FreeCString(ctx, cstr) ;
                return JS_UNDEFINED ;
            }
            self->parser.parse((uint8_t *)cstr, len) ;
            JS_FreeCString(ctx, cstr) ;
        }
        else if(JS_IsArrayBuffer(argv[0])) {
            size_t len = 0 ;
            uint8_t * data = (uint8_t *)JS_GetArrayBuffer(ctx, &len, argv[0]) ;
            if(data == nullptr || len == 0) {
                return JS_UNDEFINED ;
            }
            self->parser.parse(data, len) ;
        }
        else {
            JSTHROW("arg must be string or ArrayBuffer") ;
        }

        return JS_UNDEFINED ;
    }

    
    void REPLChannelNClass::sendData (const char * data, size_t datalen) {
        Console::setChannel("serial") ;
        emitSyncFree("output-stream", {
            JS_NewArrayBufferCopy(ctx, (const uint8_t *)data, datalen)
        }) ;
        
        // emit output event
        if(enabledOutputEvent) {
            emitSyncFree("output", {
                JS_NewArrayBufferCopy(ctx, (uint8_t *)data, datalen)
            }) ;
        }

        Console::setChannel(nullptr) ;
    }

    void REPLChannelNClass::send (Package & pkg) {
        Console::setChannel("serial") ;

        size_t len = 0 ;
        uint8_t * data = pkg.toStream(&len) ;
        emitSyncFree("output-stream", {
            JS_NewArrayBuffer(ctx, data, len, freeArrayBuffer, NULL, false)
        }) ;

        // emit output event
        if(enabledOutputEvent && pkg.head.fields.cmd == Cmd::OUTPUT) {
            emitSyncFree("output", {
                JS_NewArrayBufferCopy(ctx, pkg.body(), pkg.body_len)
            }) ;
        }

        Console::setChannel(nullptr) ;
    }

    /**
     * 启用输出事件
     * 
     * 启用后，REPL 的输出内容会通过 `output` 事件发送，
     * 方便在 JS 层捕获和处理 REPL 的输出。
     * 
     * 默认情况下只有 `output-stream` 事件，启用后会额外触发 `output` 事件。
     * 
     * 示例：
     * ```javascript
     * import * as repl from "repl"
     * 
     * const channel = new repl.REPLChannel()
     * 
     * // 启用输出事件
     * channel.enableEventOutput()
     * 
     * // 监听输出事件
     * channel.on("output", (data) => {
     *     console.log("REPL 输出:", data)
     * })
     * 
     * // 同时 output-stream 事件仍然可用
     * channel.on("output-stream", (data) => {
     *     // 发送到传输层
     * })
     * ```
     * 
     * @module repl
     * @class REPLChannel
     * @method enableEventOutput
     * @return undefined
     */
    JSValue REPLChannelNClass::enableEventOutput(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(REPLChannelNClass, self)
        self->enabledOutputEvent = true ;
        return JS_UNDEFINED ;
    }
}