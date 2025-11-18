#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include "config.h"

// 日志级别枚举
typedef enum {
    LOG_ERROR = 0,
    LOG_WARN = 1,
    LOG_INFO = 2,
    LOG_DEBUG = 3
} log_level_t;

// 日志函数声明
void log_message(log_level_t level, const char *format, ...);

#endif