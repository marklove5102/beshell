#pragma once

#include "../NativeClass.hpp"
#include <cstddef>


#undef stringify   // defined in quickjs/cutils.h

namespace be {

    typedef void (* ConsoleWriteHandler)(const char * buff, size_t len) ;

    class Console: public NativeClass {
        DECLARE_NCLASS_META
        static std::vector<JSCFunctionListEntry> methods ;

    public:
        Console(JSContext * ctx, JSValue jsobj=JS_NULL) ;
        static JSValue constructor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;

        std::string stringify(JSContext * ctx, JSValue val) ;

        static JSValue jsWrite(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue jsLog(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;

        static void setChannel(const char * channelName) ;

        static ConsoleWriteHandler writeHandler ;

    private:
        JSValue jsStringify = JS_UNDEFINED ;
        static char const * channelName ;
    } ;

}