import LuaEnv;

import <filesystem>;

import Luau;
import libs.fslib;
import libs.tablelib;
import libs.dbglib;
import libs.baselib;
import libs.closurelib;
import libs.gclib;

void luaL_register(lua_State* L, const luaL_Reg* l)
{
	for (; l->name; l++)
	{
		lua_pushcclosure(L, l->func, l->name);
		lua_setfield(L, -2, l->name);
	}
}

// keeps copy on stack
void registerLibCopy(lua_State* L, const char* name)
{
	lua_createtable(L, 0, 0);
	lua_getglobal(L, name);

	int index = 0;
	lua_pushnil(L); // Stack: result, lib, nil
	while (lua_next(L, -2)) // Stack: result, lib, k, v
	{
		lua_pushvalue(L, -2); // Stack: result, lib, k, v, k
		lua_pushvalue(L, -2); // Stack: result, lib, k, v, k, v
		lua_settable(L, -6); // Stack: result, lib, k, v
		lua_pop(L, 1); // Stack: result, lib, k
	}

	lua_pop(L, 1); // Stack: result
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, name);
}

void registerFunction(lua_State* L, lua_CFunction function, const char* name, bool addAlsoAsGlobal = false)
{
	lua_pushcclosure(L, function, name);
	lua_setfield(L, -2, name);

	if (addAlsoAsGlobal)
	{
		lua_pushcclosure(L, function, name);
		lua_setglobal(L, name);
	}
}

void LuaApiRuntimeState::injectEnvironment(lua_State* L)
{
	mainEnv = L->gt;

	lua_createtable(L, 0, 0);
	lua_setglobal(L, "_G");

	lua_createtable(L, 0, 0);
	lua_setglobal(L, "shared");

	lua_pushvalue(L, LUA_GLOBALSINDEX);

	luaL_register(L, baseLibrary);
	luaL_register(L, filesystemLibrary);
	luaL_register(L, tableLibrary);
	luaL_register(L, debug_library);
	luaL_register(L, closureLibrary);
	luaL_register(L, closureDebugLibrary);
	luaL_register(L, gcLibrary);

	// debug
	{
		registerLibCopy(L, "debug");

		luaL_register(L, closureDebugLibrary);
		if (luaApiRuntimeState.getLuaSettings().allow_setproto)
			registerFunction(L, coal_setproto, "setproto", true);

		lua_totable(L, -1)->readonly = true;
		lua_pop(L, 1);
	}

	lua_pop(L, 1); // globals
};