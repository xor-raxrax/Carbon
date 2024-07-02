export module LuaEnv;

import <filesystem>;

export struct LuaApiSettings
{
	bool getupvalue_block_cclosure = false;
	bool setupvalue_block_cclosure = false;
	bool setstack_block_different_type = false;
	bool getconstant_block_functions = false;
	bool allow_setproto = true;
	bool getstateenv_returns_ref = true;
};

export class LuaApiRuntimeState
{
public:

	void injectEnvironment(struct lua_State* L);

	void setLuaSettings(LuaApiSettings* settings) { apiSettings = settings; };
	const LuaApiSettings& getLuaSettings() const { return *apiSettings; };

	std::filesystem::path userContentApiRootDirectory;
	struct Table* mainEnv = nullptr;
private:
	LuaApiSettings* apiSettings = nullptr;
};

export inline LuaApiRuntimeState luaApiRuntimeState;