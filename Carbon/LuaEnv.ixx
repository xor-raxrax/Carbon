export module LuaEnv;

import <filesystem>;
import LuaStateWatcher;

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

	void injectEnvironment(GlobalStateInfo* info) const;
	void runScript(GlobalStateInfo* info, const std::string& source) const;

	// pushes closure on success
	// OR error string on failure
	bool compile(struct lua_State* L, const char* source, const char* chunkname) const;

	void setLuaSettings(LuaApiSettings* settings) { apiSettings = settings; };
	const LuaApiSettings& getLuaSettings() const { return *apiSettings; };

	std::filesystem::path userContentApiRootDirectory;
private:
	LuaApiSettings* apiSettings = nullptr;
};

export inline LuaApiRuntimeState luaApiRuntimeState;