
#ifndef LOGGER_IMP_HPP
#define LOGGER_IMP_HPP

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

// 返回码定义
#define LOG_SUCCESS 0
#define LOG_ERROR_INVALID_PARTITION_NAME -1
#define LOG_ERROR_PARTITION_NOT_FOUND -2
#define LOG_ERROR_MEMORY_ALLOCATION -3
#define LOG_ERROR_TASK_CREATION -4
#define LOG_ERROR_INVALID_PARAMETER -5
#define LOG_ERROR_WRITE_FAILED -6
#define LOG_ERROR_INVALID_START -7
#define LOG_ERROR_INVALID_END -8

// C 接口函数声明
#ifdef __cplusplus
extern "C" {
#endif

int log_setup(const char * name, size_t bufferSize=1024, size_t start=0, size_t end=0, int core_id=1);
int log_flush();
size_t log_write(const void *buf, size_t length, bool timestampPrefix=true);
size_t log_writef(const char *format, va_list args);
void log_erase();
void log_clear();
size_t log_length();
size_t log_read(size_t start_pos, size_t read_length, uint8_t* buffer);

void log_set_write_offset(size_t addr);
void log_set_reade_offset(size_t addr);
void log_fill(const char data);


#ifdef __cplusplus
}
#endif

#endif // LOGGER_IMP_HPP
