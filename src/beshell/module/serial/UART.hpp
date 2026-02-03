#pragma once

#include "../../NativeClass.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "./soc_serial.h"



namespace be{
    class UART: public be::NativeClass {
        DECLARE_NCLASS_META
    private:
        static std::vector<JSCFunctionListEntry> methods ;

        int m_uartNum ;
        
        static UART * uart0 ;
        #if SOC_UART_HP_NUM>1
        static UART * uart1 ;
        #endif
        #if SOC_UART_HP_NUM>2
        static UART * uart2 ;
        #endif
        #if SOC_UART_HP_NUM>3
        static UART * uart3 ;
        #endif
        #if SOC_UART_HP_NUM>4
        static UART * uart4 ;
        #endif
        #if SOC_UART_LP_NUM>0
        static UART * uartlp0 ;
        #endif

        TaskHandle_t taskListenerHandle = nullptr ;
        JSValue listener = JS_NULL ;
        QueueHandle_t data_queue = nullptr ;
        static void task_listen(void * arg) ;
        static void loop(JSContext * ctx, void * opaque) ;

    public:
        UART(JSContext * ctx, int port) ;

        static UART * flyweight(JSContext *, int) ;

        int uartNum() const ;

        static JSValue setup(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue unsetup(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue isInstalled(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue read(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue write(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
        static JSValue listen(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) ;
    } ;
}