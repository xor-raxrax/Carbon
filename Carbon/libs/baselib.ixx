export module libs.baselib;

import Luau;
import Luau.Riblix;
import RiblixStructures;
import LuaEnv;
import StringUtils;

int carbon_getreg(lua_State* L);
int carbon_getstateenv(lua_State* L);

int carbon_identifyexecutor(lua_State* L);

int carbon_getnamecallmethod(lua_State* L);
int carbon_setnamecallmethod(lua_State* L);

int carbon_setidentity(lua_State* L);
int carbon_getidentity(lua_State* L);

int carbon_setcapability(lua_State* L);
int carbon_hascapability(lua_State* L);

int carbon_checkcaller(lua_State* L);
int carbon_isourthread(lua_State* L);
int carbon_setourthread(lua_State* L);

int carbon_getcallingscript(lua_State* L);

int carbon_getinstances(lua_State* L);
int carbon_getnilinstances(lua_State* L);

int carbon_cacheinvalidate(lua_State* L);
int carbon_cachereplace(lua_State* L);
int carbon_iscached(lua_State* L);
int carbon_cloneref(lua_State* L);

int carbon_loadstring(lua_State* L);

export const luaL_Reg baseLibrary[] = {
	{"getreg", carbon_getreg},
	{"getstateenv", carbon_getstateenv},

	{"identifyexecutor", carbon_identifyexecutor},

	{"getnamecallmethod", carbon_getnamecallmethod},
	{"setnamecallmethod", carbon_setnamecallmethod},

	{"setidentity", carbon_setidentity},
	{"getidentity", carbon_getidentity},

	{"setcapability", carbon_setcapability},
	{"hascapability", carbon_hascapability},

	{"checkcaller", carbon_checkcaller},
	{"isourthread", carbon_isourthread},
	{"setourthread", carbon_setourthread},

	{"getcallingscript", carbon_getcallingscript},

	{"getinstances", carbon_getinstances},
	{"getnilinstances", carbon_getnilinstances},

	{"cacheinvalidate", carbon_cacheinvalidate},
	{"cachereplace", carbon_cachereplace},
	{"iscached", carbon_iscached},
	{"cloneref", carbon_cloneref},

	{"load", carbon_loadstring},

	{nullptr, nullptr},
};

int carbon_getreg(lua_State* L)
{
	lua_pushvalue(L, LUA_REGISTRYINDEX);
	return 1;
}


int carbon_identifyexecutor(lua_State* L)
{
	lua_pushstring(L, "coal");
	lua_pushstring(L, "mines");
	return 2;
}

int carbon_getnamecallmethod(lua_State* L)
{
	if (L->namecall)
		lua_pushrawstring(L, L->namecall);
	else
		lua_pushnil(L);
	return 1;
}

int carbon_setnamecallmethod(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TSTRING);
	L->namecall = &index2addr(L, 1)->value.gc->ts;
	return 0;
}

int carbon_getstateenv(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTHREAD);
	auto state = thvalue(index2addr(L, 1));

	if (luaApiRuntimeState.getLuaSettings().getstateenv_returns_ref)
		lua_pushrawtable(L, state->gt);
	else
		lua_pushrawtable(L, luaH_clone(L, state->gt));

	return 1;
}



const char* getCapabilityName(uint32_t capability)
{
	if (capability == Capabilities::Restricted) return "Restricted";
	if (capability == Capabilities::Plugin) return "Plugin";
	if (capability == Capabilities::LocalUser) return "LocalUser";
	if (capability == Capabilities::WritePlayer) return "WritePlayer";
	if (capability == Capabilities::RobloxScript) return "RobloxScript";
	if (capability == Capabilities::RobloxEngine) return "RobloxEngine";
	if (capability == Capabilities::NotAccessible) return "NotAccessible";
	if (capability == Capabilities::RunClientScript) return "RunClientScript";
	if (capability == Capabilities::RunServerScript) return "RunServerScript";
	if (capability == Capabilities::AccessOutsideWrite) return "AccessOutsideWrite";
	if (capability == Capabilities::SpecialCapability) return "SpecialCapability";
	if (capability == Capabilities::AssetRequire) return "AssetRequire";
	if (capability == Capabilities::LoadString) return "LoadString";
	if (capability == Capabilities::ScriptGlobals) return "ScriptGlobals";
	if (capability == Capabilities::CreateInstances) return "CreateInstances";
	if (capability == Capabilities::Basic) return "Basic";
	if (capability == Capabilities::Audio) return "Audio";
	if (capability == Capabilities::DataStore) return "DataStore";
	if (capability == Capabilities::Network) return "Network";
	if (capability == Capabilities::Physics) return "Physics";
	if (capability == Capabilities::Dummy) return "Dummy";

	return "Unknown";
}

Capabilities::CapabilityType nameToCapability(const char* name)
{
	if (strcmp_caseInsensitive(name, "All"))
	{
		return (Capabilities::CapabilityType)(
			Capabilities::Plugin
			| Capabilities::LocalUser
			| Capabilities::WritePlayer
			| Capabilities::RobloxScript
			| Capabilities::RobloxEngine
			| Capabilities::NotAccessible
			| Capabilities::RunClientScript
			| Capabilities::RunServerScript
			| Capabilities::AccessOutsideWrite
			| Capabilities::SpecialCapability
			| Capabilities::AssetRequire
			| Capabilities::LoadString
			| Capabilities::ScriptGlobals
			| Capabilities::CreateInstances
			| Capabilities::Basic
			| Capabilities::Audio
			| Capabilities::DataStore
			| Capabilities::Network
			| Capabilities::Physics
			);
	}

	if (strcmp_caseInsensitive(name, "Restricted")) return Capabilities::Restricted;
	if (strcmp_caseInsensitive(name, "Plugin")) return Capabilities::Plugin;
	if (strcmp_caseInsensitive(name, "LocalUser")) return Capabilities::LocalUser;
	if (strcmp_caseInsensitive(name, "WritePlayer")) return Capabilities::WritePlayer;
	if (strcmp_caseInsensitive(name, "RobloxScript")) return Capabilities::RobloxScript;
	if (strcmp_caseInsensitive(name, "RobloxEngine")) return Capabilities::RobloxEngine;
	if (strcmp_caseInsensitive(name, "NotAccessible")) return Capabilities::NotAccessible;
	if (strcmp_caseInsensitive(name, "RunClientScript")) return Capabilities::RunClientScript;
	if (strcmp_caseInsensitive(name, "RunServerScript")) return Capabilities::RunServerScript;
	if (strcmp_caseInsensitive(name, "AccessOutsideWrite")) return Capabilities::AccessOutsideWrite;
	if (strcmp_caseInsensitive(name, "SpecialCapability")) return Capabilities::SpecialCapability;
	if (strcmp_caseInsensitive(name, "AssetRequire")) return Capabilities::AssetRequire;
	if (strcmp_caseInsensitive(name, "LoadString")) return Capabilities::LoadString;
	if (strcmp_caseInsensitive(name, "ScriptGlobals")) return Capabilities::ScriptGlobals;
	if (strcmp_caseInsensitive(name, "CreateInstances")) return Capabilities::CreateInstances;
	if (strcmp_caseInsensitive(name, "Basic")) return Capabilities::Basic;
	if (strcmp_caseInsensitive(name, "Audio")) return Capabilities::Audio;
	if (strcmp_caseInsensitive(name, "DataStore")) return Capabilities::DataStore;
	if (strcmp_caseInsensitive(name, "Network")) return Capabilities::Network;
	if (strcmp_caseInsensitive(name, "Physics")) return Capabilities::Physics;

	return Capabilities::Restricted;
}

int carbon_setcapability(lua_State* L)
{
	const char* name = luaL_checklstring(L, 1);
	bool doSet = luaL_optboolean(L, 2, true);
	if (doSet)
	{
		L->userdata->capabilities.set(nameToCapability(name));
		getCurrentContext()->capabilities.set(nameToCapability(name));
	}
	else
	{
		L->userdata->capabilities.clear(nameToCapability(name));
		getCurrentContext()->capabilities.clear(nameToCapability(name));
	}

	return 0;
}

int carbon_hascapability(lua_State* L)
{
	const char* name = luaL_checklstring(L, 1);
	lua_pushboolean(L, getCurrentContext()->capabilities.isSet(nameToCapability(name)));

	return 1;
}

int carbon_checkcaller(lua_State* L)
{
	lua_pushboolean(L, L->userdata->capabilities.isSet(Capabilities::OurThread));
	return 1;
}

int carbon_isourthread(lua_State* L)
{
	int argbase = 0;
	auto target = getthread(L, argbase);
	lua_pushboolean(L, target->userdata->capabilities.isSet(Capabilities::OurThread));
	return 1;
}

int carbon_setourthread(lua_State* L)
{
	int argbase = 0;
	auto target = getthread(L, argbase);

	bool isOur = luaL_checkboolean(L, argbase + 1);
	if (isOur)
		target->userdata->capabilities.set(Capabilities::OurThread);
	else
		target->userdata->capabilities.clear(Capabilities::OurThread);

	return 0;
}

int carbon_setidentity(lua_State* L)
{
	int argbase = 0;
	auto target = getthread(L, argbase);
	int identity = luaL_checkinteger(L, argbase + 1);
	target->userdata->identity = identity;
	if (target == L)
		getCurrentContext()->identity = identity;
	return 0;
}

int carbon_getidentity(lua_State* L)
{
	int argbase = 0;
	auto target = getthread(L, argbase);
	lua_pushinteger(L, target->userdata->identity);
	return 1;
}


int carbon_getcallingscript(lua_State* L)
{
	auto extraSpace = L->userdata;
	if (auto script = extraSpace->script)
		InstanceBridge_pushshared(L, script->shared.lock());
	else
		lua_pushnil(L);

	return 1;
}

int carbon_getinstances(lua_State* L)
{
	lua_createtable(L, 0, 0);
	push_instanceBridgeMap(L);

	int index = 0;
	lua_pushnil(L); // Stack: result, map, nil
	while (lua_next(L, -2)) // Stack: result, map, k, v
	{
		if (isTypeofType(L, -1, "Instance"))
		{
			lua_pushinteger(L, ++index); // Stack: result, map, k, v, index
			lua_pushvalue(L, -2); // Stack: result, map, k, v, index, v
			lua_rawset(L, -6); // Stack: result, map, k, v
		}
		lua_pop(L, 1); // Stack: result, map, k
	}
	lua_pushvalue(L, -2); // Stack: result, map, result
	return 1;
}

int carbon_getnilinstances(lua_State* L)
{
	lua_createtable(L, 0, 0);
	push_instanceBridgeMap(L);

	int index = 0;
	lua_pushnil(L); // Stack: result, map, nil
	while (lua_next(L, -2)) // Stack: result, map, k, v
	{
		if (isTypeofType(L, -1, "Instance"))
		{
			if (auto instance = toInstance(L, -1))
			{
				if (instance->parent == nullptr)
				{
					lua_pushinteger(L, ++index); // Stack: result, map, k, v, index
					lua_pushvalue(L, -2); // Stack: result, map, k, v, index, v
					lua_rawset(L, -6); // Stack: result, map, k, v
				}
			}
		}
		lua_pop(L, 1); // Stack: result, map, k
	}
	lua_pushvalue(L, -2); // Stack: result, map, result
	return 1;
}


int carbon_cacheinvalidate(lua_State* L)
{
	auto instance = checkInstance(L, 1);
	push_instanceBridgeMap(L);
	lua_pushlightuserdatatagged(L, instance);
	lua_pushnil(L);
	lua_rawset(L, -3);
	return 1;
}

int carbon_cachereplace(lua_State* L)
{
	auto instance = checkInstance(L, 1);
	checkInstance(L, 2);
	push_instanceBridgeMap(L);
	lua_pushlightuserdatatagged(L, instance);
	lua_pushvalue(L, 2);
	lua_rawset(L, -3);
	return 1;
}

int carbon_iscached(lua_State* L)
{
	auto instance = checkInstance(L, 1);
	push_instanceBridgeMap(L);
	lua_pushlightuserdatatagged(L, instance);
	lua_rawget(L, -2);
	lua_pushboolean(L, !lua_isnil(L, -1));
	return 1;
}

int carbon_cloneref(lua_State* L)
{
	auto instance = checkInstance(L, 1);
	lua_newuserdatatagged(L, sizeof(instance));
	lua_getmetatable(L, 1);
	lua_setmetatable(L, -2);
	return 1;
}

int carbon_loadstring(lua_State* L)
{
	size_t length = 0;
	const char* source = luaL_checklstring(L, 1, &length);
	const char* chunkname = luaL_optlstring(L, 2, "COAL");

	if (luaApiRuntimeState.compile(L, source, chunkname))
		return 1;

	lua_pushnil(L);
	lua_insert(L, -2);
	return 2;
}