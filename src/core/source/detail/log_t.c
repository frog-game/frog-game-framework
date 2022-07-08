#include "log_t.h"

#include <stdio.h>
#include <stdlib.h>

#include <stdarg.h>
#include <time.h>

#include <stdatomic.h>

#include "thread_t.h"
#include "utility_t.h"
#include "fs_t.h"

static mutex_tt           s_mutexfile;
static enLogSeverityLevel s_eLogLevel        = elog_trace;
static logCustomPrintFunc s_fnlogCustomPrint = NULL;
static int32_t            s_iCount           = 0;

static void initLogFile(void)
{
    mutex_init(&s_mutexfile);
    fs_mkdir("tmp");
}

void logDefaultPrint(enLogSeverityLevel eLevel, const char* szMessage)
{
    time_t SetTime;
    time(&SetTime);

    static once_flag_tt in_init_flag = ONCE_FLAG_INIT;
    callOnce(&in_init_flag, initLogFile);

    mutex_lock(&s_mutexfile);

    FILE* f = fopen("tmp/output.log", "ab");

    struct tm* pTm;
    pTm = localtime(&SetTime);
    pTm->tm_year += 1900;
    pTm->tm_mon += 1;


    fprintf(f,
            "[%s %d-%d-%d %d:%d:%d] id:[%d] %s\n",
            logErrorStrArray[eLevel],
            pTm->tm_year,
            pTm->tm_mon,
            pTm->tm_mday,
            pTm->tm_hour,
            pTm->tm_min,
            pTm->tm_sec,
            s_iCount++,
            szMessage);

    fclose(f);
    mutex_unlock(&s_mutexfile);

    if (eLevel == eLog_fatal) {
        abort();
    }
}

void setLogStderr(enLogSeverityLevel eLevel)
{
    s_eLogLevel = eLevel;
}

void setLogCustomPrint(logCustomPrintFunc fn)
{
    s_fnlogCustomPrint = fn;
}

#define def_MAX_ERROR_STR 256

void logPrint(enLogSeverityLevel eLevel, const char* szFileName, int32_t iLine, const char* szFmt,
              ...)
{
    if (eLevel < s_eLogLevel) {
        return;
    }

    char szErrstr[def_MAX_ERROR_STR];
    sprintf(szErrstr, "file:[%s] line:[%d] ", szFileName, iLine);
    size_t nLength = strlen(szErrstr);

    size_t  nMaxLength = def_MAX_ERROR_STR - nLength;
    va_list args;
    va_start(args, szFmt);
    size_t nOutBufferLength = vsnprintf(szErrstr + nLength, nMaxLength, szFmt, args);
    va_end(args);

    if (nOutBufferLength >= nMaxLength) {
        char* szErrBuffer = NULL;
        nMaxLength        = def_MAX_ERROR_STR;
        while (true) {
            nMaxLength *= 2;
            szErrBuffer = mem_malloc(nMaxLength);
            sprintf(szErrBuffer, "file:[%s] line:[%d] ", szFileName, iLine);
            va_start(args, szFmt);
            nOutBufferLength = vsnprintf(szErrBuffer + nLength, nMaxLength - nLength, szFmt, args);
            va_end(args);
            if (nOutBufferLength < nMaxLength - nLength) {
                break;
            }
        }

        if (s_fnlogCustomPrint != NULL) {
            s_fnlogCustomPrint(eLevel, szErrBuffer);
        }
        else {
            logDefaultPrint(eLevel, szErrBuffer);
        }

        if (szErrBuffer) {
            mem_free(szErrBuffer);
        }
    }
    else {
        if (s_fnlogCustomPrint != NULL) {
            s_fnlogCustomPrint(eLevel, szErrstr);
        }
        else {
            logDefaultPrint(eLevel, szErrstr);
        }
    }
}
