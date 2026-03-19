#include "Flash.hpp"
#include "esp_idf_version.h"

namespace be{
    
    DEFINE_NCLASS_META(Partition, NativeClass)

    char const * const Flash::name = "flash" ;

    std::vector<JSCFunctionListEntry> Partition::methods = {
        JS_CFUNC_DEF("erase", 0, Partition::erase),
        JS_CFUNC_DEF("read", 0, Partition::read),
        JS_CFUNC_DEF("write", 0, Partition::write),
        JS_CFUNC_DEF("checksum", 0, Partition::checksum),
        JS_CGETSET_DEF("label", Partition::label,Partition::invalidSetter),
        JS_CGETSET_DEF("name", Partition::label,Partition::invalidSetter),
        JS_CGETSET_DEF("type", Partition::type,Partition::invalidSetter),
        JS_CGETSET_DEF("subtype", Partition::subtype,Partition::invalidSetter),
        JS_CGETSET_DEF("address", Partition::address,Partition::invalidSetter),
        JS_CGETSET_DEF("size", Partition::size,Partition::invalidSetter),
        JS_CGETSET_DEF("readonly", Partition::readonly,Partition::invalidSetter),
        JS_CGETSET_DEF("encrypted",Partition::encrypted,Partition::invalidSetter),
    } ;


    Partition::Partition(const esp_partition_t * partition, JSContext * ctx, JSValue _jsobj)
        : NativeClass(ctx,build(ctx,_jsobj))
        , partition(partition)
    {
        shared() ;
    }

    /**
     * 擦除分区指定范围的数据
     * 
     * 擦除操作会将指定范围内的 Flash 存储单元设置为全 0xFF。
     * 注意：擦除操作是必需的，在写入数据之前，必须先擦除对应区域。
     * 
     * 示例：
     * ```javascript
     * import * as flash from "flash"
     * 
     * // 获取 fs 分区
     * const fs = flash.partition("fs")
     * 
     * // 擦除从偏移量 0 开始的 4096 字节（一个扇区）
     * fs.erase(0, 4096)
     * console.log("Sector erased")
     * 
     * // 擦除整个分区（谨慎操作！）
     * // fs.erase(0, fs.size)
     * ```
     *
     * @module flash
     * @class Partition
     * @function erase
     * @param offset:number 起始偏移量
     * @param length:number 擦除长度
     * @return undefined
     * @throws 擦除失败
     */
    JSValue Partition::erase(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Partition, self)
        CHECK_ARGC(2)
        ARGV_TO_UINT32(0, offset)
        ARGV_TO_UINT32(1, length)
        esp_err_t err = esp_partition_erase_range(self->partition, offset, length) ;
        if(err!=ESP_OK) {
            JSTHROW("erase failed: %d", err)
        }
        return JS_UNDEFINED ;
    }

    /**
     * 从分区读取数据
     * 
     * 从指定的偏移位置读取指定长度的数据，返回 ArrayBuffer 对象。
     * 
     * 示例：
     * ```javascript
     * import * as flash from "flash"
     * 
     * // 获取 fs 分区
     * const fs = flash.partition("fs")
     * 
     * // 从偏移量 0 读取 1024 字节
     * const data = fs.read(0, 1024)
     * console.log("Read", data.byteLength, "bytes")
     * 
     * // 将 ArrayBuffer 转换为字符串
     * const decoder = new TextDecoder()
     * const str = decoder.decode(data)
     * console.log("Content:", str)
     * 
     * // 读取整个分区（注意内存限制，最大 1MB）
     * // const allData = fs.read(0, Math.min(fs.size, 1024 * 1024))
     * ```
     *
     * @module flash
     * @class Partition
     * @function read
     * @param offset:number 起始偏移量
     * @param length:number 读取长度
     * @return ArrayBuffer 读取到的数据
     * @throws 读取长度超过 1MB
     * @throws 超出分区范围
     * @throws 读取失败
     * @throws 内存分配失败
     */
    JSValue Partition::read(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Partition, self)
        CHECK_ARGC(2)
        ARGV_TO_UINT32(0, offset)
        ARGV_TO_UINT32(1, length)

        if(length>1024*1024) {
            JSTHROW("read length too long, max 1M")
        }
        if(offset+length>self->partition->size) {
            JSTHROW("out of partition range")
        }

        uint8_t * buffer = (uint8_t*) malloc(length) ;
        esp_err_t err = esp_partition_read(self->partition, offset, buffer, length) ;
        if(err!=ESP_OK) {
            free(buffer) ;
            JSTHROW("read failed: %d", err)
        }

        return JS_NewArrayBuffer(ctx, buffer, length, freeArrayBuffer, NULL, false) ;  
    }

    /**
     * 向分区写入数据
     * 
     * 向指定的偏移位置写入数据。支持 ArrayBuffer 或字符串类型的数据。
     * 注意：写入前必须先擦除对应区域！
     * 
     * 示例：
     * ```javascript
     * import * as flash from "flash"
     * 
     * // 获取 fs 分区
     * const fs = flash.partition("fs")
     * 
     * // 先擦除要写入的区域
     * fs.erase(0, 4096)
     * 
     * // 写入字符串
     * fs.write(0, "Hello, World!")
     * console.log("String written")
     * 
     * // 写入 ArrayBuffer
     * const buffer = new ArrayBuffer(16)
     * const view = new Uint8Array(buffer)
     * view.set([0x01, 0x02, 0x03, 0x04])
     * fs.write(100, buffer)
     * console.log("Buffer written")
     * 
     * // 验证写入的数据
     * const readBack = fs.read(0, 13)
     * const decoder = new TextDecoder()
     * console.log("Read back:", decoder.decode(readBack))
     * ```
     *
     * @module flash
     * @class Partition
     * @function write
     * @param offset:number 起始偏移量
     * @param data:ArrayBuffer|string 要写入的数据
     * @return undefined
     * @throws 写入失败
     * @throws 数据类型无效
     */
    JSValue Partition::write(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Partition, self)
        CHECK_ARGC(2)
        ARGV_TO_UINT32(0, offset)
        
        uint8_t* data = NULL;
        size_t datalen = 0;
        esp_err_t err = ESP_OK;
        
        // Check if argument is ArrayBuffer
        if (JS_IsArrayBuffer(argv[1])) {
            ARGV_TO_ARRAYBUFFER(1, buffer, bufferlen)
            data = buffer;
            datalen = bufferlen;

            err = esp_partition_write(self->partition, offset, data, datalen);
            if(err != ESP_OK) {
                JSTHROW("write failed: %d", err)
            }
        }
        // Check if argument is string
        else if (JS_IsString(argv[1])) {

            size_t str_len = 0;
            const char* str = JS_ToCStringLen(ctx, &str_len, argv[1]);
            if (!str) {
                JSTHROW("invalid string data")
            }
            
            err = esp_partition_write(self->partition, offset, (const uint8_t*)str, str_len);
            JS_FreeCString(ctx, str);
            
            if (err != ESP_OK) {
                JSTHROW("write failed: %d", err)
            }
        } else {
            JSTHROW("write data must be an ArrayBuffer or string")
        }
        
        return JS_UNDEFINED;
    }

    /**
     * 计算分区指定范围的数据校验和（所有字节累加）
     * 
     * 计算指定范围内所有字节的累加和，用于数据完整性验证。
     * 
     * 示例：
     * ```javascript
     * import * as flash from "flash"
     * 
     * // 获取 fs 分区
     * const fs = flash.partition("fs")
     * 
     * // 写入一些数据
     * fs.erase(0, 4096)
     * fs.write(0, "Test data")
     * 
     * // 计算前 1024 字节的校验和
     * const sum1 = fs.checksum(1024, 0)
     * console.log("Checksum:", sum1)
     * 
     * // 修改数据
     * fs.erase(0, 4096)
     * fs.write(0, "Modified data")
     * 
     * // 重新计算校验和
     * const sum2 = fs.checksum(1024, 0)
     * console.log("New checksum:", sum2)
     * 
     * // 校验和不同时说明数据已改变
     * if (sum1 !== sum2) {
     *     console.log("Data has changed")
     * }
     * 
     * // 计算整个分区的校验和（注意内存和性能）
     * // const totalSum = fs.checksum(fs.size, 0)
     * ```
     *
     * @module flash
     * @class Partition
     * @function checksum
     * @param length:number 计算长度
     * @param offset:number=0 起始偏移量
     * @return number 校验和值
     * @throws 超出分区范围
     * @throws 内存分配失败
     * @throws 读取失败
     */
    JSValue Partition::checksum(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        THIS_NCLASS(Partition, self)
        CHECK_ARGC(1)
        ARGV_TO_UINT32(0, length)
        ARGV_TO_UINT32_OPT(1, offset, 0)

        // Validate parameters
        if (offset + length > self->partition->size) {
            JSTHROW("checksum range exceeds partition size")
        }

        // Allocate a buffer for reading in chunks (64KB chunks to balance memory usage and performance)
        const size_t BUFFER_SIZE = 64 * 1024;
        uint8_t* buffer = (uint8_t*)malloc(BUFFER_SIZE);
        if (!buffer) {
            JSTHROW("failed to allocate memory for checksum calculation")
        }

        uint32_t checksum = 0;
        uint32_t remaining = length;
        uint32_t current_offset = offset;

        while (remaining > 0) {
            uint32_t read_size = (remaining > BUFFER_SIZE) ? BUFFER_SIZE : remaining;
            
            esp_err_t err = esp_partition_read(self->partition, current_offset, buffer, read_size);
            if (err != ESP_OK) {
                free(buffer);
                JSTHROW("checksum read failed: %d", err)
            }

            // Calculate checksum by summing all bytes
            for (uint32_t i = 0; i < read_size; i++) {
                checksum += buffer[i];
            }
            
            remaining -= read_size;
            current_offset += read_size;
        }

        free(buffer);
        return JS_NewInt32(ctx, checksum);
    }

    /**
     * 获取分区标签名称
     * 
     * @module flash
     * @class Partition
     * @function label
     * @return string 分区标签
     */
    JSValue Partition::label(JSContext *ctx, JSValueConst this_val) {
        THIS_NCLASS(Partition, self)
        return JS_NewString(ctx, self->partition->label) ;
    }

    /**
     * 获取分区类型
     * 
     * @module flash
     * @class Partition
     * @function type
     * @return string 分区类型，可能值为 "app"、"data"、"bootloader"、"partition_table" 或 "UNKNOWN"
     */
    JSValue Partition::type(JSContext *ctx, JSValueConst this_val) {
        THIS_NCLASS(Partition, self)
        switch(self->partition->type) {
            case ESP_PARTITION_TYPE_APP: return JS_NewString(ctx, "app") ;
            case ESP_PARTITION_TYPE_DATA: return JS_NewString(ctx, "data") ;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
            case ESP_PARTITION_TYPE_BOOTLOADER: return JS_NewString(ctx, "bootloader") ;
            case ESP_PARTITION_TYPE_PARTITION_TABLE: return JS_NewString(ctx, "partition_table") ;
#endif
            default: return JS_NewString(ctx, "UNKNOWN") ;
        }
    }

    /**
     * 获取分区子类型
     * 
     * @module flash
     * @class Partition
     * @function subtype
     * @return number 子类型值
     */
    JSValue Partition::subtype(JSContext *ctx, JSValueConst this_val) {
        THIS_NCLASS(Partition, self)
        return JS_NewInt32(ctx, self->partition->subtype) ;
    }

    /**
     * 获取分区起始地址
     * 
     * @module flash
     * @class Partition
     * @function address
     * @return number 起始地址
     */
    JSValue Partition::address(JSContext *ctx, JSValueConst this_val) {
        THIS_NCLASS(Partition, self)
        return JS_NewInt32(ctx, self->partition->address) ;
    }

    /**
     * 获取分区大小
     * 
     * @module flash
     * @class Partition
     * @function size
     * @return number 分区大小（字节）
     */
    JSValue Partition::size(JSContext *ctx, JSValueConst this_val) {
        THIS_NCLASS(Partition, self)
        return JS_NewInt32(ctx, self->partition->size) ;
    }

    /**
     * 检查分区是否只读
     * 
     * @module flash
     * @class Partition
     * @function readonly
     * @return bool 是否只读
     */
    JSValue Partition::readonly(JSContext *ctx, JSValueConst this_val) {
        THIS_NCLASS(Partition, self)
        return JS_NewBool(ctx, self->partition->readonly) ;
    }

    /**
     * 检查分区是否加密
     * 
     * @module flash
     * @class Partition
     * @function encrypted
     * @return bool 是否加密
     */
    JSValue Partition::encrypted(JSContext *ctx, JSValueConst this_val) {
        THIS_NCLASS(Partition, self)
        return JS_NewBool(ctx, self->partition->encrypted) ;
    }

    JSValue Partition::invalidSetter(JSContext *ctx, JSValueConst this_val, JSValueConst value) {
        JSTHROW("property is readonly") ;
    }



    Flash::Flash(JSContext * ctx, const char * name)
        : NativeModule(ctx, name, 0)
    {
        exportFunction("allPartitions",allPartitions,0) ;
        exportFunction("partition",partition,0) ;
    }

    /**
     * 获取所有 Flash 分区的列表
     * 
     * 返回一个对象，包含系统中所有的 Flash 分区信息。
     * 
     * 示例：
     * ```javascript
     * import * as flash from "flash"
     * 
     * // 获取所有分区
     * const partitions = flash.allPartitions()
     * 
     * // 遍历所有分区
     * for (let label in partitions) {
     *     let part = partitions[label]
     *     console.log("Label:", part.label)
     *     console.log("Type:", part.type)
     *     console.log("Size:", part.size, "bytes")
     *     console.log("Address:", part.address)
     *     console.log("Readonly:", part.readonly)
     * }
     * 
     * // 获取特定分区
     * const fsPartition = partitions["fs"]
     * if (fsPartition) {
     *     console.log("FS partition size:", fsPartition.size)
     * }
     * ```
     *
     * @module flash
     * @function allPartitions
     * @return object 包含所有分区的对象，key 为分区标签，value 为 [Partition](Partition.html "Partition(Flash的分区类)") 对象
     * @throws 无法找到分区
     */
    JSValue Flash::allPartitions(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        
        // 创建分区迭代器（匹配所有类型和子类型）
        esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
        if (it == NULL) {
            JSTHROW("can't find partition")
        }

        JSValue list = JS_NewObject(ctx) ;

        const esp_partition_t *partition;
        while (it && (partition = esp_partition_get(it)) != NULL) {

            auto jspartition = new Partition(partition, ctx) ;
            JS_SetPropertyStr(ctx, list, partition->label, jspartition->jsobj) ;
            
            it = esp_partition_next(it);
        }

        esp_partition_iterator_release(it);

        return list ;
    }

    /**
     * 根据名称获取指定的 Flash 分区，返回值是一个 [Partition](#Partition.html "Partition(Flash的分区类)") 类
     * 
     * 通过分区标签名称获取特定的 Flash 分区对象。
     * 
     * 示例：
     * ```javascript
     * import * as flash from "flash"
     * 
     * // 获取名为 "fs" 的分区（通常是文件系统分区）
     * const fsPart = flash.partition("fs")
     * console.log("FS partition:", fsPart.label)
     * console.log("Size:", fsPart.size)
     * 
     * // 获取应用程序分区
     * const appPart = flash.partition("app")
     * console.log("App partition type:", appPart.type)
     * 
     * // 读取分区数据
     * const data = fsPart.read(0, 1024)
     * console.log("Read", data.byteLength, "bytes")
     * 
     * // 写入数据（注意：只适用于可写分区）
     * fsPart.write(0, "Hello World")
     * 
     * // 计算校验和
     * const checksum = fsPart.checksum(1024, 0)
     * console.log("Checksum:", checksum)
     * ```
     *
     * @module flash
     * @function partition
     * @param name:string 分区名称
     * @return [Partition](#Partition.html) 分区对象
     * @throws 分区不存在
     */
    JSValue Flash::partition(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
        CHECK_ARGC(1)
        std::string ARGV_TO_STRING(0, name)

        const esp_partition_t * partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, name.c_str()) ;
        if(!partition) {
            JSTHROW("partition not found")
        }
        auto jspartition = new Partition(partition, ctx) ;
        return jspartition->jsobj ;
    }
}
