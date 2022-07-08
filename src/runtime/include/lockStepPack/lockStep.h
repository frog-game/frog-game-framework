

#pragma once
extern "C" {

#include "platform_t.h"

// type
#include <stdint.h>

#if DEF_PLATFORM == DEF_PLATFORM_WINDOWS
#    ifdef def_dllimport
#        define LOCKSTEPPACK_API __declspec(dllimport)
#    else
#        define LOCKSTEPPACK_API __declspec(dllexport)
#    endif
#else
#    ifdef def_dllimport
#        define LOCKSTEPPACK_API extern
#    else
#        define LOCKSTEPPACK_API __attribute__((__visibility__("default")))
#    endif
#endif

struct lua_State;

LOCKSTEPPACK_API int32_t luaopen_lruntime_llockStep(struct lua_State* L);
}
