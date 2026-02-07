#include "GPIO.hpp"
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <soc/gpio_num.h>
#include <cstddef>
#include <cstdint>
#include <vector>
#include "quickjs.h"

#include "../../js/gpio.c"

using namespace std ;

namespace be {

    static JSValue jsHandler = JS_NULL ;
    static vector<uint8_t> pending_level_changes ;

    bool GPIO::isr_installed = false ;

    GPIO::GPIO(JSContext * ctx, const char * name)
        : NativeModule(ctx, name, 0)
    {
        exportFunction("setMode", setMode, 2);
        exportFunction("pull", pull, 2);
        exportFunction("write", write, 2);
        exportFunction("read", read, 1);
        exportFunction("resetPin", resetPin, 0);
        exportName("blink");

        // for watch
        exportFunction("apiSetHandler", apiSetHandler, 0);
        exportFunction("apiAddISR", apiAddISR, 0);
        exportFunction("apiRemoveISR", apiRemoveISR, 0);
        exportName("watch");
        exportName("unwatch");

        // for adc
        exportFunction("adcUnitInit", adcUnitInit, 1);
        exportFunction("adcChannelInit", adcChannelInit, 1);
        exportFunction("adcPinInit", adcPinInit, 1);
        exportFunction("adcRead", adcRead, 1);
        exportFunction("readAnalog", adcRead, 1);
        exportFunction("adcChannelRead", adcChannelRead, 1);
        exportFunction("adcInfo", adcInfo, 0);
        exportFunction("adcContinuousStart", adcContinuousStart, 1);
        exportFunction("adcContinuousRead", adcContinuousRead, 2);
        exportFunction("adcContinuousStop", adcContinuousStop, 1);
        exportFunction("adcContinuousSetCallback", adcContinuousSetCallback, 2);

        // for PWM
        exportFunction("apiConfigPWM", apiConfigPWM, 0);
        exportFunction("apiWritePWM", apiWritePWM, 0);
        exportFunction("apiUpdatePWM", apiUpdatePWM, 0);
        exportFunction("apiStopPWM", apiStopPWM, 0);
        exportFunction("pwmMaxSpeedMode", pwmMaxSpeedMode, 0);
        exportName("configPWM");
        exportName("writePWM");
        exportName("updatePWM");
        exportName("stopPWM");

        // EXPORT_FUNCTION(test);

        // initialize ADC pin reflection (implementation in GPIO.adc.cpp)
        GPIO::adcReflectPins();

        JSEngine::fromJSContext(ctx)->addLoopFunction(GPIO::loop, nullptr, true, 0);
        JSEngine::fromJSContext(ctx)->addLoopFunction(GPIO::adcContinuousLoop, nullptr, true, 0);
    }

    void GPIO::exports(JSContext *ctx) {
        JSEngineEvalEmbeded(ctx, gpio)
    }

    /**
     * 设置 GPIO 引脚的模式。
     *
     * @module gpio
     * @function setMode
     * @param pin:number 引脚号
     * @param mode:string 引脚模式，支持 input|output|output-od|input-output|input-output-od
     *
     * @return undefined
     */
    JSValue GPIO::setMode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        ASSERT_ARGC(2)
        ARGV_TO_UINT8(0, pin)
        const char * mode = JS_ToCString(ctx, argv[1]) ;

        gpio_config_t conf = {
            .pin_bit_mask = (1ULL<<pin),
            .mode = GPIO_MODE_DISABLE,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };

        if(strcmp(mode,"input")==0) {
            conf.mode = GPIO_MODE_INPUT ;
        }
        else if(strcmp(mode,"output")==0) {
            conf.mode = GPIO_MODE_OUTPUT ;
        }
        else if(strcmp(mode,"output-od")==0) {
            conf.mode = GPIO_MODE_OUTPUT_OD ;
        }
        else if(strcmp(mode,"input-output")==0) {
            conf.mode = GPIO_MODE_INPUT_OUTPUT ;
        }
        else if(strcmp(mode,"input-output-od")==0) {
            conf.mode = GPIO_MODE_INPUT_OUTPUT_OD ;
        }
        else {
            JSTHROW("unknow pin mode(input, output, output-od, input-output, input-output-od)")
        }

        if(gpio_config(&conf) != ESP_OK) {
            JSTHROW("set pin mode failed, arg invalid?")
        }

        JS_FreeCString(ctx, mode) ;
        return JS_UNDEFINED ;
    }

    /**
     * 配置 GPIO 引脚的上拉或下拉模式。
     *
     * @module gpio
     * @function pull
     * @param pin:number 引脚号
     * @param mode:string 拉力模式，支持 up|down|updown|floating
     *
     * @return undefined
     */
    JSValue GPIO::pull(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        ASSERT_ARGC(2)
        ARGV_TO_UINT8(0, pin)
        ARGV_TO_CSTRING(1, mode)

        if(strcmp(mode,"up")==0) {
            gpio_set_pull_mode((gpio_num_t)pin, GPIO_PULLUP_ONLY) ;
            gpio_pullup_en((gpio_num_t)pin) ;
        }
        else if(strcmp(mode,"down")==0) {
            gpio_set_pull_mode((gpio_num_t)pin, GPIO_PULLDOWN_ONLY) ;
            gpio_pulldown_en((gpio_num_t)pin) ;
        }
        else if(strcmp(mode,"updown")==0) {
            gpio_set_pull_mode((gpio_num_t)pin, GPIO_PULLUP_PULLDOWN) ;
            gpio_pulldown_en((gpio_num_t)pin) ;
            gpio_pullup_en((gpio_num_t)pin) ;
        }
        else if(strcmp(mode,"floating")==0) {
            gpio_set_pull_mode((gpio_num_t)pin, GPIO_FLOATING) ;
            gpio_pullup_dis((gpio_num_t)pin) ;
            gpio_pulldown_dis((gpio_num_t)pin) ;
        }
        else {
            JSTHROW("unknow pin pull mode(up|down|updown|floating)")
        }
        JS_FreeCString(ctx, mode) ;
        return JS_UNDEFINED ;
    }

    /**
     * 写入 GPIO 引脚电平。
     *
     * @module gpio
     * @function write
     * @param pin:number 引脚号
     * @param value:number 输出电平，0 或 1
     *
     * @return boolean 设置成功返回 true，否则返回 false
     */
    JSValue GPIO::write(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        ASSERT_ARGC(2)
        ARGV_TO_UINT8(0, pin)
        ARGV_TO_UINT8(1, value)
        return gpio_set_level((gpio_num_t)pin, value)==ESP_OK? JS_TRUE: JS_FALSE ;
    }

    /**
     * 读取 GPIO 引脚当前电平。
     *
     * @module gpio
     * @function read
     * @param pin:number 引脚号
     *
     * @return number 当前电平值，0 或 1
     */
    JSValue GPIO::read(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        ASSERT_ARGC(1)
        ARGV_TO_UINT8(0, pin)
        return JS_NewUint32(ctx, gpio_get_level((gpio_num_t)pin)) ;
    }

    static void /*IRAM_ATTR*/ gpio_isr_handler(void* arg) {
        gpio_num_t pin = (gpio_num_t) (uint32_t) arg ;
        uint8_t val = gpio_get_level( pin ) ;
        uint8_t event = pin | (val<<7) ;
        pending_level_changes.push_back(event) ;
    }
    
    bool GPIO::installISR(int flag) {
        if(isr_installed) {
            return true ;
        }
        esp_err_t res = gpio_install_isr_service(flag) ;
        if(res!=ESP_OK) {
            printf("gpio_install_isr_service() failed:%d\n", res) ;
            return false ;
        }
        isr_installed = true ;
        return true ;
    }

    void GPIO::uninstallISR() {
        if(!isr_installed) {
            return ;
        }
        gpio_uninstall_isr_service() ;
        isr_installed = false ;
    }
    
    JSValue GPIO::apiSetHandler(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        CHECK_ARGC(1)
        if(!JS_IsFunction(ctx, argv[0])) {
            JSTHROW("apiSetHandler() arg callback must be a function")
        }
        JS_FreeValue(ctx, jsHandler) ;
        jsHandler = JS_DupValue(ctx, argv[0]) ;
        return JS_UNDEFINED ;
    }
    
    JSValue GPIO::apiAddISR(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        CHECK_ARGC(1)
        ARGV_TO_GPIO(0, pin)

        installISR(0) ;
        
        esp_err_t err = gpio_isr_handler_add((gpio_num_t)pin, gpio_isr_handler, (void *)pin) ;
        if(err!=ESP_OK) {
            JSTHROW("gpio_isr_handler_add() failed, err:%d", err)
        }

        err = gpio_set_intr_type((gpio_num_t)pin, GPIO_INTR_ANYEDGE);
        if(err!=ESP_OK) {
            JSTHROW("gpio_set_intr_type() failed, err:%d", err)
        }
        err = gpio_intr_enable((gpio_num_t)pin) ;
        if(err!=ESP_OK) {
            JSTHROW("gpio_intr_enable() failed, err:%d", err)
        }
        return JS_UNDEFINED ;
    }
    
    JSValue GPIO::apiRemoveISR(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        CHECK_ARGC(1)
        ARGV_TO_GPIO(0, pin)
        esp_err_t err = gpio_isr_handler_remove((gpio_num_t)pin);
        if(err!=ESP_OK) {
            JSTHROW("gpio_isr_handler_remove() failed, err:%d", err)
        }
        return JS_UNDEFINED ;
    }

    void GPIO::loop(JSContext * ctx, void * arg) {

        if( !pending_level_changes.size() ){
            return ;
        }
        if( !JS_IsFunction(ctx, jsHandler) ) {
            return ;
        }

        JSValueConst argv[2] ;

        for(auto event: pending_level_changes) {
            argv[0] = JS_NewInt32(ctx, event & 0x7F) ;      // pin number
            argv[1] = JS_NewInt32(ctx, (event>>7) & 0x01) ; // level
            JSValue ret = JS_Call(ctx, jsHandler, JS_UNDEFINED, 2, argv) ;
            if(JS_IsException(ret)) {
                JSEngine::fromJSContext(ctx)->dumpError() ;
            }
            JS_FreeValue(ctx, ret) ;
        }
        
        pending_level_changes.clear() ;
    }
    
    
    JSValue GPIO::test(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        return JS_UNDEFINED ;
    }

    /**
     * 重置 GPIO 引脚到默认状态。
     *
     * @module gpio
     * @function resetPin
     * @param pin:number GPIO 引脚编号
     *
     * @return undefined
     */
    JSValue GPIO::resetPin(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        CHECK_ARGC(1)
        ARGV_TO_UINT8(0, pin)
        
        // Validate pin number
        if (pin >= GPIO_NUM_MAX) {
            JSTHROW("Invalid GPIO pin number")
        }
        
        // Reset the pin to default state
        esp_err_t err = gpio_reset_pin((gpio_num_t)pin);
        if (err != ESP_OK) {
            JSTHROW("Reset GPIO pin failed, err: %d", err)
        }
        
        return JS_UNDEFINED;
    }
}


/**
 * 监听引脚电平变化并注册回调。
 *
 * @module gpio
 * @function watch
 * @param pin:number GPIO 引脚号
 * @param edge:"rising"|"falling"|"both" 触发的边沿类型
 * @param callback:Function 变化时调用的函数
 */


/**
 * 取消引脚电平变化的监听。
 *
 * @module gpio
 * @function unwatch
 * @param gpio:number GPIO 引脚号
 * @param edge:"rising"|"falling"|"both" 取消的边沿类型
 * @param callback:Function 要移除的回调函数
 */


/**
 * GPIO 闪烁，执行该函数后，指定的引脚会持续高低电平切换。
 * 
 * @module gpio
 * @function blink
 * @param pin:number 引脚序号
 * @param time:number 间隔时间，单位毫秒，闪烁的半周期
 * 
 * @return number 定时器id，可使用 `clearTimeout()` 停止闪烁。
 */
