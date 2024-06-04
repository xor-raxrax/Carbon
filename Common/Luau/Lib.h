#pragma once
struct lua_State;

typedef int (*lua_CFunction)(lua_State* L);

struct luaL_Reg
{
	const char* name;
	lua_CFunction func;
};