#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>
#include "logging.h"
#include "config.h"

void log_message(log_level_t level, const char *format, ...) {
#if LOG_ENABLED
    if (level > LOG_LEVEL) return;
    
    FILE *log_file = fopen(LOG_FILE, "a");
    if (!log_file) return;
    
    // 获取当前时间
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm);
    
    // 日志级别字符串
    const char *level_str;
    switch (level) {
        case LOG_ERROR: level_str = "ERROR"; break;
        case LOG_WARN: level_str = "WARN"; break;
        case LOG_INFO: level_str = "INFO"; break;
        case LOG_DEBUG: level_str = "DEBUG"; break;
        default: level_str = "UNKNOWN"; break;
    }
    
    // 写入日志头
    fprintf(log_file, "[%s] [%s] [PID:%d] ", timestamp, level_str, getpid());
    
    // 写入日志内容
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    
    // 换行并关闭文件
    fprintf(log_file, "\n");
    fclose(log_file);
#endif
}