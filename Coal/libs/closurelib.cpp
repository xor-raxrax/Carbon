#include "../../Common/Luau/Luau.h"
#include "closurelib.h"
#include "../FunctionMarker.h"
#include "../HookHandler.h"

#include "../LuaEnv.h"

int coal_iscclosure(lua_State* L)
{
	lua_pushboolean(L, luaL_checkfunction(L, 1)->isC);
	return 1;
}

int coal_islclosure(lua_State* L)
{
	lua_pushboolean(L, !luaL_checkfunction(L, 1)->isC);
	return 1;
}

inline void expectLuaFunction(lua_State* L, int idx)
{
	if (iscfunction(luaL_checkfunction(L, idx)))
		luaL_argerrorL(L, 1, "Lua function expected");
}

inline void expectCFunction(lua_State* L, int idx)
{
	if (isluafunction(luaL_checkfunction(L, idx)))
		luaL_argerrorL(L, 1, "C function expected");
}

inline void expectLuaFunction(lua_State* L, Closure& closure)
{
	if (closure.isC)
		luaL_argerrorL(L, 1, "Lua function expected");
}

int usercclosureHandler(lua_State* L)
{
	int passedArgsSize = lua_gettop(L);

	lua_pushvalue(L, lua_upvalueindex(1));

	for (int i = 1; i <= passedArgsSize; i++)
		lua_pushvalue(L, i);

	lua_call(L, passedArgsSize, LUA_MULTRET);

	int valuesReturned = lua_gettop(L) - passedArgsSize;
	return valuesReturned;
}

int coal_newcclosure(lua_State* L)
{
	expectLuaFunction(L, 1);
	auto name = luaL_optlstring(L, 2, nullptr);
	lua_pushvalue(L, 1);
	lua_pushcclosure(L, usercclosureHandler, name, 1);
	return 1;
}

void checkUpvalueCountForHook(lua_State* L, Closure* target, Closure* hook)
{
	if (hook->nupvalues > target->nupvalues && hook->nupvalues > 1)
		luaL_errorL(L, "hook function has too many upvalues");
}

void replaceLuaClosure(lua_State* L, Closure* target, Closure* hook)
{
	for (int i = 0; i < hook->nupvalues; i++)
		setobj(&target->l.uprefs[i], &hook->l.uprefs[i]);

	target->nupvalues = hook->nupvalues;
	target->stacksize = hook->stacksize;
	target->preload = hook->preload;
	target->gclist = hook->gclist;

	// change target's proto to newProto, which contains hook code
	auto newProto = luaF_newproto(L);
	auto targetProto = target->l.p;
	auto hookProto = hook->l.p;

	newProto->nups = hookProto->nups;
	newProto->nups = hookProto->nups;
	newProto->numparams = hookProto->numparams;
	newProto->is_vararg = hookProto->is_vararg;
	newProto->maxstacksize = hookProto->maxstacksize;
	newProto->flags = hookProto->flags;

	newProto->k = hookProto->k;
	newProto->code = hookProto->code;
	newProto->p = hookProto->p;
	newProto->codeentry = hookProto->codeentry;

	newProto->execdata = hookProto->execdata;
	newProto->exectarget = hookProto->exectarget;

	// TODO: add line info setting?
	newProto->lineinfo = hookProto->lineinfo; // setting?
	newProto->abslineinfo = hookProto->abslineinfo; // setting?
	newProto->locvars = hookProto->locvars; // setting?
	newProto->upvalues = hookProto->upvalues;
	newProto->source = targetProto->source;

	newProto->debugname = targetProto->debugname;
	newProto->debuginsn = targetProto->debuginsn;

	newProto->typeinfo = targetProto->typeinfo;

	newProto->userdata = hookProto->userdata;
	newProto->gclist = hookProto->gclist;

	newProto->sizecode = hookProto->sizecode;
	newProto->sizep = hookProto->sizep;
	newProto->sizelocvars = hookProto->sizelocvars;
	newProto->sizeupvalues = hookProto->sizeupvalues;
	newProto->sizek = hookProto->sizek;
	newProto->sizelineinfo = hookProto->sizelineinfo; // setting?
	newProto->linegaplog2 = hookProto->linegaplog2;
	newProto->linedefined = targetProto->linedefined;
	newProto->bytecodeid = hookProto->bytecodeid;
	newProto->sizetypeinfo = hookProto->sizetypeinfo;

	target->l.p = newProto;
}

void pushLuaClosureCopy(lua_State* L, Closure* target)
{
	Closure* copy = luaF_newLclosure(L, target->nupvalues, target->env, target->l.p);
	lua_pushclosure(L, copy);

	copy->nupvalues = target->nupvalues;
	copy->stacksize = target->stacksize;
	copy->preload = target->preload;
	copy->gclist = target->gclist;
}


void pushCClosureCopy(lua_State* L, Closure* target)
{
	for (int i = 0; i < target->nupvalues; i++)
		luaA_pushobject(L, &target->c.upvals[i]);

	lua_pushcclosure(L, target->c.f, target->c.debugname, target->nupvalues);
	clvalue(luaA_toobject(L, -1))->env = target->env;
}

void replaceCClosure(Closure* target, Closure* hook)
{
	target->c.f = hook->c.f;
	for (int i = 0; i < hook->nupvalues; i++)
		setobj(&target->c.upvals[i], &hook->c.upvals[i]);
}

int coal_hookfunction(lua_State* L)
{
	luaL_checkfunction(L, 1);
	luaL_checkfunction(L, 2);

	if (lua_iscfunction(L, 1))
	{
		auto target = clvalue(luaA_toobject(L, 1));

		Closure* hook = nullptr;

		if (lua_isluafunction(L, 2))
		{
			lua_pushvalue(L, 2);
			lua_pushcclosure(L, usercclosureHandler, nullptr, 1);
			hook = clvalue(luaA_toobject(L, -1));
		}
		else
		{
			hook = clvalue(luaA_toobject(L, 2));
		}

		checkUpvalueCountForHook(L, target, hook);

		pushCClosureCopy(L, target);
		replaceCClosure(target, hook);
	}
	else
	{
		expectLuaFunction(L, 2);
		auto target = clvalue(luaA_toobject(L, 1));
		auto hook = clvalue(luaA_toobject(L, 2));

		checkUpvalueCountForHook(L, target, hook);

		pushLuaClosureCopy(L, target);
		replaceLuaClosure(L, target, hook);
	}

	return 1;
}

static thread_local bool isInMetamethodHandler = false;

CallInfo* luaD_growCI_hook(lua_State* L)
{
	if (!isInMetamethodHandler)
	{
		auto original = hookHandler.getHook(HookId::growCI).getOriginal();
		return reinterpret_cast<decltype(luaApiAddresses.luaD_growCI)>(original)(L);
	}
	else
	{
		const int hardlimit = LUAI_MAXCALLS + (LUAI_MAXCALLS >> 3);
		const int hooklimit = LUAI_MAXCALLS + (LUAI_MAXCALLS >> 4);

		if (L->size_ci >= hardlimit)
			luaD_throw(L, LUA_ERRERR); // error while handling stack error

		int request;
		if (L->size_ci >= LUAI_MAXCALLS)
			request = hooklimit;
		else
		{
			if (L->size_ci * 2 < LUAI_MAXCALLS)
				request = L->size_ci * 2;
			else
				request = LUAI_MAXCALLS;
		}

		luaApiAddresses.luaD_reallocCI(L, request);

		if (L->size_ci > hooklimit)
		{
			lua_pushstring(L, "stack overflow");
			luaD_throw(L, LUA_ERRRUN);
		}

		return ++L->ci;
	}
}

int hookmetamethod_handler(lua_State* L)
{
	int passedArgsSize = lua_gettop(L);
	lua_pushvalue(L, lua_upvalueindex(1));

	for (int i = 1; i <= passedArgsSize; i++)
		lua_pushvalue(L, i);

	isInMetamethodHandler = true;
	L->nCcalls--; // "hide" handler
	
	try
	{
		lua_call(L, passedArgsSize, LUA_MULTRET);
		L->nCcalls++;
		isInMetamethodHandler = false;
	}
	catch (...)
	{
		L->nCcalls++;
		isInMetamethodHandler = false;
		throw;
	}

	int valuesReturned = lua_gettop(L) - passedArgsSize;
	return valuesReturned;
}

// args:
// 1 - object
// 2 - metamethodName
// 3 - lua closure as hook
int coal_hookmetamethod(lua_State* L)
{
	TValue* object = index2addr(L, 1);
	Table* metatable = nullptr;

	switch (ttype(object))
	{
	case LUA_TTABLE:
		metatable = hvalue(object)->metatable;
		break;
	case LUA_TUSERDATA:
		metatable = uvalue(object)->metatable;
		break;
	default:
		luaL_typeerrorL(L, 1, "table or userdata");
	}

	if (!metatable)
		luaL_argerrorL(L, 1, "object does not have metatable");

	luaL_checklstring(L, 2);
	
	auto metamethodName = tsvalue(index2addr(L, 2));
	lua_pushrawtable(L, metatable);
	lua_pushvalue(L, 2);
	lua_rawget(L, -2);

	auto metamethod = luaA_toobject(L, -1);

	if (metamethod == luaO_nilobject())
		luaL_errorL(L, "object does not have metamethod with name '%s'", metamethodName->data);

	if (!ttisfunction(metamethod))
		luaL_errorL(L, "metamethod type is '%s', 'function' expected", lua_typename(L, metamethod->tt));

	Closure* hook = nullptr;
	Closure* target = clvalue(metamethod);

	expectLuaFunction(L, 3);
	if (iscfunction(metamethod))
	{
		lua_pushvalue(L, 3);
		lua_pushcclosure(L, hookmetamethod_handler, clvalue(metamethod)->c.debugname, 1);
		hook = clvalue(luaA_toobject(L, -1));

		checkUpvalueCountForHook(L, target, hook);

		pushCClosureCopy(L, target);

		if (hook->nupvalues > target->nupvalues && target->nupvalues == 0)
			target->nupvalues = 1;

		replaceCClosure(target, hook);
	}
	else
	{
		hook = clvalue(luaA_toobject(L, 3));
		checkUpvalueCountForHook(L, target, hook);

		pushLuaClosureCopy(L, target);
		replaceLuaClosure(L, target, hook);
	}

	return 1;
}

int coal_isourclosure(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TFUNCTION);
	auto func = clvalue(luaA_toobject(L, 1));
	lua_pushboolean(L, functionMarker->isOurClosure(func));
	return 1;
}

int coal_setourclosure(lua_State* L)
{
	expectLuaFunction(L, 1);
	auto func = clvalue(luaA_toobject(L, 1));
	functionMarker->setOurProto(func->l.p);
	return 0;
}

int coal_setsafeenv(lua_State* L)
{
	TValue* object = index2addr(L, 1);
	Table* env = nullptr;

	switch (ttype(object))
	{
	case LUA_TTABLE:
		env = hvalue(object);
		break;
	case LUA_TFUNCTION:
		env = clvalue(object)->env;
		if (!env)
			env = L->gt;
		break;
	case LUA_TTHREAD:
		env = thvalue(object)->gt;
		break;
	case LUA_TBOOLEAN:
		// one arg mode
		L->gt->safeenv = luaL_checkboolean(L, 1);
		return 0;
	default:
		luaL_typeerrorL(L, 1, "table | function | thread | bool");
	}
	
	env->safeenv = luaL_checkboolean(L, 2);
	return 0;
}


int coal_getstack(lua_State* L)
{
	int level = luaL_checkinteger(L, 1);
	
	auto callInfo = getcallinfo(L, level);
	if (!callInfo)
		luaL_argerrorL(L, 1, "level out of range");

	if (clvalue(callInfo->func)->isC)
		luaL_errorL(L, "stack points to a C closure, Lua function expected");

	if (lua_isnumber(L, 2))
	{
		int index = lua_tointegerx(L, 2);
		if (index < 1)
			luaL_errorL(L, "stack index starts at 1");

		if (callInfo->top - callInfo->base < index)
			luaL_errorL(L, "stack index out of range");

		luaA_pushobject(L, callInfo->base + index - 1);
	}
	else
	{
		lua_createtable(L, 0, 0);

		int index = 1;
		for (StkId v = callInfo->base; v < callInfo->top; v++)
		{
			lua_pushinteger(L, index++);
			luaA_pushobject(L, v);
			lua_settable(L, -3);
		}
	}

	return 1;
}

int coal_setstack(lua_State* L)
{
	int level = luaL_checkinteger(L, 1);
	int index = luaL_checkinteger(L, 2);

	auto callInfo = getcallinfo(L, level);
	if (!callInfo)
		luaL_argerrorL(L, 1, "level out of range");

	if (clvalue(callInfo->func)->isC)
		luaL_errorL(L, "stack points to a C closure, Lua function expected");

	if (index < 1)
		luaL_errorL(L, "stack index starts at 1");

	if (callInfo->top - callInfo->base < index)
		luaL_errorL(L, "stack index out of range");

	TValue* target = callInfo->base + index - 1;
	TValue* changeTo = index2addr(L, 3);

	if (luaApiRuntimeState.getLuaSettings().setstack_block_different_type)
	{
		if (ttype(target) != ttype(changeTo))
			luaL_errorL(L, "cannot change value to a different type");
	}
	
	setobj(target, changeTo);

	return 0;
}

int coal_getmaxstacksize(lua_State* L)
{
	lua_pushinteger(L, L->stacksize);
	lua_pushinteger(L, L->size_ci);
	return 2;
}

int coal_getstacksize(lua_State* L)
{
	lua_pushinteger(L, (int)(L->top - L->stack));
	lua_pushinteger(L, (int)(L->end_ci - L->base_ci));
	return 2;
}

int coal_setupvalue(lua_State* L)
{
	Closure* func = getclosure(L, 1, true);
	if (luaApiRuntimeState.getLuaSettings().setupvalue_block_cclosure)
		expectLuaFunction(L, *func);

	int index = luaL_checkinteger(L, 2);
	luaL_checktype(L, 2, LUA_TNUMBER);
	luaL_checkany(L, 3);

	if (index < 1)
		luaL_errorL(L, "upvalue index starts at 1");

	if (index > func->nupvalues)
		luaL_errorL(L, "upvalue index is out of range");

	auto changeTo = index2addr(L, 3);

	if (func->isC)
	{
		TValue* value = &func->c.upvals[index - 1];
		setobj(value, changeTo);
	}
	else
	{
		TValue* upvalueo = &func->l.uprefs[index - 1];
		TValue* value = ttisupval(upvalueo) ? upvalue(upvalueo)->v : upvalueo;
		setobj(upvalueo, changeTo);
	}

	return 0;
}

int coal_getupvalues(lua_State* L)
{
	Closure* func = getclosure(L, 1, true);

	if (func->isC && luaApiRuntimeState.getLuaSettings().getupvalue_block_cclosure)
	{
		lua_pushnil(L);
		return 1;
	}

	auto sizeups = func->nupvalues;
	lua_createtable(L, sizeups, 0);

	for (int index = 0; index < sizeups; index++)
	{
		lua_pushinteger(L, index + 1);

		TValue* value;

		if (func->isC)
		{
			value = &func->c.upvals[index];
		}
		else
		{
			TValue* up = &func->l.uprefs[index];
			value = ttisupval(up) ? upvalue(up)->v : up;
		}

		luaA_pushobject(L, value);
		lua_settable(L, -3);
	}

	return 1;
}

int coal_getupvalue(lua_State* L)
{
	Closure* func = getclosure(L, 1, true);
	int index = luaL_checkinteger(L, 2);

	if (luaApiRuntimeState.getLuaSettings().getupvalue_block_cclosure && func->isC)
		lua_pushnil(L);
	else
	{
		if (index < 1)
			luaL_errorL(L, "upvalue index starts at 1");
		if (index - 1 >= func->nupvalues)
			luaL_errorL(L, "upvalue index is out of range");

		TValue* value;

		if (func->isC)
		{
			value = &func->c.upvals[index - 1];
		}
		else
		{
			TValue* up = &func->l.uprefs[index - 1];
			value = ttisupval(up) ? upvalue(up)->v : up;
		}

		luaA_pushobject(L, value);
	}

	return 1;
}

void cloneConstant(lua_State* L, TValue* constants, int index)
{
	switch (constants[index].tt)
	{
	case LUA_TNIL:
		lua_pushnil(L);
		break;
	case LUA_TBOOLEAN:
		lua_pushboolean(L, constants[index].value.b);
		break;
	case LUA_TNUMBER:
		lua_pushnumber(L, constants[index].value.n);
		break;
	case LUA_TSTRING:
	{
		auto& string = constants[index].value.gc->ts;
		lua_pushlstring(L, string.data, string.len);
		break;
	}
	case LUA_TVECTOR:
	{
		lua_pushvector(L,
			constants[index].value.v[0],
			constants[index].value.v[1],
			constants[index].value.v[2]
		);
		break;
	}
	case LUA_TFUNCTION:
	{
		if (luaApiRuntimeState.getLuaSettings().getconstant_block_functions)
		{
			lua_pushlightuserdatatagged(L, nullptr);
			break;
		}

		[[fallthrough]];
	}
	case LUA_TTABLE:
	case LUA_TUSERDATA:
		luaA_pushobject(L, &constants[index]);
		break;
	default:
		lua_pushnil(L);
		break;
	}
}

int coal_getconstants(lua_State* L)
{
	Closure* func = getclosure(L, 1, true);
	expectLuaFunction(L, *func);

	auto sizek = func->l.p->sizek;
	lua_createtable(L, sizek, 0);

	for (int index = 0; index < sizek; index++)
	{
		lua_pushinteger(L, index + 1);
		cloneConstant(L, func->l.p->k, index);
		lua_settable(L, -3);
	}

	return 1;
}

int coal_getconstant(lua_State* L)
{
	Closure* func = getclosure(L, 1, true);
	expectLuaFunction(L, *func);

	auto index = luaL_checkinteger(L, 2);

	if (index < 1)
		luaL_errorL(L, "constant index starts at 1");
	if (index >= func->l.p->sizek)
		luaL_errorL(L, "constant index is out of range");

	cloneConstant(L, func->l.p->k, index - 1);
	return 1;
}

int coal_setconstant(lua_State* L)
{
	Closure* func = getclosure(L, 1, true);
	expectLuaFunction(L, *func);

	auto index = luaL_checkinteger(L, 2);

	if (index < 1)
		luaL_errorL(L, "constant index starts at 1");
	if (index >= func->l.p->sizek)
		luaL_errorL(L, "constant index is out of range");

	auto target = &func->l.p->k[index - 1];
	auto changeTo = index2addr(L, 3);

	setobj(target, changeTo);

	return 0;
}

int coal_getprotos(lua_State* L)
{
	Closure* func = getclosure(L, 1, true);
	expectLuaFunction(L, *func);

	auto sizep = func->l.p->sizep;
	lua_createtable(L, sizep, 0);

	for (int index = 0; index < sizep; index++)
	{
		// lua index style
		lua_pushinteger(L, index + 1);

		Closure* cl = luaF_newLclosure(L, 0, func->env, *(func->l.p->p) + index);
		lua_pushclosure(L, cl);

		lua_settable(L, -3);
	}

	return 1;
}

int coal_getproto(lua_State* L)
{
	Closure* func = getclosure(L, 1, true);
	expectLuaFunction(L, *func);

	// lua index style
	auto index = luaL_checkinteger(L, 2);

	if (index < 1)
		luaL_errorL(L, "proto index starts at 1");
	if (index >= func->l.p->sizep)
		luaL_errorL(L, "proto index is out of range");

	Closure* cl = luaF_newLclosure(L, 0, func->env, *(func->l.p->p) + index - 1);
	lua_pushclosure(L, cl);

	return 1;
}

// TODO: ignores possible upvalue disbalance
int coal_setproto(lua_State* L)
{
	Closure* func = getclosure(L, 1, true);
	expectLuaFunction(L, *func);

	// lua index style
	auto index = luaL_checkinteger(L, 2);

	if (index < 1)
		luaL_errorL(L, "proto index starts at 1");
	if (index >= func->l.p->sizep)
		luaL_errorL(L, "proto index is out of range");

	expectLuaFunction(L, 3);
	auto hook = clvalue(luaA_toobject(L, 1));

	func->l.p->p[index - 1] = hook->l.p;
	return 0;
}

int coal_getprotoinfo(lua_State* L)
{
	Closure* func = getclosure(L, 1, true);
	expectLuaFunction(L, *func);
	int results = 0;

	Proto* proto = func->l.p;

	const char* options = luaL_checklstring(L, 2);
	for (const char* it = options; *it; it++)
	{
		switch (*it)
		{
		case 'p':
			lua_pushinteger(L, proto->sizep);
			results++;
			break;
		case 'u':
			lua_pushinteger(L, proto->nups);
			results++;
			break;
		case 'k':
			lua_pushinteger(L, proto->sizek);
			results++;
			break;
		case 'b':
			lua_pushinteger(L, proto->bytecodeid);
			results++;
			break;
		case 's':
			lua_pushinteger(L, proto->maxstacksize);
			results++;
			break;
		default:
			luaL_argerrorL(L, 2, "invalid option");
		}
	}

	return results;
}

int coal_getinfo(lua_State* L)
{
	lua_Debug ar;
	int argsBase;
	lua_State* L1 = getthread(L, argsBase);

	CallInfo* call = nullptr;
	Closure* func = nullptr;

	func = lua_toclosure(L1, argsBase + 1);
	if (!func)
	{
		call = getcallinfo(L1, argsBase + 1, true);
		func = clvalue(call->func);
	}

	const char* options = luaL_optlstring(L, argsBase + 2, "flSu");

	lua_createtable(L, 0, 4);

	if (strchr(options, 'S'))
	{
		if (func->isC)
		{
			ar.source = "=[C]";
			ar.what = "C";
			ar.linedefined = -1;
			ar.short_src = "[C]";
		}
		else
		{
			TString* source = func->l.p->source;
			ar.source = getstr(source);
			ar.what = "Lua";
			ar.linedefined = func->l.p->linedefined;
			ar.short_src = luaO_chunkid(ar.ssbuf, sizeof(ar.ssbuf), getstr(source), source->len);
		}

		lua_pushstring(L, ar.source);
		lua_setfield(L, -2, "source");

		lua_pushstring(L, ar.short_src);
		lua_setfield(L, -2, "short_src");

		lua_pushinteger(L, ar.linedefined);
		lua_setfield(L, -2, "linedefined");

		lua_pushstring(L, ar.what);
		lua_setfield(L, -2, "what");
	}

	if (strchr(options, 'l'))
	{
		int currentline = getcurrentline(func, call);
		lua_pushinteger(L, currentline);
		lua_setfield(L, -2, "currentline");
	}

	if (strchr(options, 'u'))
	{
		lua_pushinteger(L, func->nupvalues);
		lua_setfield(L, -2, "nups");
	}

	if (strchr(options, 'f'))
	{
		lua_pushclosure(L, func);
		lua_setfield(L, -2, "func");
	}

	return 1;
}