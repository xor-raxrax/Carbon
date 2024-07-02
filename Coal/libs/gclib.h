#pragma once
#include "../../Common/Luau/Lib.h"

int coal_getgc(lua_State* L);
int coal_filtergc(lua_State* L);

static const luaL_Reg gcLibrary[] = {
	{"getgc", coal_getgc},
	{"filtergc", coal_filtergc},

	{nullptr, nullptr},
};