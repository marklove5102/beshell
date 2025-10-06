#pragma once

// #include "BeShell.class.hpp"
#include "../../NativeModule.hpp"

namespace be{
    
// 前向声明
    class BeShell ;
    class TelnetChannelLogCapture;

    class Logger: public be::NativeModule {
    public:
        // Logger错误代码常量
        enum Error {
            SUCCESS = 0,
            ERROR_PARTITION_NOT_FOUND = -1,
            ERROR_PARTITION_READ_FAILED = -2,
            ERROR_PARTITION_WRITE_FAILED = -3,
            ERROR_INVALID_PARTITION_NAME = -4,
            ERROR_MEMORY_ALLOCATION_FAILED = -5
        };

        
        static char const * const name;

        Logger(JSContext * ctx, const char * name=nullptr);

        static int setup(BeShell & beshell, const char * partitionName, size_t bufferSize = 1024, size_t start=0, size_t end = 0, int core_id = 0);
        static int write(const void *buf, size_t count, bool timestampPrefix = true);
        static int write(const void *format, ...);
        static void pause();
        static void resume();
        static bool isCapturing();
        static int flush();
        static int clear();
        static void erase();
        static size_t length();
        static size_t read(size_t start_pos, size_t read_length, uint8_t * buffer);

        static JSValue setup(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue pause(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue resume(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue clear(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
        static JSValue erase(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
        static JSValue length(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
        static JSValue read(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
        static JSValue write(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
        static JSValue flush(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
        static JSValue top(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
        static JSValue tail(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

        static JSValue setWriteOffset(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
        static JSValue getWriteOffset(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
        static JSValue fill(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

    
    };
}
