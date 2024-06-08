#include "../../Common/Luau/Luau.h"
#include "tablelib.h"

int coal_getrawmetatable(lua_State* L)
{
	luaL_checkany(L, 1);
	if (!lua_getmetatable(L, 1))
		lua_pushnil(L);
	return 1;
}

int coal_setrawmetatable(lua_State* L)
{
	int mtype = lua_type(L, 1);
	if (mtype != LUA_TNIL && mtype != LUA_TTABLE)
		luaL_typeerrorL(L, 1, "nil | table");

	lua_pop(L, 1);
	lua_setmetatable(L, 1);
	return 0;
}

int coal_setreadonly(lua_State* L)
{
	luaL_checktable(L, 1)->readonly = luaL_checkboolean(L, 2);
	return 0;
}

int coal_make_writeable(lua_State* L)
{
	luaL_checktable(L, 1)->readonly = false;
	return 0;
}

int coal_make_readonly(lua_State* L)
{
	luaL_checktable(L, 1)->readonly = true;
	return 0;
}

int coal_isreadonly(lua_State* L)
{
	lua_pushboolean(L, luaL_checktable(L, 1)->readonly);
	return 1;
}