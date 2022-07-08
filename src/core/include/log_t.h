#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#include "platform_t.h"

//颜色宏定义
#define NONE "\033[m"
#define RED "\033[0;32;31m"
#define LIGHT_RED "\033[1;31m"
#define GREEN "\033[0;32;32m"
#define LIGHT_GREEN "\033[1;32m"
#define BLUE "\033[0;32;34m"
#define LIGHT_BLUE "\033[1;34m"
#define DARY_GRAY "\033[1;30m"
#define CYAN "\033[0;36m"
#define LIGHT_CYAN "\033[1;36m"
#define PURPLE "\033[0;35m"
#define LIGHT_PURPLE "\033[1;35m"
#define BROWN "\033[0;33m"
#define YELLOW "\033[1;33m"
#define LIGHT_GRAY "\033[0;37m"
#define WHITE "\033[1;37m"

typedef enum
{
    elog_trace,
    elog_debug,
    eLog_info,
    eLog_warning,
    eLog_error,
    eLog_fatal,
} enLogSeverityLevel;

const static char* const logErrorStrArray[] = {
    "TRACE ", "DEBUG ", "INFO ", "WARNING ", "ERROR ", "FATAL "};

const static char* const logErrorColorArray[] = {PURPLE, WHITE, CYAN, YELLOW, RED, LIGHT_RED};

typedef void (*logCustomPrintFunc)(enLogSeverityLevel eLevel, const char* szMessage);

frCore_API void setLogStderr(enLogSeverityLevel eLevel);

frCore_API void setLogCustomPrint(logCustomPrintFunc fn);

frCore_API void logPrint(enLogSeverityLevel eLevel, const char* szFileName, int32_t iLine,
                         const char* szFmt, ...);

#define Check(_value)                                      \
    if (!(_value)) {                                       \
        logPrint(eLog_fatal, __FILE__, __LINE__, #_value); \
    }

#define Log(_level, _text, ...) logPrint(_level, __FILE__, __LINE__, _text, ##__VA_ARGS__)

#ifdef _DEBUG
#    define DLog(_level, _text, ...) logPrint(_level, __FILE__, __LINE__, _text, ##__VA_ARGS__)
#else
#    define DLog(_level, _text, ...)
#endif
