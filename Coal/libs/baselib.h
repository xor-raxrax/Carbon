#pragma once
#include "../../Common/Luau/Lib.h"

int coal_getreg(lua_State* L);
int coal_getgenv(lua_State* L);
int coal_getrenv(lua_State* L);
int coal_getstateenv(lua_State* L);

int coal_identifyexecutor(lua_State* L);

int coal_getnamecallmethod(lua_State* L);
int coal_setnamecallmethod(lua_State* L);

int coal_setidentity(lua_State* L);
int coal_getidentity(lua_State* L);

int coal_setcapability(lua_State* L);
int coal_hascapability(lua_State* L);

int coal_checkcaller(lua_State* L);
int coal_isourthread(lua_State* L);
int coal_setourthread(lua_State* L);

int coal_getcallingscript(lua_State* L);
int coal_getinstances(lua_State* L);
int coal_getnilinstances(lua_State* L);

static const luaL_Reg baseLibrary[] = {
	{"getreg", coal_getreg},
	{"getgenv", coal_getgenv},
	{"getrenv", coal_getrenv},
	{"getstateenv", coal_getstateenv},

	{"identifyexecutor", coal_identifyexecutor},

	{"getnamecallmethod", coal_getnamecallmethod},
	{"setnamecallmethod", coal_setnamecallmethod},

	{"setidentity", coal_setidentity},
	{"getidentity", coal_getidentity},

	{"setcapability", coal_setcapability},
	{"hascapability", coal_hascapability},

	{"checkcaller", coal_checkcaller},
	{"isourthread", coal_isourthread},
	{"setourthread", coal_setourthread},

	{"getcallingscript", coal_getcallingscript},
	{"getinstances", coal_getinstances},
	{"getnilinstances", coal_getnilinstances},

	{nullptr, nullptr},
};