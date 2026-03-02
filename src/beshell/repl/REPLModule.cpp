#include "REPLModule.hpp"
#include "REPLChannelNClass.hpp"
#include "../js/repl.c"
#include "qjs_utils.h"
#include "../BeShell.hpp"

namespace be{

    char const * const REPLModule::name = "repl" ;

    std::vector<NativeModuleExportorFunc> REPLModule::exportors ;

    REPLModule::REPLModule(JSContext * ctx, const char * name)
        : EventModule(ctx, name, 0)
    {
        exportClass<REPLChannelNClass>() ;
        exportName("ws") ;
        exportName("cdc") ;
        exportName("mqtt") ;
        exportName("startLog") ;
        exportName("stopLog") ;
        
        EXPORT_FUNCTION(enableCrypto) ;
        EXPORT_FUNCTION(disableCrypto) ;
        EXPORT_FUNCTION(setCryptoKey) ;
        EXPORT_FUNCTION(setPassword) ;
    }

    void REPLModule::registerExportor(NativeModuleExportorFunc func) {
        exportors.push_back(func) ;
    }

    void REPLModule::exports(JSContext *ctx) {
        for(auto func : exportors) {
            func(ctx, this) ;
        }

        JSEngineEvalEmbeded(ctx, repl)
    }

    JSValue REPLModule::enableCrypto(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        JSEngine::fromJSContext(ctx)->beshell->repl->enableCrypto = true ;
        return JS_UNDEFINED ;
    }
    JSValue REPLModule::disableCrypto(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        JSEngine::fromJSContext(ctx)->beshell->repl->enableCrypto = false ;
        return JS_UNDEFINED ;
    }
    JSValue REPLModule::setCryptoKey(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        CHECK_ARGC(2)
        if(!JS_IsArrayBuffer(argv[0])){
            JSTHROW("crypto key must be array buffer") ;
        }
        if(!JS_IsArrayBuffer(argv[1])){
            JSTHROW("crypto vi must be array buffer") ;
        }

        size_t key_len, vi_len ;
        uint8_t * key = JS_GetArrayBuffer(ctx,&key_len, argv[0]) ;
        uint8_t * vi = JS_GetArrayBuffer(ctx,&vi_len, argv[1]) ;

        if(key_len!=16) {
            JSTHROW("crypto key length must be 16") ;
        }
        if(vi_len!=16) {
            JSTHROW("crypto vi length must be 16") ;
        }

        auto repl = JSEngine::fromJSContext(ctx)->beshell->repl ;
        memcpy( repl->cryptoKey, key, 16) ;
        memcpy( repl->cryptoVI, vi, 16) ;

        return JS_UNDEFINED ;
    }

    JSValue REPLModule::setPassword(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        CHECK_ARGC(1)
        const char * pwd = JS_ToCString(ctx, argv[0]) ;
        JSEngine::fromJSContext(ctx)->beshell->cammonds->setPassword(pwd) ;
        JS_FreeCString(ctx, pwd) ;
        return JS_UNDEFINED ;
    }
}
