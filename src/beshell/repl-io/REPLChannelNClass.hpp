#pragma once

#include <EventEmitter.hpp>
#include "REPLChannel.hpp"

namespace be {
    class REPLChannelNClass: public be::EventEmitter, public REPLChannel {
        DECLARE_NCLASS_META
    private:
        static std::vector<JSCFunctionListEntry> methods ;
        // static std::vector<JSCFunctionListEntry> staticMethods ;

        bool enabledOutputEvent = false ;

        Parser parser ;
    public:
        REPLChannelNClass(JSContext * ctx, JSValue _jsobj=JS_NULL) ;
        ~REPLChannelNClass() ;
        
        static JSValue constructor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;

        static JSValue process(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue enableEventOutput(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
    
        virtual void sendData (const char * data, size_t datalen) ;
        virtual void send (Package & pkg) ;
    } ;
}