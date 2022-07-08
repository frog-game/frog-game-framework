

#include "internal/lloadCache_t.h"

#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include "log_t.h"
#include "rbtree_t.h"
#include "thread_t.h"
#include "utility_t.h"

typedef struct luaload_cache_s
{
    RB_ENTRY(luaload_cache_s)
    entry;
    char*  szName;
    char*  pCodeCache;
    size_t nCodeLength;
} luaload_cache_tt;

static int32_t lualoadCacheNodeCmp(struct luaload_cache_s* pNode1, struct luaload_cache_s* pNode2)
{
    int32_t iRet = strcmp(pNode1->szName, pNode2->szName);
    return (iRet < 0 ? -1 : iRet > 0 ? 1 : 0);
}

RB_HEAD(luaload_cache_tree_s, luaload_cache_s);
RB_GENERATE_STATIC(luaload_cache_tree_s, luaload_cache_s, entry, lualoadCacheNodeCmp)

typedef struct luaload_cache_tree_s luaload_cache_tree_tt;

typedef struct lualoadCache_s
{
    rwlock_tt             rwlock;
    atomic_bool           bActive;
    luaload_cache_tree_tt cacheTree;
} lualoadCache_tt;

static lualoadCache_tt s_lualoadCache;

void luaCache_init()
{
    atomic_init(&s_lualoadCache.bActive, true);
    RB_INIT(&s_lualoadCache.cacheTree);
    rwlock_init(&s_lualoadCache.rwlock);
}

void luaCache_clear()
{
    luaload_cache_tt* pCache;
    luaload_cache_tt* pIter;

    RB_FOREACH_SAFE(pCache, luaload_cache_tree_s, &s_lualoadCache.cacheTree, pIter)
    {
        RB_REMOVE(luaload_cache_tree_s, &s_lualoadCache.cacheTree, pCache);
        mem_free(pCache->szName);
        mem_free(pCache->pCodeCache);
        mem_free(pCache);
    }
    rwlock_destroy(&s_lualoadCache.rwlock);
}

int32_t writerCache(struct lua_State* L, const void* p, size_t sz, void* ud)
{
    luaload_cache_tt* pCache = (luaload_cache_tt*)ud;
    pCache->pCodeCache       = mem_realloc(pCache->pCodeCache, pCache->nCodeLength + sz);
    memcpy(pCache->pCodeCache + pCache->nCodeLength, p, sz);
    pCache->nCodeLength += sz;
    return 0;
}

void luaCache_on()
{
    atomic_store(&s_lualoadCache.bActive, true);
}

void luaCache_off()
{
    atomic_store(&s_lualoadCache.bActive, false);
}

bool luaCache_abandon(const char* szFileName)
{
    if (strcmp(szFileName, "all") == 0) {
        luaload_cache_tt* pCache;
        luaload_cache_tt* pIter;
        rwlock_wrlock(&s_lualoadCache.rwlock);
        RB_FOREACH_SAFE(pCache, luaload_cache_tree_s, &s_lualoadCache.cacheTree, pIter)
        {
            RB_REMOVE(luaload_cache_tree_s, &s_lualoadCache.cacheTree, pCache);
            mem_free(pCache->szName);
            mem_free(pCache->pCodeCache);
            mem_free(pCache);
        }
        rwlock_wrunlock(&s_lualoadCache.rwlock);
    }
    else {
        luaload_cache_tt dataNode;
        dataNode.szName          = (char*)szFileName;
        luaload_cache_tt* pCache = NULL;
        rwlock_wrlock(&s_lualoadCache.rwlock);
        pCache = RB_FIND(luaload_cache_tree_s, &s_lualoadCache.cacheTree, &dataNode);
        if (pCache == NULL) {
            rwlock_wrunlock(&s_lualoadCache.rwlock);
            return false;
        }

        RB_REMOVE(luaload_cache_tree_s, &s_lualoadCache.cacheTree, pCache);
        mem_free(pCache->szName);
        mem_free(pCache->pCodeCache);
        mem_free(pCache);
        rwlock_wrunlock(&s_lualoadCache.rwlock);
    }
    return true;
}

int32_t loadfileCache(struct lua_State* L, const char* szFileName)
{
    if (!atomic_load(&s_lualoadCache.bActive)) {
        return luaL_loadfile(L, szFileName);
    }

    luaload_cache_tt dataNode;
    dataNode.szName = (char*)szFileName;

    luaload_cache_tt* pNode = NULL;
    rwlock_rdlock(&s_lualoadCache.rwlock);
    pNode = RB_FIND(luaload_cache_tree_s, &s_lualoadCache.cacheTree, &dataNode);
    if (pNode != NULL) {
        Check(luaL_loadbuffer(L, pNode->pCodeCache, pNode->nCodeLength, szFileName) == LUA_OK);
        rwlock_rdunlock(&s_lualoadCache.rwlock);
        return LUA_OK;
    }
    rwlock_rdunlock(&s_lualoadCache.rwlock);

    rwlock_wrlock(&s_lualoadCache.rwlock);
    pNode = RB_FIND(luaload_cache_tree_s, &s_lualoadCache.cacheTree, &dataNode);
    if (pNode != NULL) {
        Check(luaL_loadbuffer(L, pNode->pCodeCache, pNode->nCodeLength, szFileName) == LUA_OK);
        rwlock_wrunlock(&s_lualoadCache.rwlock);
        return LUA_OK;
    }

    int32_t stat = luaL_loadfile(L, szFileName);
    if (stat == LUA_OK) {
        luaload_cache_tt* pCache = mem_malloc(sizeof(luaload_cache_tt));
        pCache->szName           = mem_strdup(szFileName);
        pCache->pCodeCache       = NULL;
        pCache->nCodeLength      = 0;
        Check(lua_dump(L, writerCache, pCache, 0) == 0);
        Check(RB_INSERT(luaload_cache_tree_s, &s_lualoadCache.cacheTree, pCache) == NULL);
    }
    rwlock_wrunlock(&s_lualoadCache.rwlock);
    return stat;
}

// lua src loadlib.c
//------------------------------------------------------------------------------------------
/*
** {======================================================
** 'require' function
** =======================================================
*/
#define LUA_PATH_SEP ";"

#if !defined(LUA_LSUBSEP)
#    define LUA_LSUBSEP LUA_DIRSEP
#endif

static int readable(const char* filename)
{
    FILE* f = fopen(filename, "r"); /* try to open file */
    if (f == NULL) return 0;        /* open failed */
    fclose(f);
    return 1;
}

/*
** Get the next name in '*path' = 'name1;name2;name3;...', changing
** the ending ';' to '\0' to create a zero-terminated string. Return
** NULL when list ends.
*/
static const char* getnextfilename(char** path, char* end)
{
    char* sep;
    char* name = *path;
    if (name == end)
        return NULL;           /* no more names */
    else if (*name == '\0') {  /* from previous iteration? */
        *name = *LUA_PATH_SEP; /* restore separator */
        name++;                /* skip it */
    }
    sep = strchr(name, *LUA_PATH_SEP); /* find next separator */
    if (sep == NULL)                   /* separator not found? */
        sep = end;                     /* name goes until the end */
    *sep  = '\0';                      /* finish file name */
    *path = sep;                       /* will start next search from here */
    return name;
}

/*
** Given a path such as ";blabla.so;blublu.so", pushes the string
**
** no file 'blabla.so'
**	no file 'blublu.so'
*/
static void pusherrornotfound(lua_State* L, const char* path)
{
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    luaL_addstring(&b, "no file '");
    luaL_addgsub(&b, path, LUA_PATH_SEP, "'\n\tno file '");
    luaL_addstring(&b, "'");
    luaL_pushresult(&b);
}

static const char* searchpath(lua_State* L, const char* name, const char* path, const char* sep,
                              const char* dirsep)
{
    luaL_Buffer buff;
    char*       pathname;    /* path with name inserted */
    char*       endpathname; /* its end */
    const char* filename;
    /* separator is non-empty and appears in 'name'? */
    if (*sep != '\0' && strchr(name, *sep) != NULL)
        name = luaL_gsub(L, name, sep, dirsep); /* replace it by 'dirsep' */
    luaL_buffinit(L, &buff);
    /* add path to the buffer, replacing marks ('?') with the file name */
    luaL_addgsub(&buff, path, LUA_PATH_MARK, name);
    luaL_addchar(&buff, '\0');
    pathname    = luaL_buffaddr(&buff); /* writable list of file names */
    endpathname = pathname + luaL_bufflen(&buff) - 1;
    while ((filename = getnextfilename(&pathname, endpathname)) != NULL) {
        if (readable(filename))                 /* does file exist and is readable? */
            return lua_pushstring(L, filename); /* save and return name */
    }
    luaL_pushresult(&buff);                    /* push path to create error message */
    pusherrornotfound(L, lua_tostring(L, -1)); /* create error message */
    return NULL;                               /* not found */
}

static const char* findfile(lua_State* L, const char* name, const char* pname, const char* dirsep)
{
    const char* path;
    lua_getfield(L, lua_upvalueindex(1), pname);
    path = lua_tostring(L, -1);
    if (path == NULL) luaL_error(L, "'package.%s' must be a string", pname);
    return searchpath(L, name, path, ".", dirsep);
}

static int checkload(lua_State* L, int stat, const char* filename)
{
    if (stat) {                      /* module loaded successfully? */
        lua_pushstring(L, filename); /* will be 2nd argument to module */
        return 2;                    /* return open function and file name */
    }
    else
        return luaL_error(L,
                          "error loading module11 '%s' from file '%s':\n\t%s",
                          lua_tostring(L, 1),
                          filename,
                          lua_tostring(L, -1));
}

//------------------------------------------------------------------------------------------

int32_t loadCache(struct lua_State* L)
{
    const char* szFileName;
    const char* szName = luaL_checkstring(L, 1);
    szFileName         = findfile(L, szName, "path", LUA_LSUBSEP);
    if (szFileName == NULL) {
        return 1;
    }

    if (!atomic_load(&s_lualoadCache.bActive)) {
        return checkload(L, luaL_loadfile(L, szFileName) == LUA_OK, szFileName);
    }

    luaload_cache_tt dataNode;
    dataNode.szName = (char*)szFileName;

    luaload_cache_tt* pNode = NULL;
    rwlock_rdlock(&s_lualoadCache.rwlock);
    pNode = RB_FIND(luaload_cache_tree_s, &s_lualoadCache.cacheTree, &dataNode);
    if (pNode != NULL) {
        int32_t stat = luaL_loadbuffer(L, pNode->pCodeCache, pNode->nCodeLength, szFileName);
        rwlock_rdunlock(&s_lualoadCache.rwlock);
        return checkload(L, stat == LUA_OK, szFileName);
    }
    rwlock_rdunlock(&s_lualoadCache.rwlock);

    rwlock_wrlock(&s_lualoadCache.rwlock);
    pNode = RB_FIND(luaload_cache_tree_s, &s_lualoadCache.cacheTree, &dataNode);
    if (pNode != NULL) {
        int32_t stat = luaL_loadbuffer(L, pNode->pCodeCache, pNode->nCodeLength, szFileName);
        rwlock_wrunlock(&s_lualoadCache.rwlock);
        return checkload(L, stat == LUA_OK, szFileName);
    }

    int32_t stat = luaL_loadfile(L, szFileName);
    if (stat == LUA_OK) {
        luaload_cache_tt* pCache = mem_malloc(sizeof(luaload_cache_tt));
        pCache->szName           = mem_strdup(szFileName);
        pCache->pCodeCache       = NULL;
        pCache->nCodeLength      = 0;
        Check(lua_dump(L, writerCache, pCache, 0) == 0);
        Check(RB_INSERT(luaload_cache_tree_s, &s_lualoadCache.cacheTree, pCache) == NULL);
    }
    rwlock_wrunlock(&s_lualoadCache.rwlock);
    return checkload(L, stat == LUA_OK, szFileName);
}