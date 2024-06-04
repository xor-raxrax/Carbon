#pragma once
#include "../../Common/Luau/Lib.h"

int coal_getrawmetatable(lua_State* L);
int coal_setrawmetatable(lua_State* L);

int coal_setreadonly(lua_State* L);
int coal_make_writeable(lua_State* L);
int coal_make_readonly(lua_State* L);
int coal_isreadonly(lua_State* L);

inline const luaL_Reg tableLibrary[] = {
	{"getrawmetatable", coal_getrawmetatable},
	{"setrawmetatable", coal_setrawmetatable},

	{"setreadonly", coal_setreadonly},
	{"make_writeable", coal_make_writeable},
	{"make_readonly", coal_make_readonly},
	{"isreadonly", coal_isreadonly},

	{nullptr, nullptr},
};
