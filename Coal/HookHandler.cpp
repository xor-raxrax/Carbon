#include "HookHandler.h"
#include "MinHook/MinHook.h"
#include "../Common/Exception.h"

import <iostream>;
import Luau;
import libs.closurelib;

Hook::Hook(const std::string& name, void* hook)
	: name(name)
	, hook(hook)
{
}

bool Hook::setup()
{
	if (MH_OK != MH_CreateHook(target, hook, &original))
	{
		std::cout << "failed to create " + name + " hook" << std::endl;
		return false;
	}

	if (MH_OK != MH_EnableHook(target))
	{
		std::cout << "failed to enable " + name + " hook" << std::endl;
		return false;
	}

	return true;
}

bool Hook::remove()
{
	if (MH_OK != MH_RemoveHook(target))
	{
		std::cout << "failed to remove " + name + " hook" << std::endl;
		return false;
	}

	return true;
}

HookHandler::HookHandler()
	: hooks({
		{ "growCI", luaD_growCI_hook }
	})
{
	MH_Initialize();
}

HookHandler::~HookHandler()
{
	removeAll();
	MH_Uninitialize();
}

void HookHandler::setupAll()
{
	getHook(HookId::growCI).setTarget(luaApiAddresses.luaD_growCI);
	
	for (auto& hook : hooks)
		if (!hook.setup())
			raise("failed to setup hooks");
}

void HookHandler::removeAll()
{
	for (auto& hook : hooks)
		hook.remove();
}