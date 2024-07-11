module LuaEnv;

import <filesystem>;

import Luau;
import libs.fslib;
import libs.tablelib;
import libs.dbglib;
import libs.baselib;
import libs.closurelib;
import libs.gclib;

import Luau.Riblix;
import Luau.Compile;
import RiblixStructures;
import Console;
import LuauOffsets;

void luaL_register(lua_State* L, const luaL_Reg* l)
{
	for (; l->name; l++)
	{
		lua_pushcclosure(L, l->func, l->name);
		lua_setfield(L, -2, l->name);
	}
}

// keeps copy on stack
void registerLibCopy(lua_State* L, const char* name, Table* from)
{
	lua_createtable(L, 0, 0);
	lua_pushrawtable(L, from);
	lua_getfield(L, -1, name);

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

Table* getrenv(lua_State* L)
{
	if (!L->gt->metatable)
		Console::getInstance() << "cannot get renv from thread (crashing now XDD)" << std::endl;

	lua_pushrawtable(L, L->gt->metatable);
	lua_pushstring(L, "__index");
	lua_rawget(L, -2);
	auto result = hvalue(luaA_toobject(L, -1));
	lua_pop(L, 1);
	return result;
}

void createRedirectionProxy(lua_State* L, Table* main, Table* to)
{
	lua_createtable(L);
	auto proxy = lua_totable(L, -1);

	lua_pushrawtable(L, to);
	lua_setfield(L, -2, "__index");

	lua_pushstring(L, "The metatable is locked");
	lua_setfield(L, -2, "__metatable");
	main->metatable = proxy;
	lua_pop(L, 1);
}

void LuaApiRuntimeState::injectEnvironment(lua_State* from)
{
	lua_State* L = from;
	auto renv = getrenv(from);
	L = lua_newthread(from);
	mainThread = L;

	lua_pushlightuserdatatagged(L, L);
	lua_pushrawthread(L, L);
	lua_rawset(L, LUA_REGISTRYINDEX);

	{
		lua_createtable(L);
		auto genv = lua_totable(L, -1);

		createRedirectionProxy(L, genv, renv);
		registerEnvGetters(L, genv, renv);

		lua_createtable(L);
		lua_setfield(L, -2, "_G");

		lua_createtable(L);
		lua_setfield(L, -2, "shared");

		luaL_register(L, baseLibrary);
		luaL_register(L, filesystemLibrary);
		luaL_register(L, tableLibrary);
		luaL_register(L, debug_library);
		luaL_register(L, closureLibrary);
		luaL_register(L, closureDebugLibrary);
		luaL_register(L, gcLibrary);

		// debug
		{
			registerLibCopy(L, "debug", renv);

			luaL_register(L, closureDebugLibrary);
			if (luaApiRuntimeState.getLuaSettings().allow_setproto)
				registerFunction(L, carbon_setproto, "setproto", true);

			lua_totable(L, -1)->readonly = true;
			lua_pop(L, 1);
		}

		lua_pop(L, 1);
	}
};

void LuaApiRuntimeState::runScript(const std::string& source) const
{
	auto L = lua_newthread(mainThread);

	lua_createtable(L);
	auto env = lua_totable(L, -1);
	auto genv = L->gt;
	lua_pop(L, 1);

	createRedirectionProxy(L, env, genv);
	L->gt = env;

	if (compile(L, source.c_str(), "carbon"))
	{
		// stack: closure
		try
		{
			lua_call(L, 0, 0);
		}
		catch (const lua_exception& e)
		{
			Console::getInstance() << e.what() << std::endl;
			lua_getglobal(L, "warn");
			lua_pushstring(L, e.what());
			lua_pcall(L, 1, 0, 0);
		}
		catch (const std::exception& e)
		{
			Console::getInstance() << e.what() << std::endl;
			lua_getglobal(L, "warn");
			lua_pushstring(L, e.what());
			lua_pcall(L, 1, 0, 0);
		}
	}
	else
	{
		// stack: error
		lua_getglobal(L, "warn");
		lua_insert(L, -2);
		lua_pcall(L, 1, 0, 0);
	}
}

bool LuaApiRuntimeState::compile(lua_State* L, const char* source, const char* chunkname) const
{
	auto bytecode = ::compile(std::string(source));

	bool didLoad = !riblixAddresses.luau_load(L, chunkname, bytecode.c_str(), bytecode.size(), 0);
	return didLoad;
}