/**
 * PWM-related implementation split out from GPIO.cpp
 */

#include "GPIO.hpp"
#include <driver/ledc.h>
#include "quickjs.h"

namespace be {

    JSValue GPIO::apiConfigPWM(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        CHECK_ARGC(1)
        ARGV_TO_UINT8(0, pin)
        
        // Default values
        uint8_t speed_mode = LEDC_LOW_SPEED_MODE;
        uint32_t duty = 0;
        uint32_t freq = 1000;
        uint8_t channel = 0;
        ledc_timer_bit_t duty_resolution = LEDC_TIMER_10_BIT;
        ledc_timer_t timer_num = LEDC_TIMER_0;
        ledc_clk_cfg_t clk_cfg = LEDC_AUTO_CLK;
        ledc_intr_type_t intr_type = LEDC_INTR_DISABLE;
        
        // Process options object if provided
        if (argc > 1 && !JS_IsUndefined(argv[1])) {
            if (!JS_IsObject(argv[1])) {
                JSTHROW("Second parameter must be an options object")
            }
            
            // Extract speed_mode
            JSValue js_speed_mode = JS_GetPropertyStr(ctx, argv[1], "mode");
            if (!JS_IsUndefined(js_speed_mode)) {
                uint32_t sm;
                if (JS_ToUint32(ctx, &sm, js_speed_mode) != 0) {
                    JS_FreeValue(ctx, js_speed_mode);
                    JSTHROW("Invalid mode value")
                }
                if (sm > LEDC_SPEED_MODE_MAX) {
                    JS_FreeValue(ctx, js_speed_mode);
                    JSTHROW("Speed mode must be 0 - %d", LEDC_SPEED_MODE_MAX)
                }
                speed_mode = sm;
            }
            JS_FreeValue(ctx, js_speed_mode);
            
            // Extract duty
            JSValue js_duty = JS_GetPropertyStr(ctx, argv[1], "duty");
            if (!JS_IsUndefined(js_duty)) {
                if (JS_ToUint32(ctx, &duty, js_duty) != 0) {
                    JS_FreeValue(ctx, js_duty);
                    JSTHROW("Invalid duty value")
                }
            }
            JS_FreeValue(ctx, js_duty);
            
            // Extract freq
            JSValue js_freq = JS_GetPropertyStr(ctx, argv[1], "freq");
            if (!JS_IsUndefined(js_freq)) {
                if (JS_ToUint32(ctx, &freq, js_freq) != 0) {
                    JS_FreeValue(ctx, js_freq);
                    JSTHROW("Invalid frequency value")
                }
                if (freq < 1) freq = 1;
                if (freq > 40000) freq = 40000; // Limit frequency to reasonable range
            }
            JS_FreeValue(ctx, js_freq);
            
            // Extract channel
            JSValue js_channel = JS_GetPropertyStr(ctx, argv[1], "channel");
            if (!JS_IsUndefined(js_channel)) {
                uint32_t ch;
                if (JS_ToUint32(ctx, &ch, js_channel) != 0) {
                    JS_FreeValue(ctx, js_channel);
                    JSTHROW("Invalid channel value")
                }
                if (ch > 7) {
                    JS_FreeValue(ctx, js_channel);
                    JSTHROW("Channel must be between 0-7")
                }
                channel = ch;
            }
            JS_FreeValue(ctx, js_channel);
            
            // Extract duty_resolution
            JSValue js_duty_resolution = JS_GetPropertyStr(ctx, argv[1], "resolution");
            if (!JS_IsUndefined(js_duty_resolution)) {
                uint32_t res;
                if (JS_ToUint32(ctx, &res, js_duty_resolution) != 0) {
                    JS_FreeValue(ctx, js_duty_resolution);
                    JSTHROW("Invalid resolution value")
                }
                if (res < 1 || res > 20) {
                    JS_FreeValue(ctx, js_duty_resolution);
                    JSTHROW("Duty resolution must be between 1-20")
                }
                duty_resolution = (ledc_timer_bit_t)res;
            }
            JS_FreeValue(ctx, js_duty_resolution);
            
            // Extract timer_num
            JSValue js_timer_num = JS_GetPropertyStr(ctx, argv[1], "timer");
            if (!JS_IsUndefined(js_timer_num)) {
                uint32_t tm;
                if (JS_ToUint32(ctx, &tm, js_timer_num) != 0) {
                    JS_FreeValue(ctx, js_timer_num);
                    JSTHROW("Invalid timer value")
                }
                if (tm > 3) {
                    JS_FreeValue(ctx, js_timer_num);
                    JSTHROW("Timer number must be between 0-3")
                }
                timer_num = (ledc_timer_t)tm;
            }
            JS_FreeValue(ctx, js_timer_num);
            
            // Extract clk_cfg
            JSValue js_clk_cfg = JS_GetPropertyStr(ctx, argv[1], "clk");
            if (!JS_IsUndefined(js_clk_cfg)) {
                uint32_t clk;
                if (JS_ToUint32(ctx, &clk, js_clk_cfg) != 0) {
                    JS_FreeValue(ctx, js_clk_cfg);
                    JSTHROW("Invalid clk value")
                }
                clk_cfg = (ledc_clk_cfg_t)clk;
            }
            JS_FreeValue(ctx, js_clk_cfg);
            
            // Extract intr_type
            JSValue js_intr_type = JS_GetPropertyStr(ctx, argv[1], "intr");
            if (!JS_IsUndefined(js_intr_type)) {
                uint32_t intr;
                if (JS_ToUint32(ctx, &intr, js_intr_type) != 0) {
                    JS_FreeValue(ctx, js_intr_type);
                    JSTHROW("Invalid intr value")
                }
                intr_type = (ledc_intr_type_t)intr;
            }
            JS_FreeValue(ctx, js_intr_type);
        }
        
        // Configure LEDC timer
        ledc_timer_config_t timer_conf = {
            .speed_mode = (ledc_mode_t)speed_mode,
            .duty_resolution = duty_resolution,
            .timer_num = timer_num,
            .freq_hz = freq,
            .clk_cfg = clk_cfg
        };
        
        esp_err_t err = ledc_timer_config(&timer_conf);
        if (err != ESP_OK) {
            JSTHROW("Config LEDC timer failed, err: %d", err)
        }
        
        // Configure LEDC channel
        ledc_channel_config_t channel_conf = {
            .gpio_num = pin,
            .speed_mode = (ledc_mode_t)speed_mode,
            .channel = (ledc_channel_t)channel,
            .intr_type = intr_type,
            .timer_sel = timer_num,
            .duty = duty,
            .hpoint = 0
        };
        
        err = ledc_channel_config(&channel_conf);
        if (err != ESP_OK) {
            JSTHROW("Config LEDC channel failed, err: %d", err)
        }
        
        return JS_UNDEFINED;
    }

    JSValue GPIO::apiWritePWM(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        CHECK_ARGC(3)
        ARGV_TO_UINT8(0, speed_mode)
        ARGV_TO_UINT8(1, channel)
        ARGV_TO_UINT32(2, duty)
        
        // Validate speed_mode
        if (speed_mode > LEDC_SPEED_MODE_MAX) {
            JSTHROW("Speed mode must be 0 - %d", LEDC_SPEED_MODE_MAX)
        }
        
        // Validate channel
        if (channel > 7) {
            JSTHROW("Channel must be between 0-7")
        }
        
        bool update = true;  // Default is true
        if (argc > 3 && !JS_IsUndefined(argv[3])) {
            update = JS_ToBool(ctx, argv[3]);
        }
        
        // Set the duty cycle
        esp_err_t err = ledc_set_duty((ledc_mode_t)speed_mode, (ledc_channel_t)channel, duty);
        if (err != ESP_OK) {
            JSTHROW("Set PWM duty failed, err: %d", err)
        }
        
        // Update the duty if requested
        if (update) {
            err = ledc_update_duty((ledc_mode_t)speed_mode, (ledc_channel_t)channel);
            if (err != ESP_OK) {
                JSTHROW("Update PWM duty failed, err: %d", err)
            }
        }
        
        return JS_UNDEFINED;
    }

    JSValue GPIO::apiUpdatePWM(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        CHECK_ARGC(2)
        ARGV_TO_UINT8(0, speed_mode)
        ARGV_TO_UINT8(1, channel)

        // Validate speed_mode
        if (speed_mode > LEDC_SPEED_MODE_MAX) {
            JSTHROW("Speed mode must be 0 - %d", LEDC_SPEED_MODE_MAX)
        }
        
        // Validate channel
        if (channel > 7) {
            JSTHROW("Channel must be between 0-7")
        }

        // Update the duty cycle
        esp_err_t err = ledc_update_duty((ledc_mode_t)speed_mode, (ledc_channel_t)channel);
        if (err != ESP_OK) {
            JSTHROW("Update PWM duty failed, err: %d", err)
        }
        
        return JS_UNDEFINED;
    }

    JSValue GPIO::apiStopPWM(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        CHECK_ARGC(2)
        ARGV_TO_UINT8(0, speed_mode)
        ARGV_TO_UINT8(1, channel)
        
        // Validate speed_mode
        if (speed_mode > LEDC_SPEED_MODE_MAX) {
            JSTHROW("Speed mode must be 0 - %d", LEDC_SPEED_MODE_MAX)
        }
        
        // Validate channel
        if (channel > 7) {
            JSTHROW("Channel must be between 0-7")
        }
        
        // Stop PWM output
        esp_err_t err = ledc_stop((ledc_mode_t)speed_mode, (ledc_channel_t)channel, 0);
        if (err != ESP_OK) {
            JSTHROW("Stop PWM failed, err: %d", err)
        }
        
        return JS_UNDEFINED;
    }

    JSValue GPIO::pwmMaxSpeedMode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        return JS_NewUint32(ctx, LEDC_SPEED_MODE_MAX) ;
    }

}



// ============================================================================
// pwm 对象在 js/gpio.js 中定义，通过 exportValue 导出到 GPIO 模块
// ============================================================================

/**
 * PWM 对象
 * 
 * 提供脉冲宽度调制（PWM）输出功能，用于控制 LED 亮度、电机速度、舵机角度等。
 * 
 * @module gpio
 * @object pwm
 */

/**
 * 配置指定引脚的 PWM 输出
 * 
 * 配置 GPIO 引脚为 PWM 输出模式，支持多种参数配置。
 * 
 * 配置选项：
 * ```typescript
 * {
 *     mode: number = 0,        // 速度模式：0=低速模式, 1=高速模式
 *     duty: number = 0,        // 初始占空比
 *     freq: number = 1000,     // 频率（Hz）
 *     channel: number = auto,  // PWM 通道（自动分配）
 *     resolution: number = 10, // 分辨率（位），决定占空比精度
 *     timer: number = 0,       // 定时器号
 * }
 * ```
 * 
 * 示例：
 * ```javascript
 * import * as gpio from "gpio"
 * 
 * // 基本配置 - LED 调光
 * gpio.pwm.config(2, { freq: 1000, duty: 512 })
 * 
 * // 配置舵机（50Hz，1-2ms 脉宽）
 * gpio.pwm.config(18, { 
 *     freq: 50,           // 50Hz = 20ms 周期
 *     resolution: 16,     // 16位分辨率
 *     duty: 4915          // 1.5ms 脉宽 (居中)
 * })
 * 
 * // 修改占空比
 * gpio.pwm.write(2, 256)   // 25% 亮度
 * gpio.pwm.write(2, 768)   // 75% 亮度
 * gpio.pwm.write(2, 1023)  // 100% 亮度
 * 
 * // 停止 PWM
 * gpio.pwm.stop(2)
 * ```
 *
 * @module gpio
 * @object pwm
 * @function config
 * @param pin:number GPIO 引脚号
 * @param options:object={} PWM 配置选项
 * @param options.mode:number=0 速度模式：0=低速模式, 1=高速模式
 * @param options.duty:number=0 初始占空比
 * @param options.freq:number=1000 频率（Hz）
 * @param options.channel:number 通道号（自动分配）
 * @param options.resolution:number=10 分辨率（位）
 * @param options.timer:number=0 定时器号
 * @throws 引脚未配置为 PWM
 * @throws 无可用通道
 */

/**
 * 写入 PWM 占空比
 * 
 * 设置指定引脚的 PWM 占空比值。
 * 
 * 示例：
 * ```javascript
 * import * as gpio from "gpio"
 * 
 * // 配置 PWM
 * gpio.pwm.config(2, { resolution: 10 })  // 10位分辨率，范围 0-1023
 * 
 * // 设置不同占空比
 * gpio.pwm.write(2, 0)      // 0%   (完全关闭)
 * gpio.pwm.write(2, 256)    // 25%  
 * gpio.pwm.write(2, 512)    // 50%
 * gpio.pwm.write(2, 768)    // 75%
 * gpio.pwm.write(2, 1023)   // 100% (完全打开)
 * 
 * // 渐变效果
 * for (let i = 0; i <= 1023; i += 10) {
 *     gpio.pwm.write(2, i)
 *     process.sleep(10)
 * }
 * ```
 *
 * @module gpio
 * @object pwm
 * @function write
 * @param pin:number GPIO 引脚号
 * @param value:number 占空比值（0 到 2^resolution - 1）
 * @param update:boolean=true 是否立即更新硬件
 * @throws 引脚未配置为 PWM
 */

/**
 * 更新 PWM 输出
 * 
 * 手动触发 PWM 占空比更新（通常在批量写入后使用）。
 * 
 * 示例：
 * ```javascript
 * import * as gpio from "gpio"
 * 
 * // 配置多个 PWM 引脚
 * gpio.pwm.config(2, { freq: 1000 })
 * gpio.pwm.config(4, { freq: 1000 })
 * 
 * // 批量写入（不立即更新）
 * gpio.pwm.write(2, 512, false)
 * gpio.pwm.write(4, 768, false)
 * 
 * // 同时更新所有通道
 * gpio.pwm.update(2)
 * gpio.pwm.update(4)
 * ```
 *
 * @module gpio
 * @object pwm
 * @function update
 * @param pin:number GPIO 引脚号
 * @throws 引脚未配置为 PWM
 */

/**
 * 停止 PWM 输出
 * 
 * 停止指定引脚的 PWM 输出并释放通道资源。
 * 
 * 示例：
 * ```javascript
 * import * as gpio from "gpio"
 * 
 * // 配置并启动 PWM
 * gpio.pwm.config(2, { freq: 1000, duty: 512 })
 * 
 * // ... 使用 PWM ...
 * 
 * // 停止 PWM
 * gpio.pwm.stop(2)
 * 
 * // 引脚恢复为普通 GPIO
 * gpio.setMode(2, "output")
 * gpio.write(2, 0)
 * ```
 *
 * @module gpio
 * @object pwm
 * @function stop
 * @param pin:number GPIO 引脚号
 * @throws 引脚未配置为 PWM
 */

/**
 * 最大速度模式
 * 
 * 高速模式常量，用于 pwm.config() 的 mode 参数。
 * 
 * @module gpio
 * @object pwm
 * @name maxSpeedMode
 * @type number
 * @readonly
 */

