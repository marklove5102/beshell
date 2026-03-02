#pragma once

#include "../../NativeClass.hpp"
#include "./soc_serial.h"

namespace be {

    class SPI: public NativeClass {
        DECLARE_NCLASS_META
        static std::vector<JSCFunctionListEntry> methods ;
        // static std::vector<JSCFunctionListEntry> staticMethods ;

    private:
        int busnum ;

        static SPI * spi0 ;
        #if SOC_SPI_PERIPH_NUM > 1
        static SPI * spi1 ;
        #endif
        #if SOC_SPI_PERIPH_NUM > 2
        static SPI * spi2 ;
        #endif
        #if SOC_SPI_PERIPH_NUM > 3
        static SPI * spi3 ;
        #endif

        // std::map<int, spi_device_handle_t> devices ;

    public:
        SPI(JSContext * ctx, int busnum) ;

        int spiNum() const ;

        static SPI * flyweight(JSContext *, int) ;

        static JSValue setup(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue spiNum(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue sendU8(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue sendU16(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue sendU32(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue recvU8(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue recvU16(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue recvU32(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue transU8(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue transU16(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue transU32(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;

    } ;

}