#include "driver/i2c_master.h"
#if !BESHELL_SERIAL_I2C_USE_LEGACY

#include "I2C.hpp"
#include "hal/i2c_types.h"
#include "qjs_utils.h"
#include "thread.hpp"
#include <JSEngine.hpp>
#include <esp_log.h>
#include <cstdlib>
#include <stdlib.h>
#include <malloc.h>
#include <cstdlib>
#include <climits>

using namespace std ;

#define JSCHECK_MASTER                              \
    if(that->mode!=I2C_MODE_MASTER) {               \
        JSTHROW("I2C is not in %s mode", "master") ;\
    }
#define JSCHECK_SLAVE                               \
    if(that->mode!=I2C_MODE_SLAVE) {                \
        JSTHROW("I2C is not in %s mode", "slave") ; \
    }
    
namespace be {

    
    DEFINE_NCLASS_META(I2C, NativeClass)

    I2C * I2C::i2c0 = nullptr ;
    #if SOC_I2C_NUM > 1
    I2C * I2C::i2c1 = nullptr ;
    #endif
    #if SOC_LP_I2C_NUM > 0
    I2C * I2C::i2clp0 = nullptr ;
    #endif

    std::vector<JSCFunctionListEntry> I2C::methods = {
        JS_CFUNC_DEF("setup", 1, I2C::setup),
        JS_CFUNC_DEF("unsetup", 1, I2C::unsetup),
        JS_CFUNC_DEF("isInstalled", 1, I2C::isInstalled),
        JS_CFUNC_DEF("addDevice", 1, I2C::addDevice),
        JS_CFUNC_DEF("removeDevice", 1, I2C::removeDevice),
        JS_CFUNC_DEF("ping", 1, I2C::ping),
        JS_CFUNC_DEF("scan", 1, I2C::scan),
        JS_CFUNC_DEF("send", 2, I2C::send),
        JS_CFUNC_DEF("recv", 2, I2C::recv),
        JS_CFUNC_DEF("recvU8", 2, I2C::recvU8),
        
        JS_CFUNC_DEF("write8", 2, I2C::write8),
        JS_CFUNC_DEF("write16", 2, I2C::write16),
        JS_CFUNC_DEF("write32", 2, I2C::write32),
        JS_CFUNC_DEF("readI8", 2, I2C::readI8),
        JS_CFUNC_DEF("readI16", 2, I2C::readI16),
        JS_CFUNC_DEF("readI32", 2, I2C::readI32),
        JS_CFUNC_DEF("readU8", 2, I2C::readU8),
        JS_CFUNC_DEF("readU16", 2, I2C::readU16),
        JS_CFUNC_DEF("readU32", 2, I2C::readU32),

        #if SOC_I2C_SUPPORT_SLAVE
        // JS_CFUNC_DEF("listen", 2, I2C::listen),
        // JS_CFUNC_DEF("slaveWrite", 2, I2C::slaveWrite),
        // JS_CFUNC_DEF("slaveWriteReg", 2, I2C::slaveWriteReg),
        // JS_CFUNC_DEF("slaveWriteBuff", 2, I2C::slaveWriteBuff),
        // JS_CFUNC_DEF("slaveReadBuff", 2, I2C::slaveReadBuff),
        #endif
    } ;

    I2C::I2C(JSContext * ctx, i2c_port_t busnum)
        : NativeClass(ctx, build(ctx))
        , busnum(busnum)
    {
    }
    
    I2C::~I2C() {
        // vSemaphoreDelete(sema) ;
    }
    
    JSValue I2C::constructor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        i2c_port_t busnum = I2C_NUM_0 ;
        if(argc>0) {
            JS_ToUint32(ctx, (uint32_t*)&busnum, argv[0]) ;
        }
        auto obj = new I2C(ctx,busnum) ;
        obj->self = std::shared_ptr<I2C> (obj) ;
        return obj->jsobj ;
    }
    
    I2C * I2C::flyweight(JSContext * ctx, i2c_port_t bus) {
        if(bus==I2C_NUM_0) {
            if(!i2c0) {
                i2c0 = new I2C(ctx, I2C_NUM_0) ;
            }
            return i2c0 ;
        }
        #if SOC_I2C_NUM > 1
        else if(bus==I2C_NUM_1) {
            if(!i2c1) {
                i2c1 = new I2C(ctx, I2C_NUM_1) ;
            }
            return i2c1 ;
        }
        #endif
        #if SOC_LP_I2C_NUM > 0
        else if(bus==LP_I2C_NUM_0) {
            if(!i2clp0) {
                i2clp0 = new I2C(ctx, LP_I2C_NUM_0) ;
            }
            return i2clp0 ;
        }
        #endif
        return nullptr ;
    }

    
    bool I2C::addDevice(i2c_device_config_t & config, uint8_t regAddrBits, uint8_t regBits) {
        
        if (!isInstalled()) {
            return false;
        }
        
        uint16_t addr_key = (config.dev_addr_length==I2C_ADDR_BIT_LEN_10)? (config.device_address | (30<<10)) : config.device_address ;
        if(devices.count(addr_key)>0) {
            devices[addr_key].addr = config.device_address ;
            devices[addr_key].addr_len = config.dev_addr_length ;
            devices[addr_key].reg_addr_bits = regAddrBits ;
            devices[addr_key].reg_bits = regBits ;

            return true;
        }

        else {
            inner::i2c_device_memo_t dev ;
            dev.addr = config.device_address ;
            dev.addr_len = config.dev_addr_length ;
            dev.reg_addr_bits = regAddrBits ;
            dev.reg_bits = regBits ;
        
            esp_err_t res = i2c_master_bus_add_device(bus_handle, &config, &dev.dev_handle);
            if (res != ESP_OK) {
                return false ;
            }
            
            devices[addr_key] = dev;
            return true ;
        }
    }


    bool I2C::addDevice(JSContext *ctx, JSValue devOption) {

        if(!bus_handle || mode!=I2C_MODE_MASTER) {
            JS_ThrowReferenceError(ctx, "I2C bus is not initialized in master mode") ;
            return false;
        }

        i2c_device_config_t devConfig = {} ;
        GET_UINT_PROP(devOption, "addr", devConfig.device_address, uint16_t, {
            JS_ThrowReferenceError(ctx, "device address is required") ;
            return false ;
        })
        bool GET_BOOL_PROP_OPT(devOption, "addrBit10", addrBit10, false)
        GET_UINT32_PROP_OPT(devOption, "freq", devConfig.scl_speed_hz, 100000)
        GET_UINT32_PROP_OPT(devOption, "timeout", devConfig.scl_wait_us, 1000)
        GET_BOOL_PROP_OPT(devOption, "ackCheck", devConfig.flags.disable_ack_check, true)

        uint8_t GET_UINT8_PROP_OPT(devOption, "regAddrBits", regAddrBits, 8)
        uint8_t GET_UINT8_PROP_OPT(devOption, "regBits", regBits, regAddrBits)

        if(regAddrBits!=8 && regAddrBits!=16 && regAddrBits!=32) {
            JS_ThrowReferenceError(ctx, "invalid regBits: %d", regAddrBits) ;
            return false;
        }
        if(regBits!=8 && regBits!=16 && regBits!=32) {
            JS_ThrowReferenceError(ctx, "invalid regBits: %d", regBits) ;
            return false;
        }

        devConfig.dev_addr_length = addrBit10 ? I2C_ADDR_BIT_LEN_10 : I2C_ADDR_BIT_LEN_7 ;
        if(!addDevice(devConfig, regAddrBits, regBits)) {
            JS_ThrowReferenceError(ctx, "failed to add i2c device") ;
            return false ;
        }

        return true;
    }
    
    /**
     * 配置并启动 I2C 总线。
     * 
     * options: {
     *     sda: number ,                        // GPIO number for SDA
     *     scl: number ,                        // GPIO number for SCL
     *     mode: number = 1                     // 0: slaver , 1: master
     *     core: number = 0                     // cpu core to run i2c driver
     * 
     *     dev: [
     *         {
     *             addr: number ,               // device address
     *             addrBit10: bool = false ,    // device address
     *             regAddrBits: number = 8 ,    // register address bits , 8 or 16
     *             regBits: number = 8 ,        // register value bits , 8 or 16
     *             freq: number = 100000 ,      // clock speed for master mode
     *             timeout: number = 1000 ,     // timeout in us
     *             ackCheck: bool = true ,      // enable ACK check
     *         } , ...
     *     ]
     * }
     * 
     * @module serial
     * @class I2C
     * @function setup
     * @param options: Object
     * @return undefined
     */
    JSValue I2C::setup(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(I2C, that)
        ASSERT_ARGC(1)

        GET_GPIO_PROP(argv[0], "sda", that->_sda, )
        GET_GPIO_PROP(argv[0], "scl", that->_scl, )
        i2c_mode_t GET_UINT_PROP_OPT(argv[0], "mode", mode, i2c_mode_t, I2C_MODE_MASTER)

        auto cleanup_master_bus = [&]() {
            if(!that->devices.empty()) {
                for(auto &entry : that->devices) {
                    if(entry.second.dev_handle) {
                        i2c_master_bus_rm_device(entry.second.dev_handle) ;
                        entry.second.dev_handle = nullptr ;
                    }
                }
                that->devices.clear() ;
            }
            if(that->bus_handle) {
                i2c_del_master_bus(that->bus_handle) ;
                that->bus_handle = nullptr ;
            }
        };

        esp_err_t res = ESP_OK ;
        i2c_port_t bus = that->busnum ;

        if(mode==I2C_MODE_MASTER) {
            that->mode = mode ;
            
            uint32_t clk_source = I2C_CLK_SRC_DEFAULT ;
            GET_UINT32_PROP_OPT(argv[0], "clk_source", clk_source, I2C_CLK_SRC_DEFAULT)
            uint32_t glitch_ignore_cnt = 7 ;
            GET_UINT32_PROP_OPT(argv[0], "glitch_ignore_cnt", glitch_ignore_cnt, 7)
            bool enable_internal_pullup = true ;
            GET_BOOL_PROP_OPT(argv[0], "internal_pullup", enable_internal_pullup, true)
            bool allow_power_down = false ;
            GET_BOOL_PROP_OPT(argv[0], "allow_pd", allow_power_down, false)
            int intr_priority = 0 ;
            GET_INT32_PROP_OPT(argv[0], "intr_priority", intr_priority, 0)
            uint32_t trans_queue_depth = 0 ;
            GET_UINT32_PROP_OPT(argv[0], "trans_queue_depth", trans_queue_depth, 0)
            uint8_t GET_UINT8_PROP_OPT(argv[0], "core", core, 0)

            i2c_master_bus_config_t bus_config = {} ;
            bus_config.i2c_port = bus ;
            bus_config.sda_io_num = that->_sda ;
            bus_config.scl_io_num = that->_scl ;
            bus_config.clk_source = static_cast<i2c_clock_source_t>(clk_source) ;
            bus_config.glitch_ignore_cnt = static_cast<uint8_t>(glitch_ignore_cnt) ;
            bus_config.intr_priority = intr_priority ;
            bus_config.trans_queue_depth = trans_queue_depth ;
            bus_config.flags.enable_internal_pullup = enable_internal_pullup ;
            bus_config.flags.allow_pd = allow_power_down ;

            run_wait_on_core([&]() {
                cleanup_master_bus() ;
                res = i2c_new_master_bus(&bus_config, &that->bus_handle) ;
            }, core) ;

            if(res!=ESP_OK) {
                JSTHROW("failed to setup i2c master bus: %d", res)
            }

            JSValue dev = JS_GetPropertyStr(ctx, that->jsobj, "dev") ;
            if( JS_IsObject(dev) ) {
                uint32_t len = 0 ;
                JSValue len_val = JS_GetPropertyStr(ctx, dev, "length") ;
                JS_ToUint32(ctx, &len, len_val) ;
                JS_FreeValue(ctx, len_val) ;

                for(uint32_t i=0;i<len;i++) {
                    JSValue dev_obj = JS_GetPropertyUint32(ctx, dev, i) ;
                    if( !JS_IsObject(dev_obj) ) {
                        JS_FreeValue(ctx, dev_obj) ;
                        continue ;
                    }

                    if( !that->addDevice(ctx, dev_obj) ) {
                        JS_FreeValue(ctx, dev) ;
                        JS_FreeValue(ctx, dev_obj) ;
                        return JS_EXCEPTION ;
                    }

                    JS_FreeValue(ctx, dev_obj) ;
                }
            }
            JS_FreeValue(ctx, dev) ;

            return JS_UNDEFINED ;
        }

        #if SOC_I2C_SUPPORT_SLAVE
        if(mode==I2C_MODE_SLAVE) {
            cleanup_master_bus() ;
            JSTHROW("I2C slave mode is not supported by NG driver yet")
        }
        #endif

        JSTHROW("invalid mode")
    }

    /**
     * 添加一个设备的信息
     * 
     * options: {
     *     addr: number ,               // device address
     *     addrBit10: bool = false ,    // device address
     *     regBits: number = 8 ,        // register address bits , 8 or 16
     *     freq: number = 100000 ,      // clock speed for master mode
     *     timeout: number = 1000 ,     // timeout in us
     *     ackCheck: bool = true ,      // enable ACK check
     * }
     * 
     * @module serial
     * @class I2C
     * @function addDevice
     * @param options: Object
     * @return undefined
     */
    JSValue I2C::addDevice(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(I2C, that)
        JSCHECK_MASTER
        ASSERT_ARGC(1)
        if( !that->addDevice(ctx, argv[0]) ) {
            return JS_EXCEPTION ;
        }
        return JS_UNDEFINED ;
    }

    /**
     * 从总线移除一个设备
     * 
     * @module serial
     * @class I2C
     * @function addDevice
     * @param address: number
     * @param bit10:bool=false
     * @return undefined
     */
    JSValue I2C::removeDevice(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(I2C, that)
        JSCHECK_MASTER
        ASSERT_ARGC(1)
        ARGV_TO_INT16(0, addr)
        bool bit10 = addr > 0x7F;
        if(argc>1) {
            bit10 = JS_ToBool(ctx, argv[1]) ;   
        }

        uint16_t addr_key = bit10 ? (addr | (30<<10)) : addr ;
        if(that->devices.count(addr_key)==0) {
            JSTHROW("i2c device 0x%02X not found", addr) ;
        }
        inner::i2c_device_memo_t & dev = that->devices[addr_key] ;
        if(dev.dev_handle) {
            esp_err_t res = i2c_master_bus_rm_device(dev.dev_handle) ;
            if(res!=ESP_OK) {
                JSTHROW("failed to remove i2c device 0x%02X: %d", addr, res) ;
            }
            dev.dev_handle = nullptr ;
        }
        that->devices.erase(addr_key) ;

        return JS_UNDEFINED ;
    }

    JSValue I2C::unsetup(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(I2C, that)
        #if SOC_I2C_SUPPORT_SLAVE
        // if(that->slaverRegisters) {
        //     free(that->slaverRegisters) ;
        //     that->slaverRegisters = nullptr ;
        //     that->slaverRegisterLength = 0 ;
        // }

        // if(that->slaveTask) {
        //     vTaskDelete(that->slaveTask);
        //     that->slaveTask = nullptr ;
        // }
        #endif

        esp_err_t res = ESP_OK ;
        if(!that->devices.empty()) {
            for(auto &entry : that->devices) {
                if(entry.second.dev_handle) {
                    esp_err_t rm_res = i2c_master_bus_rm_device(entry.second.dev_handle) ;
                    if(rm_res != ESP_OK) {
                        res = rm_res ;
                    }
                    entry.second.dev_handle = nullptr ;
                }
            }
            that->devices.clear() ;
        }

        if(that->bus_handle) {
            esp_err_t del_res = i2c_del_master_bus(that->bus_handle) ;
            if(del_res != ESP_OK && res == ESP_OK) {
                res = del_res ;
            }
            that->bus_handle = nullptr ;
        }

        that->mode = I2C_MODE_MASTER ;
        return res==ESP_OK? JS_TRUE: JS_FALSE ;
    }
    JSValue I2C::isInstalled(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(I2C, that)
        return that->isInstalled()? JS_TRUE: JS_FALSE ;
    }

    bool I2C::isInstalled() const {
        i2c_master_bus_handle_t handle = nullptr;
        esp_err_t err = i2c_master_get_bus_handle(busnum, &handle);
        return err == ESP_OK && handle != nullptr;
    }

    inner::i2c_device_memo_t * I2C::devMemo(uint16_t addr, bool bit10) {

        uint16_t addr_key = bit10 ? (addr | (30<<10)) : addr ;
        if(devices.count(addr_key)>0) {
            return & devices[addr_key] ;
        }

        if( !isInstalled() || mode != I2C_MODE_MASTER ) {
            return nullptr ;
        }
        if(!bus_handle) {
            esp_err_t res = i2c_master_get_bus_handle(busnum, &bus_handle);
            if (res != ESP_OK) {
                return nullptr;
            }
        }
        
        // Create device handle with default configuration
        i2c_device_config_t dev_config = {
            .dev_addr_length = bit10 ? I2C_ADDR_BIT_LEN_10 : I2C_ADDR_BIT_LEN_7,
            .device_address = addr ,
            .scl_speed_hz = 100000,
            .scl_wait_us = 1000,
            .flags = {
                .disable_ack_check = true,
            }
        };
        i2c_master_dev_handle_t dev_handle = nullptr;
        esp_err_t res = i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle);
        if (res != ESP_OK) {
            return nullptr;
        }

        inner::i2c_device_memo_t device_info = {
            .addr = addr ,
            .dev_handle = dev_handle,
            .addr_len = dev_config.dev_addr_length ,
            .reg_addr_bits = 8 ,
            .reg_bits = 8 ,
        } ;
        devices[addr_key] = device_info ;

        return & devices[addr_key] ;
    }
    uint8_t I2C::devRegBit(uint16_t addr, bool bit10) {
        inner::i2c_device_memo_t * memo = devMemo(addr, bit10);
        if(!memo) {
            return 8 ;
        }
        return memo->reg_addr_bits ;
    }
    i2c_master_dev_handle_t I2C::devHandle(uint16_t addr, bool bit10) {
        inner::i2c_device_memo_t * memo = devMemo(addr, bit10);
        return memo ? memo->dev_handle : nullptr;
    }

    bool I2C::ping(uint16_t addr, uint32_t timeout_ms) {
        if(mode!=I2C_MODE_MASTER) {
            return false ;
        }
        int probe_timeout = (timeout_ms > static_cast<uint32_t>(INT_MAX)) ? INT_MAX : static_cast<int>(timeout_ms) ;
        
        esp_log_level_t loglevel = esp_log_level_get("i2c.master");
        esp_log_level_set("i2c.master", ESP_LOG_WARN);

        esp_err_t ret = i2c_master_probe(bus_handle, addr, probe_timeout) ;

        esp_log_level_set("i2c.master", loglevel);
        
        return ret==ESP_OK ;
    }

    void I2C::scan(uint16_t from, uint16_t to, uint32_t timeout_ms) {
        if(mode!=I2C_MODE_MASTER) {
            return ;
        }
        for(uint8_t addr=from; addr<=to; addr++) {
            if( ping(addr, timeout_ms) ){
                printf("found device: 0x%02x\n", addr) ;
            }
        }
    }

    bool I2C::send(uint16_t addr, uint8_t * data, size_t data_len) {
        if(mode != I2C_MODE_MASTER) {
            return false;
        }

        bool bit10 = addr > 0x7F;
        const uint16_t ten_bit_key = static_cast<uint16_t>(addr | (30 << 10));
        if(!bit10 && devices.count(ten_bit_key) > 0) {
            bit10 = true;
        }

        auto handle = devHandle(addr, bit10);
        if(!handle && bit10) {
            handle = devHandle(addr, false);
        }
        if(!handle) {
            return false;
        }

        constexpr int default_timeout_ms = 10;

        if(data_len == 0) {
            i2c_master_transmit_multi_buffer_info_t buffer_info = {};
            buffer_info.write_buffer = nullptr;
            buffer_info.buffer_size = 0;
            return i2c_master_multi_buffer_transmit(handle, &buffer_info, 1, default_timeout_ms) == ESP_OK;
        }

        if(!data) {
            return false;
        }

        return i2c_master_transmit(handle, data, data_len, default_timeout_ms) == ESP_OK;
    }

    bool I2C::recv(uint16_t addr, uint8_t * buff, size_t buffsize) {
        if(mode != I2C_MODE_MASTER || !buff || buffsize == 0) {
            return false;
        }

        bool bit10 = addr > 0x7F;
        const uint16_t ten_bit_key = static_cast<uint16_t>(addr | (30 << 10));
        if(!bit10 && devices.count(ten_bit_key) > 0) {
            bit10 = true;
        }

        auto handle = devHandle(addr, bit10);
        if(!handle && bit10) {
            handle = devHandle(addr, false);
        }
        if(!handle) {
            return false;
        }

        constexpr int default_timeout_ms = 10;
        return i2c_master_receive(handle, buff, buffsize, default_timeout_ms) == ESP_OK;
    }


    // JS binding
    JSValue I2C::ping(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        ASSERT_ARGC(1)
        THIS_NCLASS(I2C, that)
        JSCHECK_MASTER
        ARGV_TO_UINT16(0, addr)
        return that->ping(addr)? JS_TRUE: JS_FALSE ;
    }
    JSValue I2C::scan(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(I2C, that)
        JSCHECK_MASTER
        ARGV_TO_UINT8_OPT(0, from, 1)
        ARGV_TO_UINT8_OPT(1, to, 127)
        ARGV_TO_UINT32_OPT(2, timeout_ms, 20)
        that->scan(from,to, timeout_ms) ;
        return JS_UNDEFINED ;
    }
    JSValue I2C::send(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        ASSERT_ARGC(2)
        THIS_NCLASS(I2C, that)
        JSCHECK_MASTER
        ARGV_TO_UINT16(0, addr)
        if(!JS_IsArray(ctx, argv[1])) {
            JSTHROW("arg must be a array")
        }
        int len ;
        uint8_t * data = JS_ArrayToBufferUint8(ctx, argv[1], &len) ;
        if(data) {
            bool res = that->send(addr,data,len) ;
            free(data) ;
            return res? JS_TRUE: JS_FALSE ;
        } else {
            return JS_FALSE ;
        }
    }

    #define I2C_WRITE(type, ARGV_CONVERT)       \
        ASSERT_ARGC(3)                          \
        THIS_NCLASS(I2C, that)                  \
        JSCHECK_MASTER                          \
        ARGV_TO_UINT16(0, addr)                 \
        ARGV_TO_UINT8(1, reg)                   \
        ARGV_CONVERT(2, byte)                   \
        if(that->devices.count(addr)==0 && !that->devHandle(addr)) {    \
            JSTHROW("device 0x%02x is not installed", addr) ;           \
        }                                       \
        return  that->write<type>(addr, reg, byte)? JS_TRUE: JS_FALSE ;

    JSValue I2C::write8(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        I2C_WRITE(uint8_t,ARGV_TO_UINT8)
    }
    JSValue I2C::write16(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        I2C_WRITE(uint16_t,ARGV_TO_UINT16)
    }
    JSValue I2C::write32(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        I2C_WRITE(uint32_t,ARGV_TO_UINT32)
    }

    JSValue I2C::recv(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        ASSERT_ARGC(2)
        ARGV_TO_UINT16(0, addr)
        ARGV_TO_UINT8(1, len)
        THIS_NCLASS(I2C, that)
        JSCHECK_MASTER
        if(len<1) {
            JSTHROW("invalid recv length")
        }
        uint8_t * buffer = (uint8_t*)malloc(len) ;
        if(!buffer) {
            JSTHROW("out of memory?") ;
        }
        if(!that->recv(addr,buffer,len)){
            free(buffer) ;
            return JS_NULL;
        }
        return JS_NewArrayBuffer(ctx, buffer, len, freeArrayBuffer, NULL, false) ;
    }

    JSValue I2C::recvU8(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        ASSERT_ARGC(1)
        ARGV_TO_UINT16(0, addr)
        THIS_NCLASS(I2C, that)
        JSCHECK_MASTER
        uint8_t byte ;
        if(!that->recv(addr,&byte,1)){
            JSTHROW("i2c recv failed")
        }
        return JS_NewUint32(ctx,byte) ;
    }

    JSValue I2C::readI8(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        return readRegInt<int8_t>(ctx, this_val, argc, argv, true) ;
    }
    JSValue I2C::readI16(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        return readRegInt<int16_t>(ctx, this_val, argc, argv, true) ;
    }
    JSValue I2C::readI32(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        return readRegInt<int32_t>(ctx, this_val, argc, argv, true) ;
    }
    JSValue I2C::readU8(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        return readRegInt<uint8_t>(ctx, this_val, argc, argv, true) ;
    }
    JSValue I2C::readU16(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        return readRegInt<uint16_t>(ctx, this_val, argc, argv, true) ;
    }
    JSValue I2C::readU32(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        return readRegInt<uint32_t>(ctx, this_val, argc, argv, true) ;
    }
}

#endif
