

#pragma once
extern "C" {

#include "platform_t.h"

// type
#include <stdint.h>

#if DEF_PLATFORM == DEF_PLATFORM_WINDOWS
#    ifdef def_dllimport
#        define Frog_API __declspec(dllimport)
#    else
#        define Frog_API __declspec(dllexport)
#    endif
#else
#    ifdef def_dllimport
#        define Frog_API extern
#    else
#        define Frog_API __attribute__((__visibility__("default")))
#    endif
#endif

struct lua_State;

Frog_API int32_t luaopen_lruntime_llockstep(struct lua_State* L);
}
