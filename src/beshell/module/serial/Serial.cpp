/**
 * 
 * 串口外设类：
 * 
 * * [UART](UART.md)
 * 
 * * [I2C](I2C.md)
 * 
 * * [SPI](SPI.md)
 * 
 * 这些类不需要实例化，serial 模块会根据芯片的类型，自动初始化并导出串口类对象。
 * 
 * 不同型号的 ESP32 芯片，所提供的硬件串口外设数量不一样。
 * 
 * |           | UART | I2C | SPI | I2S |
 * | -----     | ----- | ----- | ----- | ----- | 
 * |  ESP32    |  3 |  2 |  3 |  2 |
 * |  ESP32 S2 |  2 |  2 |  3 |  1 |
 * |  ESP32 S3 |  3 |  2 |  3 |  2 |
 * |  ESP32 C2 |  2 |  1 |  2 |  1 |
 * |  ESP32 C3 |  2 |  1 |  2 |  1 |
 * |  ESP32 C6 |  3 |  1 |  2 |  1 |
 * |  ESP32 H2 |  2 |  2 |  2 |  1 |
 * |  ESP32 P4 |  6 |  2 |  3 |  3 |
 * 
 * 例如对于 `ESP32 S3`，serial 模块会导出：
 * 
 * ```typescript
 * {
 *     uart0: UART,
 *     uart1: UART,
 *     uart2: UART,
 * 
 *     i2c0: I2C,
 *     i2c1: I2C,
 * 
 *     spi0: SPI,
 *     spi1: SPI,
 *     spi2: SPI,
 * }
 * ```
 * 
 * > 注意：`spi0` 用于内部 flash / psram , 所以没有导出。
 * 
 * 
 * 
 * @module serial
 */
#include "Serial.hpp"
#include "UART.hpp"
#include "I2C.hpp"
#include "SPI.hpp"
#include <driver/uart.h>
#include <driver/spi_master.h>


using namespace std ;

namespace be {

    char const * const Serial::name = "serial" ;

    Serial::Serial(JSContext * ctx, const char * name)
        : NativeModule(ctx,name,0)
    {
        // UART ---------------
#if CONFIG_BESHELL_SERIAL_USE_UART
        exportName("uart0") ;
        
        #if SOC_UART_HP_NUM>1
        exportName("uart1") ;
        #endif

        #if SOC_UART_HP_NUM>2
        exportName("uart2") ;
        #endif

        #if SOC_UART_HP_NUM>3
        exportName("uart3") ;
        #endif

        #if SOC_UART_HP_NUM>4
        exportName("uart4") ;
        #endif

        #if SOC_UART_LP_NUM>0
        exportName("uartlp0") ;
        #endif
#endif

        // I2C ---------------
#if CONFIG_BESHELL_SERIAL_USE_I2C
        exportName("i2c0") ;

        #if SOC_I2C_NUM > 1
        exportName("i2c1") ;
        #endif

        #if SOC_LP_I2C_NUM > 0
        exportName("i2clp0") ;
        #endif
#endif

        // SPI ---------------
#if CONFIG_BESHELL_SERIAL_USE_SPI
        #if SOC_SPI_PERIPH_NUM>0
        exportName("spi0") ;
        #endif

        #if SOC_SPI_PERIPH_NUM>1
        exportName("spi1") ;
        #endif

        #if SOC_SPI_PERIPH_NUM>2
        exportName("spi2") ;
        #endif

        #if SOC_SPI_PERIPH_NUM>3
        exportName("spi3") ;
        #endif
#endif

    }
    
    Serial::~Serial() {
    }

    void Serial::exports(JSContext *ctx) {
        assert(ctx) ;
        assert(m) ;

        // UART ---------------
#if CONFIG_BESHELL_SERIAL_USE_UART
        UART * uart0 = UART::flyweight(ctx, UART_NUM_0) ;
        exportValue("uart0", JS_DupValue(ctx,uart0->jsobj)) ;

        #if SOC_UART_HP_NUM>1
        UART * uart1 = UART::flyweight(ctx, UART_NUM_1) ;
        if(uart1) {
            exportValue("uart1", JS_DupValue(ctx,uart1->jsobj)) ;
        }
        #endif

        #if SOC_UART_HP_NUM>2
        UART * uart2 = UART::flyweight(ctx, UART_NUM_2) ;
        if(uart2) {
            exportValue("uart2", JS_DupValue(ctx,uart2->jsobj)) ;
        }
        #endif

        #if SOC_UART_HP_NUM>3
        UART * uart3 = UART::flyweight(ctx, UART_NUM_3) ;
        if(uart3) {
            exportValue("uart3", JS_DupValue(ctx,uart3->jsobj)) ;
        }
        #endif

        #if SOC_UART_HP_NUM>4
        UART * uart4 = UART::flyweight(ctx, UART_NUM_4) ;
        if(uart4) {
            exportValue("uart4", JS_DupValue(ctx,uart4->jsobj)) ;
        }
        #endif

        #if SOC_UART_LP_NUM>0
        UART * uartlp0 = UART::flyweight(ctx, LP_UART_NUM_0) ;
        if(uartlp0) {
            exportValue("uartlp0", JS_DupValue(ctx,uartlp0->jsobj)) ;
        }
        #endif
#endif


        // I2C ---------------        
#if CONFIG_BESHELL_SERIAL_USE_I2C
        I2C * i2c0 = I2C::flyweight(ctx, I2C_NUM_0) ;
        exportValue("i2c0", JS_DupValue(ctx,i2c0->jsobj)) ;

        #if SOC_I2C_NUM > 1
        I2C * i2c1 = I2C::flyweight(ctx, I2C_NUM_1) ;
        if(i2c1) {
            exportValue("i2c1", JS_DupValue(ctx,i2c1->jsobj)) ;
        }
        #endif
        
        #if SOC_LP_I2C_NUM > 0
        I2C * i2clp0 = I2C::flyweight(ctx, LP_I2C_NUM_0) ;
        if(i2clp0) {
            exportValue("i2clp0", JS_DupValue(ctx,i2clp0->jsobj)) ;
        }
        #endif
#endif

        // SPI ---------------
#if CONFIG_BESHELL_SERIAL_USE_SPI
        #if SOC_SPI_PERIPH_NUM > 0
        SPI * spi0 = SPI::flyweight(ctx, SPI1_HOST) ;
        if(spi0) {
            exportValue("spi0", spi0->jsobj) ;
        }
        #endif
        
        #if SOC_SPI_PERIPH_NUM > 1
        SPI * spi1 = SPI::flyweight(ctx, SPI2_HOST) ;
        if(spi1) {
            exportValue("spi1", spi1->jsobj) ;
        }
        #endif

        #if SOC_SPI_PERIPH_NUM > 2
        SPI * spi2 = SPI::flyweight(ctx, SPI3_HOST) ;
        if(spi2) {
            exportValue("spi2", spi2->jsobj) ;
            }
        #endif

        #if SOC_SPI_PERIPH_NUM > 3
        SPI * spi3 = SPI::flyweight(ctx, SPI4_HOST) ;
        if(spi3) {
            exportValue("spi3", spi3->jsobj) ;
        }
        #endif
#endif
    }
    
}