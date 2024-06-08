#include "Offsets.h"
#include "../Common/Windows.h"
#include "../Common/Exception.h"

#include "../Common/Luau/Luau.h"
#include "../Common/Luau/Riblix.h"

import <iostream>;
import <fstream>;

Offsets::Offsets()
{
	luaApiAddresses = &::luaApiAddresses;
	riblixAddresses = &::riblixAddresses;
}

void Offsets::initAddressesFromFile(const std::wstring& dumpPath)
{
	auto map = parseAddressDumpFile(dumpPath);

	auto get = [&](const auto& name) -> uintptr_t {
		auto pos = map.find(name);
		if (pos == map.end())
		{
			std::cout << "missing " << name << '\n';
			return invalidAddress;
		}
		else
			return pos->second;
	};

	const uintptr_t currentModule = (uintptr_t)GetModuleHandleW(NULL);
#define setLuaAddr(item) luaApiAddresses->item = (decltype(luaApiAddresses->item))((void*)(currentModule + get(#item)))
#define setRiblixAddr(item) riblixAddresses->item = (decltype(riblixAddresses->item))((void*)(currentModule + get(#item)))

	setRiblixAddr(InstanceBridge_pushshared);

	setRiblixAddr(getCurrentContext);

	setLuaAddr(luaO_nilobject);
	setLuaAddr(lua_rawget);
	setLuaAddr(lua_rawset);
	setLuaAddr(lua_next);

	// TODO: unimplemented loadfile
	//setLuaAddr(lua_pcall);
	setLuaAddr(luaD_call);

	setLuaAddr(lua_getinfo);

	setLuaAddr(luaL_typeerrorL);
	setLuaAddr(luaL_errorL);
	setLuaAddr(luaL_typename);

	setLuaAddr(lua_pushlstring);
	setLuaAddr(lua_pushvalue);

	setLuaAddr(lua_tolstring);

	setLuaAddr(lua_settable);
	setLuaAddr(lua_getfield);
	setLuaAddr(lua_setfield);

	setLuaAddr(lua_createtable);
	setLuaAddr(luaH_clone);
	setLuaAddr(lua_setmetatable);
	setLuaAddr(lua_getmetatable);

	setLuaAddr(lua_pushcclosurek);
	setLuaAddr(luaF_newLclosure);
	setLuaAddr(luaF_newproto);
	
	setLuaAddr(luaD_reallocCI);
	setLuaAddr(luaD_growCI);
	setLuaAddr(lua_concat);
	
#undef setLuaAddr
#undef setRiblixAddr

	validateAddresses();
}

std::map<std::string, uintptr_t> Offsets::parseAddressDumpFile(const std::wstring& fileName)
{
	std::map<std::string, uintptr_t> result;

	std::ifstream file(fileName);

	std::string line;
	while (std::getline(file, line)) {
		size_t pos = line.find('=');
		if (pos != std::string::npos) {
			std::string name = line.substr(0, pos);
			std::string address = line.substr(pos + 1);

			try
			{
				result[name] = std::stoul(address, nullptr, 16);
			}
			catch (const std::exception& exception)
			{
				std::cout << defaultFormatter.format("failed to parse", line, ':', exception.what()) << std::endl;
			}
		}
	}

	return result;
}

void Offsets::validateAddresses()
{
	auto validateCategory = [&](auto& addresses) {
		size_t numAddresses = sizeof(addresses) / sizeof(void*);
		void** addressArray = reinterpret_cast<void**>(&addresses);

		for (int i = 0; i < numAddresses; i++)
			if (addressArray[i] == nullptr)
				std::cout << defaultFormatter.format("unhandled address in Offsets at index", i) << std::endl;
		};

	validateCategory(*luaApiAddresses);
	validateCategory(*riblixAddresses);
}
