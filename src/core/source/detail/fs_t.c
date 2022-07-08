#include "fs_t.h"

#if defined(_WINDOWS) || defined(_WIN32)
#    include <direct.h>
#    include <io.h>
#else
#    include <sys/types.h>
#    include <sys/stat.h>
#    include <unistd.h>
#    include <dirent.h>
#    define gp_stat stat
#    define gp_stat_struct struct stat
#endif

#include <stdio.h>
#include <string.h>

#include "utility_t.h"

#ifndef MAX_PATH
#    define MAX_PATH 260
#endif

bool fs_mkdir(const char* szPath)
{
#if defined(_WINDOWS) || defined(_WIN32)
    return _mkdir(szPath) != 0;
#else
    return mkdir(szPath, 0777) != 0;
#endif
}

bool fs_find(const char* szFile)
{
#if defined(_WINDOWS) || defined(_WIN32)
    return _access(szFile, 0) != -1;
#else
    return access(szFile, 0) != -1;
#endif
}

bool fs_remove(const char* szFile)
{
    return remove(szFile);
}

bool fs_resetName(const char* szSrcFile, const char* szDstFile)
{
    return rename(szSrcFile, szDstFile) != -1;
}

bool fs_removePath(const char* szPath)
{
#if defined(_WINDOWS) || defined(_WIN32)
    return _rmdir(szPath) != 0;
#else
    return rmdir(szPath) != 0;
#endif
}

bool fs_removeAll(const char* szPath)
{
#if defined(_WINDOWS) || defined(_WIN32)
    char szPathName[MAX_PATH];
    strcpy(szPathName, szPath);
    strcat(szPathName, "\\*");

    intptr_t           hFile;
    struct _finddata_t fileinfo;
    hFile = _findfirst(szPathName, &fileinfo);
    if (hFile == -1) {
        return false;
    }
    do {
        if ((fileinfo.attrib & _A_SUBDIR)) {
            if (strcmp(fileinfo.name, ".") != 0 && strcmp(fileinfo.name, "..") != 0) {
                bzero(szPathName, 256);
                strcpy(szPathName, szPath);
                strcat(szPathName, "\\");
                strcat(szPathName, fileinfo.name);
                fs_removeAll(szPathName);
            }
        }
        else {
            bzero(szPathName, 256);
            strcpy(szPathName, szPath);
            strcat(szPathName, "\\");
            strcat(szPathName, fileinfo.name);
            fs_remove(szPathName);
        }
    } while (_findnext(hFile, &fileinfo) == 0);
    _findclose(hFile);
    fs_removePath(szPath);
    return true;
#else
    DIR*           dp = NULL;
    struct dirent* dirp;
    dp = opendir(szPath);
    if (dp == NULL) {
        return false;
    }

    char szPathName[MAX_PATH];

    while ((dirp = readdir(dp)) != NULL) {
        if (strcmp(dirp->d_name, "..") == 0 || strcmp(dirp->d_name, ".") == 0) continue;
        bzero(szPathName, MAX_PATH);
        strcpy(szPathName, szPath);
        strcat(szPathName, "/");
        strcat(szPathName, dirp->d_name);
        if (dirp->d_type == DT_DIR) {
            fs_removeAll(szPathName);
        }
        else {
            fs_remove(szPathName);
        }
    }
    fs_removePath(szPath);
    closedir(dp);
    dirp = NULL;
    return true;
#endif
}
