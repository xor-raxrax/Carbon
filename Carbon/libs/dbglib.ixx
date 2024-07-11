module;
#include "../../Common/Windows.h"
#include "../../Common/Formatter.h"
export module libs.dbglib;

import <ranges>;
import Luau;
import Luau.Riblix;
import RiblixStructures;
import StringUtils;

int carbon_disablepointerencoding(lua_State* L);
int carbon_getdescriptors(lua_State* L);
int carbon_getdescriptorinfo(lua_State* L);
int carbon_getscriptcontext(lua_State* L);
int carbon_getcfunction(lua_State* L);
int carbon_repush(lua_State* L);
int carbon_gettt(lua_State* L);
int carbon_getgcaddr(lua_State* L);
int carbon_torva(lua_State* L);
int carbon_getcontext(lua_State* L);

int carbon_getinstancebrigdemap(lua_State* L);

int carbon_dumpstacks(lua_State* L);

export const luaL_Reg debug_library[] = {
	{"disablepointerencoding", carbon_disablepointerencoding},
	{"getdescriptors", carbon_getdescriptors},
	{"getdescriptorinfo", carbon_getdescriptorinfo},
	{"dumpstacks", carbon_dumpstacks},
	{"torva", carbon_torva},
	{"getcontext", carbon_getcontext},

	{"getscriptcontext", carbon_getscriptcontext},
	{"getcfunction", carbon_getcfunction},
	{"getgcaddr", carbon_getgcaddr},
	{"repush", carbon_repush},
	{"gettt", carbon_gettt},

	{"getinstancebrigdemap", carbon_getinstancebrigdemap},

	{nullptr, nullptr},
};

int carbon_disablepointerencoding(lua_State* L)
{
	L->global->ptrenckey[0] = 1;
	L->global->ptrenckey[1] = 0;
	L->global->ptrenckey[2] = 0;
	L->global->ptrenckey[3] = 0;
	return 0;
}

int carbon_getinstancebrigdemap(lua_State* L)
{
	push_instanceBridgeMap(L);
	return 1;
}

// returns false if passed categoryName is invalid
bool pushDescriptorsTable(lua_State* L, const char* categoryName, Instance* instance)
{
	auto classDescriptor = instance->classDescriptor;

	std::vector<Descriptor*>* collection = nullptr;

	if (strcmp_caseInsensitive(categoryName, "Property"))
	{
		collection = reinterpret_cast<decltype(collection)>(&classDescriptor->
			MemberDescriptorContainer<PropertyDescriptor>::collection);
	}
	else if (strcmp_caseInsensitive(categoryName, "Event"))
	{
		collection = reinterpret_cast<decltype(collection)>(&classDescriptor->
			MemberDescriptorContainer<EventDescriptor>::collection);
	}
	else if (strcmp_caseInsensitive(categoryName, "Function"))
	{
		collection = reinterpret_cast<decltype(collection)>(&classDescriptor->
			MemberDescriptorContainer<FunctionDescriptor>::collection);
	}
	else if (strcmp_caseInsensitive(categoryName, "YieldFunction"))
	{
		collection = reinterpret_cast<decltype(collection)>(&classDescriptor->
			MemberDescriptorContainer<YieldFunctionDescriptor>::collection);
	}
	else if (strcmp_caseInsensitive(categoryName, "Callback"))
	{
		collection = reinterpret_cast<decltype(collection)>(&classDescriptor->
			MemberDescriptorContainer<CallbackDescriptor>::collection);
	}

	if (!collection)
		return false;

	lua_createtable(L, (int)collection->size(), 0);

	for (auto [index, descriptor] : std::ranges::views::enumerate(*collection))
	{
		lua_pushinteger(L, (int)index + 1); // lua array style
		lua_pushstring(L, descriptor->name->c_str());
		lua_settable(L, -3);
	}

	return true;
}

int carbon_getdescriptors(lua_State* L)
{
	auto instance = checkInstance(L, 1);
	const char* name = luaL_checklstring(L, 2);

	if (strcmp_caseInsensitive(name, "All"))
	{
		lua_createtable(L, 0, 5);

		lua_pushstring(L, "Property");
		pushDescriptorsTable(L, "Property", instance);
		lua_settable(L, -3);

		lua_pushstring(L, "Event");
		pushDescriptorsTable(L, "Event", instance);
		lua_settable(L, -3);

		lua_pushstring(L, "Function");
		pushDescriptorsTable(L, "Function", instance);
		lua_settable(L, -3);

		lua_pushstring(L, "YieldFunction");
		pushDescriptorsTable(L, "YieldFunction", instance);
		lua_settable(L, -3);

		lua_pushstring(L, "Callback");
		pushDescriptorsTable(L, "Callback", instance);
		lua_settable(L, -3);
	}
	else
	{
		if (!pushDescriptorsTable(L, name, instance))
			luaL_argerrorL(L, 2, "invalid descriptor collection name");
	}

	return 1;
}

int carbon_getdescriptorinfo(lua_State* L)
{
	auto instance = checkInstance(L, 1);

	const char* descriptorName = luaL_checklstring(L, 2);

	Descriptor* target = nullptr;

	if (auto property = instance->classDescriptor->MemberDescriptorContainer<PropertyDescriptor>::getDescriptor(descriptorName))
	{
		lua_createtable(L, 0, 3);
		target = property;

		lua_pushstring(L, "Get");
		lua_pushstring(L, Formatter::pointerToString(property->getset->get).c_str());
		lua_settable(L, -3);

		lua_pushstring(L, "Set");
		lua_pushstring(L, Formatter::pointerToString(property->getset->set).c_str());
		lua_settable(L, -3);

		return 1;
	}
	else if (auto event = instance->classDescriptor->MemberDescriptorContainer<EventDescriptor>::getDescriptor(descriptorName))
	{
		lua_createtable(L, 0, 1);
		target = event;
	}
	else if (auto function = instance->classDescriptor->MemberDescriptorContainer<FunctionDescriptor>::getDescriptor(descriptorName))
	{
		lua_createtable(L, 0, 1);
		target = function;
	}
	else if (auto yieldFunction = instance->classDescriptor->MemberDescriptorContainer<YieldFunctionDescriptor>::getDescriptor(descriptorName))
	{
		lua_createtable(L, 0, 1);
		target = yieldFunction;
	}
	else if (auto callback = instance->classDescriptor->MemberDescriptorContainer<CallbackDescriptor>::getDescriptor(descriptorName))
	{
		lua_createtable(L, 0, 1);
		target = callback;
	}
	else
	{
		luaL_argerrorL(L, 2, "invalid descriptor name");
	}

	lua_pushstring(L, "Address");
	lua_pushstring(L, Formatter::pointerToString(target).c_str());
	lua_settable(L, -3);

	return 0;
}

int carbon_getscriptcontext(lua_State* L)
{
	auto extraSpace = L->userdata;
	auto scriptContext = extraSpace->scriptContext;
	InstanceBridge_pushshared(L, scriptContext->shared.lock());
	return 1;
}

int carbon_getcfunction(lua_State* L)
{
	Closure* func = getclosure(L, 1, true);
	if (!func->isC)
		luaL_argerrorL(L, 1, "c closure expected");

	lua_pushstring(L, Formatter::pointerToString(func->c.f).c_str());
	return 1;
}

int carbon_gettt(lua_State* L)
{
	lua_pushinteger(L, luaA_toobject(L, 1)->tt);
	return 1;
}

int carbon_getgcaddr(lua_State* L)
{
	lua_pushstring(L, Formatter::pointerToString(luaA_toobject(L, -1)->value.gc).c_str());
	return 1;
}

int carbon_torva(lua_State* L)
{
	auto address = Formatter::stringToPointer(luaL_checklstring(L, 1));
	if (errno == ERANGE)
		luaL_argerrorL(L, 1, "invalid address");
	lua_pushstring(L, Formatter::pointerToString(address - (uintptr_t)GetModuleHandle(nullptr) - 0x1000).c_str());
	return 1;
}

int carbon_getcontext(lua_State* L)
{
	lua_pushstring(L, Formatter::pointerToString(getCurrentContext()).c_str());
	return 1;
}

int carbon_repush(lua_State* L)
{
	auto instance = checkInstance(L, 1);
	InstanceBridge_pushshared(L, instance->shared.lock());
	InstanceBridge_pushshared(L, instance->shared.lock());
	return 2;
}

void safePush(lua_State* L, StkId v)
{
	__try
	{
		luaA_pushobject(L, v);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		lua_pushstring(L, "uninitialized");
	}
}

int carbon_dumpstacks(lua_State* L)
{
	// save previous slots because we override them in this function
	const int savedSlotsSize = 20;
	StkId savedSlots[savedSlotsSize] = { nullptr };

	int freeSlotsSize = int(L->stack_last + 1 - L->top);
	int usedSavedSlotsSize = freeSlotsSize < savedSlotsSize ? freeSlotsSize : savedSlotsSize;
	memcpy(&savedSlots, L->top, usedSavedSlotsSize * sizeof(StkId));

	lua_createtable(L, 0, 4);

	// top is not included in dump info in these lambdas
	auto pushCiStack = [&](CallInfo* base, CallInfo* top)
	{
		lua_createtable(L, int(top - base), 0);

		int index = 1;
		for (CallInfo* ci = base; ci < top; ci++)
		{
			lua_pushinteger(L, index++);

			// push call info
			{
				lua_createtable(L, 0, 3);

				lua_pushstring(L, "address");
				lua_pushstring(L, Formatter::pointerToString(ci).c_str());
				lua_settable(L, -3);

				lua_pushstring(L, "addresses");
				// push addresses
				{
					lua_createtable(L, 0, 3);

					lua_pushstring(L, "base");
					lua_pushstring(L, Formatter::pointerToString(ci->base).c_str());
					lua_settable(L, -3);

					lua_pushstring(L, "func");
					lua_pushstring(L, Formatter::pointerToString(ci->func).c_str());
					lua_settable(L, -3);

					lua_pushstring(L, "top");
					lua_pushstring(L, Formatter::pointerToString(ci->top).c_str());
					lua_settable(L, -3);
				}
				lua_settable(L, -3);

				lua_pushstring(L, "objects");
				// push objects
				{
					lua_createtable(L, 0, 3);

					if (ci->base)
					{
						lua_pushstring(L, "base");
						safePush(L, ci->base);
						lua_settable(L, -3);
					}

					if (ci->func)
					{
						lua_pushstring(L, "func");
						safePush(L, ci->func);
						lua_settable(L, -3);
					}

					if (ci->top)
					{
						lua_pushstring(L, "top");
						safePush(L, ci->top);
						lua_settable(L, -3);
					}
				}

				lua_settable(L, -3);
			}

			lua_settable(L, -3);
		}
	};

	auto pushAddress = [&](StkId v, int index)
	{
		lua_pushinteger(L, index);
		lua_pushstring(L, Formatter::pointerToString(v).c_str());
		lua_settable(L, -3);
	};

	auto pushObject = [&](StkId v, int index)
	{
		lua_pushinteger(L, index);
		safePush(L, v);
		lua_settable(L, -3);
	};
	
	auto pushStack = [&](StkId base, StkId top)
	{
		lua_createtable(L, 0, 2);

		lua_pushstring(L, "addresses");
		// push addresses
		{
			lua_createtable(L, int(top - base), 0);

			int index = 1;
			for (StkId v = base; v < top; v++)
				pushAddress(v, index++);
		}

		lua_settable(L, -3);

		lua_pushstring(L, "objects");
		// push objects
		{
			lua_createtable(L, int(top - base), 0);
			int index = 1;
			for (StkId v = base; v < top; v++)
				pushObject(v, index++);
		}

		lua_settable(L, -3);
	};

	auto pushFreeStack = [&](StkId base, StkId top)
	{
		lua_createtable(L, 0, 2);

		lua_pushstring(L, "addresses");
		// push addresses
		{
			lua_createtable(L, 0, freeSlotsSize);

			int index = 1;

			for (int i = 0; i < usedSavedSlotsSize; i++)
				pushAddress(savedSlots[i], index++);

			if (freeSlotsSize > savedSlotsSize)
			{
				for (StkId v = base + savedSlotsSize; v < top; v++)
					pushAddress(v, index++);
			}
		}

		lua_settable(L, -3);

		lua_pushstring(L, "objects");
		// push objects
		{
			lua_createtable(L, 0, freeSlotsSize);

			int index = 1;

			for (int i = 0; i < usedSavedSlotsSize; i++)
				pushObject(savedSlots[i], index++);

			if (freeSlotsSize > savedSlotsSize)
			{
				for (StkId v = base + savedSlotsSize; v < top; v++)
					pushObject(v, index++);
			}
		}

		lua_settable(L, -3);
	};

	{
		lua_pushstring(L, "ci");
		pushCiStack(L->base_ci, L->ci + 1);
		lua_settable(L, -3);
	}

	{
		lua_pushstring(L, "free_ci");
		pushCiStack(L->ci + 1, L->end_ci);
		lua_settable(L, -3);
	}

	{
		lua_pushstring(L, "stack");
		pushStack(L->stack, L->top);
		lua_settable(L, -3);
	}

	{
		lua_pushstring(L, "free_stack");
		pushFreeStack(L->top, L->stack_last + 1);
		lua_settable(L, -3);
	}

	return 1;
}