#include <cstdint>
#ifdef ESP_PLATFORM

#include "logger-imp.hpp"
#include "debug.h"
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "esp_partition.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"
#include "esp_heap_caps.h"
#include "esp_err.h"
#include "esp_system.h"
#include "soc/soc.h"

using namespace std;

#define FLASH_SECTOR_SIZE 4096

#define LOG_META_MAGIC 0xC4F474D5
// #define LOG_META_SIZE FLASH_SECTOR_SIZE

#define TASK_STACK 4*1024
#define TASK_PRIORITY 3

#define ACTION_WRITE 1
#define ACTION_CLEAR 10
#define ACTION_ERASE 11

typedef struct {
    uint8_t op; // 操作类型
    uint8_t * data; // 数据内容
    size_t data_size; // 数据内容
} log_action_t;

// 元数据结构定义
typedef struct {
    uint32_t magic;           // 魔数，用于验证元数据有效性
    size_t write_offset;      // 当前写入偏移
    bool is_wrapped;          // 是否发生循环
} log_meta_t;

log_meta_t s_meta = {
    .magic = LOG_META_MAGIC, 
    .write_offset = 0,
    .is_wrapped = false,
} ;


// 静态变量
static const esp_partition_t* s_log_partition = nullptr;
static bool s_in_log_write = false;
static bool s_in_log_flush = false;
static size_t s_partition_start = false;
static size_t s_partition_end = false;
#define s_data_addr (s_partition_start+FLASH_SECTOR_SIZE)


// FreeRTOS 任务相关变量
static TaskHandle_t s_log_task_handle = nullptr;
static QueueHandle_t s_log_queue = nullptr;

extern "C" {
    // 声明原始函数
    int __real_printf(const char *format, ...);
    int __real_vprintf(const char *format, va_list args);
    int __real_write(int fd, const void *buf, size_t count);
};

static bool meta_load() {
    if(!s_log_partition) {
        return false;
    }
    log_meta_t meta;
    esp_err_t err = esp_partition_read(s_log_partition, s_partition_start, (void *)&meta, sizeof(log_meta_t));
    if (err != ESP_OK) {
        return false;
    }
    if (meta.magic != LOG_META_MAGIC) {
        return false;
    }

    size_t max_data_len = s_partition_end - s_partition_start - FLASH_SECTOR_SIZE ;
    if(meta.write_offset >= max_data_len) {
        return false;
    }

    s_meta = meta;
    return true;
}

static void meta_save() {
    // dn2(s_meta.write_offset, s_meta.is_wrapped)
    esp_partition_erase_range(s_log_partition, s_partition_start, FLASH_SECTOR_SIZE);
    esp_partition_write(s_log_partition, s_partition_start, &s_meta, sizeof(log_meta_t));
}

static void _write_to_flash(size_t addr, uint8_t * buffer, size_t length) {
    if(!s_log_partition) {
        return ;
    }
    
    // 确保写入地址和长度在有效范围内
    if(addr < s_data_addr || addr >= s_partition_end) {
        return;
    }

    size_t end = addr + length;
    if(end > s_partition_end) {
        length = s_partition_end - addr ;
        end = s_partition_end ;
    }

    size_t erase_start = (addr % FLASH_SECTOR_SIZE == 0) ? addr : ((addr / FLASH_SECTOR_SIZE + 1) * FLASH_SECTOR_SIZE);
    size_t erase_end = (end % FLASH_SECTOR_SIZE == 0) ? end : ((end / FLASH_SECTOR_SIZE + 1) * FLASH_SECTOR_SIZE);


    // 确保擦除范围不超出分区边界
    if(erase_end > s_partition_end) {
        erase_end = s_partition_end;
    }

    // 擦除 sector 并正确处理 read_offset
    if(erase_end > erase_start) {
        // dn2(erase_start,erase_end)
        esp_err_t res = esp_partition_erase_range(s_log_partition, erase_start, erase_end - erase_start);
    }

    esp_err_t res = esp_partition_write(s_log_partition, addr, buffer, length) ;

    // 到达了结束位置
    // dn3(addr,end,s_partition_end)
    if(end>=s_partition_end) {
        s_meta.write_offset = 0 ;
        s_meta.is_wrapped = true ;
    }

    else {
        s_meta.write_offset+= length ;
    }
}

// 日志写入任务
static void log_writer_task(void* params) {


    log_action_t action;

    while(1) {
        BaseType_t res = xQueueReceive(s_log_queue, (void *)&action, portMAX_DELAY);
        if( res!=pdTRUE ){
            vTaskDelay(100/portTICK_PERIOD_MS);
            continue;
        }
        

        switch(action.op) {
            case ACTION_WRITE: {

                size_t buffer_size = action.data_size ;

                size_t actual_start = s_data_addr + s_meta.write_offset;
                size_t actual_end = actual_start + buffer_size;
                uint8_t * data_ptr = action.data ;

                // 发生了回环写入, 先写入第一部分
                if(actual_end>s_partition_end) {
                    size_t first_length = s_partition_end - actual_start;
                    _write_to_flash(actual_start, data_ptr, first_length);
                    data_ptr+= first_length ;
                    actual_start = s_data_addr;
                    actual_end = s_data_addr + actual_end - s_partition_end;
                    buffer_size-= first_length;
                }

                // 写入剩余部分
                _write_to_flash(actual_start, data_ptr, buffer_size);

                meta_save() ;

                if(action.data) {
                    heap_caps_free(action.data); // 释放数据缓冲区
                    action.data = nullptr;
                    action.data_size = 0;
                }

                break ;

            }

            case ACTION_ERASE: {

                // 擦除指定的分区范围
                size_t erase_size = s_partition_end - s_partition_start;
                esp_partition_erase_range(s_log_partition, s_partition_start, erase_size);

                // 后续执行 ACTION_CLEAR , 此分支无 break

            }
            case ACTION_CLEAR: {
                s_meta.write_offset = 0;
                s_meta.is_wrapped = false;
            
                meta_save() ;
                break ;
            }
        }

    }
}


// 实现接口函数
extern "C" {

int log_setup(const char * name, size_t bufferSize, size_t start, size_t end, int core_id) {
    static bool setuped = false;
    if (setuped) {
        return LOG_SUCCESS;
    }
    setuped = true;

    if (!name) {
        return LOG_ERROR_INVALID_PARTITION_NAME;
    }
    
    // 简单的字符串长度检查
    size_t name_len = 0;
    while (name[name_len] != '\0' && name_len < 256) {
        name_len++;
    }
    
    if (name_len == 0) {
        return LOG_ERROR_INVALID_PARTITION_NAME;
    }

    // 查找分区
    s_log_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, 
        ESP_PARTITION_SUBTYPE_ANY,
        name);
    
    if (!s_log_partition) {
        return LOG_ERROR_PARTITION_NOT_FOUND;
    }

    if(end==0) {
        end = s_log_partition->size;
    }

    // 检查 start 是否按 sector 对齐
    if (start % FLASH_SECTOR_SIZE != 0) {
        return LOG_ERROR_INVALID_START;
    }
    // 检查 end 是否按 sector 对齐
    if (end % FLASH_SECTOR_SIZE != 0) {
        return LOG_ERROR_INVALID_END;
    }
        
    // 检查 start 和 end 的范围
    if (start >= s_log_partition->size || end > s_log_partition->size) {
        return LOG_ERROR_INVALID_PARAMETER;
    }

    // 检查 start < end
    if (start >= end) {
        return LOG_ERROR_INVALID_PARAMETER;
    }
        
    // 检查 end - start >= 2 * FLASH_SECTOR_SIZE (至少2个扇区)
    if ((end - start) < (2 * FLASH_SECTOR_SIZE)) {
        return LOG_ERROR_INVALID_PARAMETER;
    }
        
    // 设置分区使用范围
    s_partition_start = start;
    s_partition_end = end;

    // 检查 core_id 合法性
    if ((core_id < 0 || core_id >= SOC_CPU_CORES_NUM)) {
        return LOG_ERROR_INVALID_PARAMETER;
    }

    // 加载元数据
    if(!meta_load()) {
        s_meta = {
            .magic = LOG_META_MAGIC, 
            .write_offset = 0,
            .is_wrapped = false,
        };
    }
        // dn2(s_meta.is_wrapped, s_meta.write_offset)
    
    // 创建 ringbuffer
    s_log_queue = xQueueCreate(10, sizeof(log_action_t));
    if (!s_log_queue) {
        return LOG_ERROR_MEMORY_ALLOCATION;
    }
    
    // 创建日志写入任务，使用指定的核心
    BaseType_t task_result = xTaskCreatePinnedToCore(
        log_writer_task,
        "log_task",
        TASK_STACK,
        nullptr,
        TASK_PRIORITY,
        &s_log_task_handle,
        core_id  // 使用指定的核心
    );
    
    if (task_result != pdPASS) {
        vQueueDelete(s_log_queue);
        s_log_queue = nullptr;
        return LOG_ERROR_TASK_CREATION;
    }

    return LOG_SUCCESS;
}



size_t log_write(const void *buf, size_t count, bool timestampPrefix) {

    if (!s_log_partition || count == 0 || !s_log_queue) {
        return 0;
    }

    // 检查并移除可能的连续null终止符
    const uint8_t* data = (const uint8_t*)buf;
    size_t actual_count = count;
    
    while (actual_count > 0 && data[actual_count - 1] == 0) {
        actual_count--;
    }
    
    if (actual_count == 0) {
        __real_printf("log_write: No valid data to write.\n");
        return 0;
    }

    // 计算时间戳前缀长度
    size_t prefix_len = 0;
    if (timestampPrefix) {
        prefix_len = 22;  // "[YYYY-mm-dd HH:MM:SS] " 固定长度
    }

    // 分配合并的缓冲区
    size_t total_size = prefix_len + actual_count;
    uint8_t* combined_buffer = (uint8_t*)heap_caps_malloc(total_size, MALLOC_CAP_8BIT);
    if (!combined_buffer) {
        __real_printf("log_write: Memory allocation failed.\n");
        return 0;
    }

    // 直接在 combined_buffer 中构建时间戳前缀
    size_t buffer_offset = 0;
    if (timestampPrefix) {
        uint32_t tick_ms = esp_timer_get_time() / 1000;
        uint32_t seconds = tick_ms / 1000;
        uint32_t minutes = seconds / 60;
        uint32_t hours = minutes / 60;
        uint32_t days = hours / 24;
        uint32_t years = 1970 + days / 365;  // 简化计算，不考虑闰年
        uint32_t remaining_days = days % 365;
        uint32_t months = remaining_days / 30 + 1;  // 简化计算
        uint32_t day_of_month = remaining_days % 30 + 1;
        
        seconds %= 60;
        minutes %= 60;
        hours %= 24;
        
        // 直接在 combined_buffer 中构建时间戳字符串 "[YYYY-mm-dd HH:MM:SS] "
        combined_buffer[buffer_offset++] = '[';
        
        // 年份 YYYY
        combined_buffer[buffer_offset++] = '0' + (years / 1000);
        combined_buffer[buffer_offset++] = '0' + ((years / 100) % 10);
        combined_buffer[buffer_offset++] = '0' + ((years / 10) % 10);
        combined_buffer[buffer_offset++] = '0' + (years % 10);
        combined_buffer[buffer_offset++] = '-';
        
        // 月份 mm
        combined_buffer[buffer_offset++] = '0' + (months / 10);
        combined_buffer[buffer_offset++] = '0' + (months % 10);
        combined_buffer[buffer_offset++] = '-';
        
        // 日期 dd
        combined_buffer[buffer_offset++] = '0' + (day_of_month / 10);
        combined_buffer[buffer_offset++] = '0' + (day_of_month % 10);
        combined_buffer[buffer_offset++] = ' ';
        
        // 小时 HH
        combined_buffer[buffer_offset++] = '0' + (hours / 10);
        combined_buffer[buffer_offset++] = '0' + (hours % 10);
        combined_buffer[buffer_offset++] = ':';
        
        // 分钟 MM
        combined_buffer[buffer_offset++] = '0' + (minutes / 10);
        combined_buffer[buffer_offset++] = '0' + (minutes % 10);
        combined_buffer[buffer_offset++] = ':';
        
        // 秒 SS
        combined_buffer[buffer_offset++] = '0' + (seconds / 10);
        combined_buffer[buffer_offset++] = '0' + (seconds % 10);
        combined_buffer[buffer_offset++] = ']';
        combined_buffer[buffer_offset++] = ' ';
    }

    // 复制实际数据到缓冲区
    memcpy(combined_buffer + buffer_offset, data, actual_count);

    // 创建 log_action_t 并发送到队列
    log_action_t action = {
        .op = ACTION_WRITE,
        .data = combined_buffer,
        .data_size = total_size
    };

    BaseType_t res = xQueueSend(s_log_queue, (void*)&action, pdMS_TO_TICKS(1));
    if (res != pdTRUE) {
        heap_caps_free(combined_buffer);  // 发送失败时释放内存
        return 0;
    }
    
    return total_size;
}

size_t log_writef(const char *format, va_list args) {
    if (!s_log_partition || !format) {
        return 0;
    }

    size_t written = 0;
    
    char buf[1024];  // 使用1024字节缓冲区
    int len = vsnprintf(buf, sizeof(buf), format, args);
    
    return log_write(buf, len, true);
}

int log_flush() {
    return 1;
}


size_t log_length() {
    // dn2(s_meta.is_wrapped, s_meta.write_offset)
    if (!s_log_partition) {
        return 0;
    }

    if (s_meta.is_wrapped) {
        size_t total = s_partition_end - s_partition_start - FLASH_SECTOR_SIZE ;
        // 如果当前写入偏移是扇区对齐的，所在 sector 还没有 erase ，可算作有效数据
        if(s_meta.write_offset%FLASH_SECTOR_SIZE == 0) {
            return total ;
        }
        
        // 否则所在 sector 已经 erase ，write_offset 后的部分无效
        else {
            return total - FLASH_SECTOR_SIZE + (s_meta.write_offset%FLASH_SECTOR_SIZE) ;
        }
    } else {
        return s_meta.write_offset;
    }
}

size_t log_read(size_t start_pos, size_t read_length, uint8_t* buffer) {
    if (!s_log_partition || !buffer || read_length == 0) {
        return 0;
    }

    size_t readed = 0 ;
    size_t reading_addr_start, reading_addr_end ;

    if(s_meta.is_wrapped) {
        
        // 当前 sector 是有效的(尚未 erase)
        if(s_meta.write_offset% FLASH_SECTOR_SIZE == 0) {
            reading_addr_start = s_data_addr + s_meta.write_offset;
        }

        // 当前sector 无效(已经 erase)，从下一个 sector 开始读
        else {
            reading_addr_start = s_data_addr + (s_meta.write_offset/FLASH_SECTOR_SIZE+1)*FLASH_SECTOR_SIZE;
        }

        reading_addr_start+= start_pos ;

        if(reading_addr_start> s_partition_end) {
            reading_addr_start = s_data_addr + reading_addr_start - s_partition_end ;

            // 回环后依然超出 write_offset
            if(reading_addr_start - s_data_addr >= s_meta.write_offset) {
                return 0; // 超出当前写入偏移，无法读取
            }
        }

        reading_addr_end = reading_addr_start + read_length;
        if(reading_addr_end > s_partition_end) {
            reading_addr_end = s_data_addr + (reading_addr_end - s_partition_end);

            if( reading_addr_end - s_data_addr > s_meta.write_offset ) {
                reading_addr_end = s_data_addr + s_meta.write_offset;
            }
        }

        if(reading_addr_start==reading_addr_end) {
            return 0; // 没有数据可读
        }

        // 发生回环，分两次读
        if(reading_addr_end<reading_addr_start) {
            size_t first_length = s_partition_end - reading_addr_start;

            esp_partition_read(s_log_partition, reading_addr_start, buffer, first_length) ;

            buffer+= first_length;
            readed+= first_length;

            read_length-= first_length;
            if(read_length> s_meta.write_offset) {
                read_length = s_meta.write_offset;
            }
            
            reading_addr_start = s_data_addr;
            reading_addr_end = reading_addr_start + read_length ;
        }
    }

    else {
        if( start_pos >= s_meta.write_offset ) {
            return 0; // 超出当前写入偏移，无法读取
        }

        reading_addr_start = s_data_addr + start_pos ;

        if( start_pos+ read_length > s_meta.write_offset ) {
            read_length = s_meta.write_offset - start_pos ;
        }

        reading_addr_end = reading_addr_start + read_length;
    }

    if(read_length) {
        if( esp_partition_read(s_log_partition, reading_addr_start, buffer, read_length) == ESP_OK ) {
            readed+= read_length;
        }
    }
    
    return readed;
}

void log_clear() {
    if (!s_log_partition) {
        return;
    }
    log_action_t action = {
        .op = ACTION_CLEAR,
        .data = nullptr,
        .data_size = 0
    };
    xQueueSend(s_log_queue, (void*)&action, pdMS_TO_TICKS(1));
}

void log_erase() {
    if (!s_log_partition) {
        return;
    }
    log_action_t action = {
        .op = ACTION_ERASE,
        .data = nullptr,
        .data_size = 0
    };
    xQueueSend(s_log_queue, (void*)&action, pdMS_TO_TICKS(1));
}


void log_set_write_offset(size_t addr) {
    s_meta.write_offset = addr;
}
void log_set_reade_offset(size_t addr) {}

void log_fill(const char data) {
    if (!s_log_partition || !s_log_queue) {
        return;
    }

    // 用 data 填充整个 log 分区
    size_t data_start = s_data_addr;
    size_t data_size = s_partition_end - data_start;
    size_t filled_size = 0;
    uint8_t* fill_buffer = (uint8_t*)heap_caps_malloc(FLASH_SECTOR_SIZE, MALLOC_CAP_8BIT);
    if (!fill_buffer) {
        __real_printf("log_fill: Memory allocation failed.\n");
        return;
    }
    memset(fill_buffer, data, FLASH_SECTOR_SIZE);

    while (filled_size < data_size) {
        size_t to_write = FLASH_SECTOR_SIZE;
        if (filled_size + to_write > data_size) {
            to_write = data_size - filled_size;
        }
        
        esp_partition_erase_range(s_log_partition, data_start + filled_size, FLASH_SECTOR_SIZE);
        esp_err_t err = esp_partition_write(s_log_partition, data_start + filled_size, fill_buffer, to_write);
        if (err != ESP_OK) {
            __real_printf("log_fill: Write failed at offset %zu.\n", filled_size);
            heap_caps_free(fill_buffer);
            return;
        }
        
        filled_size += to_write;
    }

    heap_caps_free(fill_buffer);
}


} // extern "C"

#endif // ESP_PLATFORM
