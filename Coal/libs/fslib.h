#pragma once
#include "../../Common/Luau/Lib.h"

int coal_readfile(lua_State* L);
int coal_loadfile(lua_State* L);
int coal_writefile(lua_State* L);

int coal_isfile(lua_State* L);
int coal_isfolder(lua_State* L);
int coal_listfiles(lua_State* L);
int coal_makefolder(lua_State* L);

int coal_delfolder(lua_State* L);
int coal_delfile(lua_State* L);
int coal_appendfile(lua_State* L);

inline const luaL_Reg filesystemLibrary[] = {
	{"readfile", coal_readfile},
	{"loadfile", coal_loadfile},
	{"writefile", coal_writefile},
	{"isfile", coal_isfile},
	{"isfolder", coal_isfolder},
	{"listfiles", coal_listfiles},
	{"makefolder", coal_makefolder},
	{"delfolder", coal_delfolder},
	{"delfile", coal_delfile},
	{"appendfile", coal_appendfile},

	{nullptr, nullptr},
};