#pragma once

#include <stdbool.h>
#include <stdint.h>


#include "platform_t.h"

frCore_API bool fs_mkdir(const char* szPath);

frCore_API bool fs_find(const char* szFile);

frCore_API bool fs_remove(const char* szFile);

frCore_API bool fs_resetName(const char* szSrcFile, const char* szDstFile);

frCore_API bool fs_removePath(const char* szPath);

frCore_API bool fs_removeAll(const char* szPath);
