/**
 * ADC-related implementation split out from GPIO.cpp
 */

#include "GPIO.hpp"
#include <esp_adc/adc_oneshot.h>
/**
 * ADC-related implementation split out from GPIO.cpp
 */

#include "GPIO.hpp"
#include <esp_adc/adc_oneshot.h>
#include <hal/adc_types.h>
#include <soc/gpio_num.h>
#include <map>
#include <cstddef>
#include "quickjs.h"

#define JSTHROW(...)                                \
    JS_ThrowReferenceError(ctx, __VA_ARGS__);       \
    return JS_EXCEPTION ;
    
typedef struct  {
    adc_unit_t unit ;
    adc_channel_t channel ;
    bool configured ;
} adc_channel_info_t ;


namespace be {

    static adc_oneshot_unit_handle_t adc_handles[SOC_ADC_PERIPH_NUM] = {nullptr} ;
    static std::map<gpio_num_t, adc_channel_info_t> map_gpio_adc_info ;

    static adc_channel_info_t* _getChannelInfo(adc_unit_t unit, adc_channel_t channel) {
        for(auto &entry : map_gpio_adc_info) {
            if(entry.second.channel == channel && entry.second.unit == unit) {
                return &entry.second ;
            }
        }
        return nullptr ;
    }

    static void _setChannelConfigured(adc_channel_t channel, bool configured) {
        for(auto &entry : map_gpio_adc_info) {
            if(entry.second.channel == channel) {
                entry.second.configured = configured ;
            }
        }
    }

    static size_t _adcUnitIndex(adc_unit_t unit_id) {
        switch(unit_id) {
            case ADC_UNIT_1:
                return 0 ;
        #if (SOC_ADC_PERIPH_NUM >= 2)
            case ADC_UNIT_2:
                return 1 ;
        #endif
            default:
                return SIZE_MAX ;
        }
    }

    static size_t _gpioToIndex(gpio_num_t pin) {
        int pin_int = static_cast<int>(pin) ;
        if(pin_int < 0 || pin_int >= GPIO_NUM_MAX) {
            return SIZE_MAX ;
        }
        return static_cast<size_t>(pin_int) ;
    }

    /**
     * Initialize pin -> adc reflection. This was previously inside the GPIO
     * constructor; moved here so ADC data lives with ADC implementation.
     */
    void GPIO::adcReflectPins() {
        map_gpio_adc_info.clear() ;
        for(int pin = 0; pin < GPIO_NUM_MAX; ++pin) {
            gpio_num_t gpio = static_cast<gpio_num_t>(pin) ;
            adc_unit_t unit ;
            adc_channel_t channel ;
            if(adc_oneshot_io_to_channel(gpio, &unit, &channel) == ESP_OK) {
                adc_channel_info_t info ;
                info.unit = unit ;
                info.channel = channel ;
                info.configured = false ;

                map_gpio_adc_info[gpio] = info ;
            }
        }
    }

    static esp_err_t _initADCUnit(adc_unit_t unit_id) {
        size_t idx = _adcUnitIndex(unit_id) ;
        if(idx == SIZE_MAX) {
            return ESP_ERR_INVALID_ARG ;
        }

        if(adc_handles[idx] != NULL) {
            return ESP_ERR_INVALID_STATE ;
        }

        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = unit_id,
            .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        } ;

        return adc_oneshot_new_unit(&init_config, &adc_handles[idx]) ;
    }

    static int _initADCChannel(adc_channel_info_t &channel_info, adc_atten_t atten, adc_bitwidth_t bitwidth) {

        esp_err_t err ;
        adc_unit_t uint_num = channel_info.unit ;
        adc_channel_t channel = channel_info.channel ;
        size_t idx = _adcUnitIndex(uint_num) ;
        if(idx == SIZE_MAX) {
            return ESP_ERR_INVALID_ARG ;
        }

        if(adc_handles[idx] == NULL) {
            err = _initADCUnit(uint_num) ;
            if(err!=ESP_OK && err!=ESP_ERR_INVALID_STATE) {
                return err ;
            }
        }

        adc_oneshot_chan_cfg_t config = {
            .atten = atten,
            .bitwidth = bitwidth,
        } ;
        
        err = adc_oneshot_config_channel(adc_handles[idx], channel, &config) ;
        if(err!=ESP_OK && err!=ESP_ERR_INVALID_STATE) {
            return err ;
        }

        _setChannelConfigured(channel, true) ;

        return ESP_OK ;
    }

    static bool _adcChannelFromPin(JSContext *ctx, gpio_num_t pin, adc_channel_t &channel_out, adc_unit_t &unit_out) {
        size_t pin_index = _gpioToIndex(pin) ;
        if(pin_index == SIZE_MAX) {
            JS_ThrowReferenceError(ctx, "pin is not a valid adc pin.") ;
            return false ;
        }

        if(map_gpio_adc_info.count(pin) == 0) {
            JS_ThrowReferenceError(ctx, "pin is not a valid adc pin.") ;
            return false ;
        }

        auto &info = map_gpio_adc_info.at(pin) ;
        channel_out = info.channel ;
        unit_out = info.unit ;
        return true ;
    }

    /**
     * 初始化指定的 ADC 单元。
     *
     * @module gpio
     * @function adcUnitInit
     * 
     * @param unit:number ADC 单元号，支持 1 或 2（取决于芯片能力）
     *
     * @return undefined
     */
    JSValue GPIO::adcUnitInit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        CHECK_ARGC(1)
        uint32_t unit_num = 1 ;
        if( JS_ToUint32(ctx, &unit_num, argv[0])!=0 ) {
            JSTHROW("Invalid param type")
        }
        if(unit_num<1 || unit_num>2) {
            JSTHROW("Invalid param value")
        }
        adc_unit_t unit = unit_num == 1 ? ADC_UNIT_1 : ADC_UNIT_2 ;
        esp_err_t err = _initADCUnit(unit) ;
        if(err==ESP_ERR_INVALID_STATE) {
            JSTHROW("unit already inited")
        }
        if(err!=ESP_OK) {
            JSTHROW("init adc unit failed, err:%d", err)
        }
        return JS_UNDEFINED ;
    }

    static JSValue adc_channel_init_helper(JSContext *ctx, adc_unit_t unit, adc_channel_t channel, int argc, JSValueConst *argv) {
        adc_channel_info_t *info = _getChannelInfo(unit, channel) ;
        if(info == nullptr) {
            JSTHROW("invalid adc channel configuration")
        }

        size_t idx = _adcUnitIndex(unit) ;
        if(idx == SIZE_MAX) {
            JSTHROW("invalid adc unit")
        }

        if(adc_handles[idx] == NULL) {
            esp_err_t err = _initADCUnit(unit) ;
            if(err!=ESP_OK && err!=ESP_ERR_INVALID_STATE) {
                JSTHROW("init adc unit failed, err:%d", err)
            }
        }

        adc_atten_t atten = ADC_ATTEN_DB_12 ;
        adc_bitwidth_t bitwidth = ADC_BITWIDTH_DEFAULT ;

        if(argc >= 2) {
            uint32_t atten_val = 0 ;
            if(JS_ToUint32(ctx, &atten_val, argv[1])!=0) {
                JSTHROW("Invalid atten value")
            }
            atten = static_cast<adc_atten_t>(atten_val) ;
        }

        if(argc >= 3) {
            uint32_t bitwidth_val = 0 ;
            if(JS_ToUint32(ctx, &bitwidth_val, argv[2])!=0) {
                JSTHROW("Invalid bitwidth value")
            }
            bitwidth = static_cast<adc_bitwidth_t>(bitwidth_val) ;
        }
        
        esp_err_t err = _initADCChannel(*info, atten, bitwidth) ;
        if(err!=ESP_OK) {
            JSTHROW("init adc channel failed, err:%d", err)
        }

        return JS_UNDEFINED ;
    }


    /**
     * 通过MCU引脚号初始化 ADC 通道。
     * 
     * @module gpio
     * @function adcPinInit
     * 
     * @param pin:number 引脚序号
     * @param atten:number=3 ADC 衰减值, 默认值 ADC_ATTEN_DB_12
     * @param bitwidth:number=0 ADC 位宽, 默认值 ADC_BITWIDTH_DEFAULT
     * 
     * @return undefined
     */
    JSValue GPIO::adcPinInit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        CHECK_ARGC(1)
        ARGV_TO_GPIO(0, pin)

        adc_channel_t channel ;
        adc_unit_t unit ;
        if(!_adcChannelFromPin(ctx, pin, channel, unit)) {
            return JS_EXCEPTION ;
        }
        
        return adc_channel_init_helper(ctx, unit, channel, argc, argv) ;
    }

    /**
     * 初始化指定 ADC 通道。
     * 
     * @module gpio
     * @function adcChannelInit
     * 
     * @param channel:number adc 通道号
     * @param atten:number=3 ADC 衰减值, 默认值 ADC_ATTEN_DB_12
     * @param bitwidth:number=0 ADC 位宽, 默认值 ADC_BITWIDTH_DEFAULT
     * @param unit:number=1 ADC 单元号, 默认单元1 ，部分 esp32 型号支持单元2
     * 
     * @return undefined
     */
    JSValue GPIO::adcChannelInit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        CHECK_ARGC(1)
        ARGV_TO_INT(0, channel, adc_channel_t, uint32_t, JS_ToUint32)
        uint32_t unit_num = 1 ;
        if(argc >= 4 && !JS_IsUndefined(argv[3])) {
            if(JS_ToUint32(ctx, &unit_num, argv[3])!=0) {
                JSTHROW("Invalid unit value")
            }
        }
        if(unit_num < 1
#if (SOC_ADC_PERIPH_NUM >= 2)
            || unit_num > SOC_ADC_PERIPH_NUM
#endif
        ) {
            JSTHROW("Invalid unit value")
        }
        adc_unit_t unit = ADC_UNIT_1 ;
#if (SOC_ADC_PERIPH_NUM >= 2)
        if(unit_num == 2) {
            unit = ADC_UNIT_2 ;
        }
#endif
        return adc_channel_init_helper(ctx, unit, channel, argc, argv) ;
    }

    static JSValue adc_channel_read_helper(JSContext *ctx, adc_channel_t channel, adc_unit_t uint_num) {
        adc_channel_info_t *info = _getChannelInfo(uint_num, channel) ;
        if(info == nullptr) {
            JSTHROW("invalid adc channel configuration")
        }

        if(!info->configured) {
            esp_err_t err = _initADCChannel(*info, ADC_ATTEN_DB_11, ADC_BITWIDTH_DEFAULT) ;
            if(err!=ESP_OK) {
                JSTHROW("init adc channel failed, err:%d", err)
            }
        }

        int value = 0 ;

        size_t idx = _adcUnitIndex(uint_num) ;
        if(idx == SIZE_MAX) {
            JSTHROW("invalid adc unit")
        }

        esp_err_t err = adc_oneshot_read(adc_handles[idx], (adc_channel_t)channel, &value) ;
        if(err!=ESP_OK) {
            JSTHROW("read adc failed, err:%d", err)
        }

        return JS_NewInt32(ctx, value) ;
    }
    
    /**
     * 读取指定 ADC 通道的值。
     * 
     * @module gpio
     * @function adcChannelRead
     * @param channel:number 通道号
     * @param unit:number=1 ADC 单元号, 默认单元1 ，部分 esp32 型号支持单元2
     * 
     * @return number ADC 通道的读取值
     */
    JSValue GPIO::adcChannelRead(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        CHECK_ARGC(1)
        ARGV_TO_INT(0, channel, adc_channel_t, uint32_t, JS_ToUint32)
        uint32_t unit_num = 1 ;
        if(argc >= 2 && !JS_IsUndefined(argv[1])) {
            if(JS_ToUint32(ctx, &unit_num, argv[1])!=0) {
                JSTHROW("Invalid unit value")
            }
        }
        if(unit_num < 1
#if (SOC_ADC_PERIPH_NUM >= 2)
            || unit_num > SOC_ADC_PERIPH_NUM
#endif
        ) {
            JSTHROW("Invalid unit value")
        }
        adc_unit_t unit = ADC_UNIT_1 ;
#if (SOC_ADC_PERIPH_NUM >= 2)
        if(unit_num == 2) {
            unit = ADC_UNIT_2 ;
        }
#endif
        return adc_channel_read_helper(ctx, channel, unit) ;
    }

    /**
     * 通过MCU引脚号读取 ADC 通道的值。
     * 
     * @module gpio
     * @function adcRead
     * @param pin:number 引脚号
     * 
     * @return number ADC 通道的读取值
     */
    JSValue GPIO::adcRead(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        CHECK_ARGC(1)
        ARGV_TO_GPIO(0, pin)

        adc_channel_t channel ;
        adc_unit_t unit ;
        if(!_adcChannelFromPin(ctx, pin, channel, unit)) {
            return JS_EXCEPTION ;
        }
        return adc_channel_read_helper(ctx, channel, unit) ;
    }

    /**
     * 获取当前 MCU 引脚与 ADC 通道的映射信息。
     *
     * @module gpio
     * @function adcInfo
     *
     * @return object 以引脚号为键的对象，每项包含 channel 与 unit 字段
     */
    JSValue GPIO::adcInfo(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        JSValue obj = JS_NewObject(ctx) ;
        for(const auto &entry : map_gpio_adc_info) {
            gpio_num_t gpio = entry.first ;
            const adc_channel_info_t &info = entry.second ;
            adc_channel_t channel = info.channel ;
            JSValue item = JS_NewObject(ctx) ;
            JS_SetPropertyStr(ctx, item, "channel", JS_NewInt32(ctx, static_cast<int>(channel))) ;
            int unit_display = 0 ;
            switch(info.unit) {
            case ADC_UNIT_1:
                unit_display = 1 ;
                break ;
#if (SOC_ADC_PERIPH_NUM >= 2)
            case ADC_UNIT_2:
                unit_display = 2 ;
                break ;
#endif
            default:
                unit_display = -1 ;
                break ;
            }
            JS_SetPropertyStr(ctx, item, "unit", JS_NewInt32(ctx, unit_display)) ;
            JS_SetPropertyUint32(ctx, obj, static_cast<uint32_t>(gpio), item) ;
        }
        return obj ;
    }

}
