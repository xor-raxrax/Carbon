module;
#include "MinHook/MinHook.h"
#include "../Common/Exception.h"
module HookHandler;

import <array>;
import <string>;
import Luau;
import libs.closurelib;
import Console;

Hook::Hook(const std::string& name, void* hook)
	: name(name)
	, hook(hook)
{
}

bool Hook::setup()
{
	if (MH_OK != MH_CreateHook(target, hook, &original))
	{
		Console::getInstance() << "failed to create " + name + " hook" << std::endl;
		return false;
	}

	if (MH_OK != MH_EnableHook(target))
	{
		Console::getInstance() << "failed to enable " + name + " hook" << std::endl;
		return false;
	}

	return true;
}

bool Hook::remove()
{
	if (MH_OK != MH_RemoveHook(target))
	{
		Console::getInstance() << "failed to remove " + name + " hook" << std::endl;
		return false;
	}

	return true;
}

HookHandler::HookHandler()
	: hooks({
		{ "growCI", luaD_growCI_hook },
		{ "lua_getfield", lua_getfield_Hook }
	})
{
	MH_Initialize();
}

HookHandler::~HookHandler()
{
	removeAll();
	MH_Uninitialize();
}

void HookHandler::removeAll()
{
	for (auto& hook : hooks)
		hook.remove();
}