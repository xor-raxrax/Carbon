module;
#include "../../Common/Utils.h"
export module libs.fslib;

import <filesystem>;
import <fstream>;
import <array>;
import Luau;
import LuaEnv;

int coal_readfile(lua_State* L);
int coal_loadfile(lua_State* L);
int coal_writefile(lua_State* L);

int coal_isfile(lua_State* L);
int coal_isfolder(lua_State* L);
int coal_listfiles(lua_State* L);
int coal_makefolder(lua_State* L);

int coal_delfolder(lua_State* L);
int coal_delfile(lua_State* L);
int coal_appendfile(lua_State* L);

export const luaL_Reg filesystemLibrary[] = {
	{"readfile", coal_readfile},
	{"loadfile", coal_loadfile},
	{"writefile", coal_writefile},
	{"isfile", coal_isfile},
	{"isfolder", coal_isfolder},
	{"listfiles", coal_listfiles},
	{"makefolder", coal_makefolder},
	{"delfolder", coal_delfolder},
	{"delfile", coal_delfile},
	{"appendfile", coal_appendfile},

	{nullptr, nullptr},
}; 

std::wstring getPath(lua_State* L)
{
	size_t size;
	auto cstring = luaL_checklstring(L, 1, &size);

	std::string path = std::string(cstring, size);
	std::replace(path.begin(), path.end(), '/', '\\');

	if (path.find("..") != std::string::npos)
		luaL_errorL(L, "attempt to escape directory");

	std::wstring wpath = std::wstring(path.begin(), path.end());
	auto result = luaApiRuntimeState.userContentApiRootDirectory / wpath;
	
	return result;
}


bool isExtensionAllowed(const std::filesystem::path& path)
{
	static const std::array<std::string, 13> disallowedExtensions{
		".exe", ".scr", ".bat", ".com", ".csh", ".msi", ".vb", ".vbs", ".vbe", ".ws", ".wsf", ".wsh", ".ps1"
	};

	for (auto& test : disallowedExtensions)
		if (strcmp_caseInsensitive(path.extension().string(), test))
			return false;

	return true;
}

int coal_readfile(lua_State* L)
{
	auto path = getPath(L);

	if (!std::filesystem::exists(path))
		luaL_errorL(L, "file does not exist");

	std::ifstream stream(path, std::ios_base::binary);
	std::string result((std::istreambuf_iterator<char>(stream)),
		std::istreambuf_iterator<char>());

	lua_pushlstring(L, result.c_str(), result.size());

	return 1;
}

int coal_loadfile(lua_State* L)
{
	/*auto path = getPath(L);

	if (!std::filesystem::exists(path))
		luaL_errorL(L, "file does not exist");

	std::ifstream stream(path, std::ios_base::binary);
	std::string result((std::istreambuf_iterator<char>(stream)),
		std::istreambuf_iterator<char>());

	lua_getglobal(L, "loadstring");
	lua_pushlstring(L, result.c_str(), result.size());
	lua_pcall(L, 1, 1, 0);

	return 1;*/
	return 0;
}

int coal_writefile(lua_State* L)
{
	auto path = getPath(L);

	size_t contentSize;
	auto contentCString = luaL_checklstring(L, 2, &contentSize);

	if (!isExtensionAllowed(path))
		luaL_errorL(L, "forbidden extension");

	std::ofstream out;
	out.open(path, std::ios::out | std::ios::binary);
	out.write(contentCString, contentSize);
	out.close();

	return 0;
}

int coal_isfile(lua_State* L)
{
	auto path = getPath(L);
	lua_pushboolean(L, std::filesystem::is_regular_file(path));
	return 1;
}

int coal_isfolder(lua_State* L)
{
	auto path = getPath(L);
	lua_pushboolean(L, (std::filesystem::is_directory(path)));
	return 1;
}

int coal_listfiles(lua_State* L)
{
	auto path = getPath(L);
	lua_createtable(L, 0, 0);

	int index = 1;
	for (auto& p : std::filesystem::directory_iterator(path))
	{
		auto subpath = p.path().string().substr(luaApiRuntimeState.userContentApiRootDirectory.string().size() + 1);
		lua_pushinteger(L, index++);
		lua_pushstring(L, subpath.c_str());
		lua_settable(L, -3);
	}

	return 1;
}

int coal_makefolder(lua_State* L)
{
	auto path = getPath(L);
	std::filesystem::create_directories(path);
	return 0;
}

int coal_delfolder(lua_State* L)
{
	auto path = getPath(L);
	if (!std::filesystem::remove_all(path))
		luaL_errorL(L, "folder does not exist");

	return 0;
}

int coal_delfile(lua_State* L)
{
	auto path = getPath(L);
	if (!std::filesystem::remove(path))
		luaL_errorL(L, "file does not exist");

	return 0;
}

int coal_appendfile(lua_State* L)
{
	auto path = getPath(L);

	size_t contentSize;
	auto contentCString = luaL_checklstring(L, 2, &contentSize);

	if (!isExtensionAllowed(path))
		luaL_errorL(L, "forbidden extension");

	if (!std::filesystem::exists(path))
		luaL_errorL(L, "file does not exist");

	std::ofstream out;
	out.open(path, std::ios_base::app | std::ios_base::binary);
	out.write(contentCString, contentSize);
	out.close();

	return 0;
}
