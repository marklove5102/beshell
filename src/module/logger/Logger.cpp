#include "./Logger.hpp"
#include "logger-imp.hpp"
#include "../basic/Console.hpp"
#include "../telnet/TelnetChannel.hpp"
#include "../telnet/Protocal.hpp"
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

using namespace std;

#ifdef ESP_PLATFORM
// ESP32 中定义 ssize_t
#ifndef _SSIZE_T_DEFINED
typedef int ssize_t;
#define _SSIZE_T_DEFINED
#endif
#endif

// 线程本地变量，防止重复调用
static thread_local bool s_in_log_write = false;

static bool s_log_capture_enabled = false;

extern "C" {

#ifdef CONFIG_BESHELL_LOGGER_ENABLE_WRAP

    int __real_printf(const char *format, ...);
    int __real_vprintf(const char *format, va_list args);
    ssize_t __real_write(int fd, const void *buf, size_t count);
    
    // 实现 __wrap_write
    ssize_t __wrap_write(int fd, const void *buf, size_t count) {
        // 如果有可用的日志系统，写入日志
        if (s_log_capture_enabled && buf && count > 0 && !s_in_log_write && fd==6) {
            // static const char tmp[32] ;
            // int n = sprintf(tmp,"[%d]write:", fd) ;
            // log_write(tmp,n);
            log_write(buf, count);
        }
        
        // 调用原始 write 函数
        return __real_write(fd, buf, count);
    }

    int __wrap_printf(const char *format, ...) {
        va_list args;
        va_start(args, format);

        // 如果有可用的日志系统，写入日志
        if (s_log_capture_enabled && format && !s_in_log_write) {
            
            va_list args_copy;
            va_copy(args_copy, args);

            log_writef(format, args_copy);
            
            va_end(args_copy);
        }
        
        // 调用原始 vprintf 函数
        s_in_log_write = true;
        int ret = __real_vprintf(format, args);
        s_in_log_write = false;

        va_end(args);
        
        return ret;
    }
    
    int __wrap_vprintf(const char *format, va_list args) {
        // 如果有可用的日志系统，写入日志
        if (s_log_capture_enabled && format && !s_in_log_write) {
            
            va_list args_copy;
            va_copy(args_copy, args);
            
            // log_write("vpf:", 4);
            log_writef(format, args_copy);
            
            va_end(args_copy);
        }
        
        s_in_log_write = true;
        int ret = __real_vprintf(format, args);
        s_in_log_write = false;

        return ret;
    }

#endif

}

namespace be {

    class TelnetChannelLogCapture : public TelnetChannel {
    public:
        using TelnetChannel::TelnetChannel;  // 继承基类构造函数

        // 实现纯虚函数 sendData，这是数据截获的关键点
        virtual void sendData(const char* data, size_t datalen) override {
            if (data && datalen > 0) {
                log_write(data, datalen);
            }
        }

        // 重写 send Package 方法
        virtual void send(Package& pkg) override {
            if (pkg.head.fields.cmd != EXCEPTION && pkg.head.fields.cmd != OUTPUT) {
                return;
            }
            size_t dataLen = pkg.chunk_len > 0 ? pkg.chunk_len : pkg.body_len;
            log_write(pkg.body(), dataLen);
        }
    };

    int Logger::setup(BeShell & beshell, const char * partitionName, size_t bufferSize, size_t start, size_t end, int core_id) {
        static bool setuped = false;
        if (setuped) {
            return Logger::SUCCESS;
        }
        setuped = true;

        // 添加日志捕获通道
        beshell.telnet->addChannel(new TelnetChannelLogCapture(beshell.telnet));

        // 调用底层实现
        int result = log_setup(partitionName, bufferSize, start, end, core_id);
        
        // 转换错误码
        switch (result) {
            case LOG_SUCCESS:
                s_log_capture_enabled = true;
                return Logger::SUCCESS;
            case LOG_ERROR_INVALID_PARTITION_NAME:
                return Logger::ERROR_INVALID_PARTITION_NAME;
            case LOG_ERROR_PARTITION_NOT_FOUND:
                return Logger::ERROR_PARTITION_NOT_FOUND;
            case LOG_ERROR_MEMORY_ALLOCATION:
                return Logger::ERROR_MEMORY_ALLOCATION_FAILED;
            case LOG_ERROR_TASK_CREATION:
                return Logger::ERROR_MEMORY_ALLOCATION_FAILED; // 使用已有的错误码
            default:
                return Logger::ERROR_PARTITION_NOT_FOUND; // 使用已有的错误码作为默认
        }
    }

    int Logger::write(const void *buf, size_t count, bool timestampPrefix) {
        return (int)log_write(buf, count, timestampPrefix);
    }

    int Logger::write(const void *format, ...) {
        if (!format) {
            return 0;
        }
        
        // 将 void* 转换为 const char* 用于格式化
        const char* fmt_str = (const char*)format;
        
        va_list args;
        va_start(args, format);
        
        char buf[512];
        int len = vsnprintf(buf, sizeof(buf), fmt_str, args);
        
        va_end(args);
        
        if (len > 0 && len < (int)sizeof(buf)) {
            return write(buf, len);
        } else if (len >= (int)sizeof(buf)) {
            // 缓冲区太小，分配更大的缓冲区
            char* large_buf = (char*)malloc(len + 1);
            if (large_buf) {
                va_start(args, format);
                int new_len = vsnprintf(large_buf, len + 1, fmt_str, args);
                va_end(args);
                
                if (new_len > 0) {
                    int result = write(large_buf, new_len);
                    free(large_buf);
                    return result;
                }
                free(large_buf);
            }
        }
        
        return 0;
    }

    bool Logger::isCapturing() {
        return s_log_capture_enabled ;
    }

    void Logger::pause() {
        s_log_capture_enabled = false ;
    }

    void Logger::resume() {
        s_log_capture_enabled = true ;
    }

    int Logger::flush() {
        return log_flush();
    }

    size_t Logger::length() {
        return log_length();
    }

    size_t Logger::read(size_t start_pos, size_t read_length, uint8_t* buffer) {
        return log_read(start_pos, read_length, buffer);
    }

    int Logger::clear() {
        log_clear();
        return Logger::SUCCESS;
    }

    void Logger::erase() {
        log_erase();
    }
}