#pragma once
#include "../../Common/Luau/Lib.h"

int coal_disablepointerencoding(lua_State* L);
int coal_getdescriptors(lua_State* L);
int coal_getdescriptorinfo(lua_State* L);
int coal_getscriptcontext(lua_State* L);
int coal_getcfunction(lua_State* L);
int coal_repush(lua_State* L);
int coal_gettt(lua_State* L);
int coal_getgcaddr(lua_State* L);
int coal_torva(lua_State* L);
int coal_getcontext(lua_State* L);

int coal_getinstancebrigdemap(lua_State* L);

int coal_dumpstacks(lua_State* L);

static const luaL_Reg debug_library[] = {
	{"disablepointerencoding", coal_disablepointerencoding},
	{"getdescriptors", coal_getdescriptors},
	{"getdescriptorinfo", coal_getdescriptorinfo},
	{"dumpstacks", coal_dumpstacks},
	{"torva", coal_torva},
	{"getcontext", coal_getcontext},

	{"getscriptcontext", coal_getscriptcontext},
	{"getcfunction", coal_getcfunction},
	{"getgcaddr", coal_getgcaddr},
	{"repush", coal_repush},
	{"gettt", coal_gettt},

	{"getinstancebrigdemap", coal_getinstancebrigdemap},

	{nullptr, nullptr},
};