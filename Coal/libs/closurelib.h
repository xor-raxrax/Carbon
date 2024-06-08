#pragma once
#include "../../Common/Luau/Lib.h"

CallInfo* luaD_growCI_hook(lua_State* L);

int coal_iscclosure(lua_State* L);
int coal_islclosure(lua_State* L);
int coal_newcclosure(lua_State* L);
int coal_hookfunction(lua_State* L);
int coal_hookmetamethod(lua_State* L);

int coal_setourclosure(lua_State* L);
int coal_isourclosure(lua_State* L);

int coal_setsafeenv(lua_State* L);

int coal_getstack(lua_State* L);
int coal_setstack(lua_State* L);

int coal_getmaxstacksize(lua_State* L);
int coal_getstacksize(lua_State* L);

int coal_getupvalues(lua_State* L);
int coal_getupvalue(lua_State* L);
int coal_setupvalue(lua_State* L);

int coal_getconstants(lua_State* L);
int coal_getconstant(lua_State* L);
int coal_setconstant(lua_State* L);

int coal_getprotoinfo(lua_State* L);
int coal_getprotos(lua_State* L);
int coal_getproto(lua_State* L);
int coal_setproto(lua_State* L);
int coal_getinfo(lua_State* L);

static const luaL_Reg closureDebugLibrary[] = {
	{"getstack", coal_getstack},
	{"setstack", coal_setstack},

	{"getmaxstacksize", coal_getmaxstacksize},
	{"getstacksize", coal_getstacksize},

	{"getupvalue", coal_getupvalue},
	{"setupvalue", coal_setupvalue},
	{"getupvalues", coal_getupvalues},

	{"getconstants", coal_getconstants},
	{"getconstant", coal_getconstant},
	{"setconstant", coal_setconstant},

	{"getprotos", coal_getprotos},
	{"getproto", coal_getproto},

	{"getinfo", coal_getinfo},
	{"getprotoinfo", coal_getprotoinfo},

	{nullptr, nullptr},
};

static const luaL_Reg closureLibrary[] = {
	{"setsafeenv", coal_setsafeenv},

	{"iscclosure", coal_iscclosure},
	{"islclosure", coal_islclosure},
	{"setourclosure", coal_setourclosure},
	{"isourclosure", coal_isourclosure},
	{"newcclosure", coal_newcclosure},
	{"hookfunction", coal_hookfunction},
	{"hookmetamethod", coal_hookmetamethod},

	{nullptr, nullptr},
};