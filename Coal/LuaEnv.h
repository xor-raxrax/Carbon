#pragma once

import <filesystem>;

struct ApiSettings
{
	const bool getupvalue_block_cclosure = false;
	const bool setupvalue_block_cclosure = false;
	const bool setstack_block_different_type = false;
	const bool getconstant_block_functions = false;
	const bool allow_setproto = true;
	const bool getstateenv_returns_ref = true;

	std::filesystem::path userContentApiRootDirectory;
	struct Table* mainEnv = nullptr;
};

inline ApiSettings apiSettings;

class LuaApiRegistrar
{
public:
	static void run(struct lua_State* L);
};