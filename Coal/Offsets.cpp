#include "Offsets.h"
#include "../Common/Windows.h"
#include "../Common/Exception.h"

#include "../Common/Luau/Luau.h"
#include "../Common/Luau/Riblix.h"
#include "../Common/Utils.h"

import <iostream>;
import <fstream>;
import <filesystem>;

Offsets::Offsets()
{
	luaApiAddresses = &::luaApiAddresses;
	riblixAddresses = &::riblixAddresses;
}

void Offsets::initAddressesFromFile(const std::wstring& dumpPath, const std::wstring& dumperPath)
{
	auto map = parseAddressDumpFile(dumpPath);

	if (checkDifferentValue(map))
	{
		std::cout << "redoing dump info\n";

		STARTUPINFO startupInfo;
		PROCESS_INFORMATION processInfo;

		ZeroMemory(&startupInfo, sizeof(startupInfo));
		startupInfo.cb = sizeof(startupInfo);
		ZeroMemory(&processInfo, sizeof(processInfo));

		std::filesystem::path dumpFilePath(dumpPath);
		std::wstring directory = dumpFilePath.parent_path().wstring();
		std::string fileName = dumpFilePath.filename().string();
		std::wstring commandLine = L"\"" + dumperPath + L"\" " + std::wstring(fileName.begin(), fileName.end());

		if (CreateProcessW(
			nullptr,               // path to the executable
			const_cast<wchar_t*>(commandLine.c_str()),  // command line arguments (NULL if none)
			nullptr,               // process security attributes (NULL for default)
			nullptr,               // thread security attributes (NULL for default)
			false,                 // handle inheritance option
			0,                     // creation flags
			nullptr,               // environment block (NULL to use the parent's environment)
			directory.c_str(),     // current directory
			&startupInfo,          // pointer to STARTUPINFO structure
			&processInfo           // pointer to PROCESS_INFORMATION structure
		))
		{
			WaitForSingleObject(processInfo.hProcess, INFINITE);

			CloseHandle(processInfo.hProcess);
			CloseHandle(processInfo.hThread);

			map = parseAddressDumpFile(dumpPath);

			if (checkDifferentValue(map))
				std::cout << "still has different value for some reason" << std::endl;
		}
		else
		{
			// notice: format message does not apply arguments
			DWORD errorId = GetLastError();
			LPWSTR messageBuffer = nullptr;
			size_t size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL, errorId, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&messageBuffer, 0, NULL);

			std::wstring message(messageBuffer, size);

			LocalFree(messageBuffer);

			raise("failed to start process:", tostring(message));
		}
	}

	auto get = [&](const auto& name) -> uintptr_t {
		auto pos = map.find(name);
		if (pos == map.end())
		{
			std::cout << "missing " << name << '\n';
			return invalidAddress;
		}
		else
			return pos->second.address;
	};

#define setLuaAddr(item) luaApiAddresses->item = (decltype(luaApiAddresses->item))((void*)getAddressFromOffset(get(#item)))
#define setRiblixAddr(item) riblixAddresses->item = (decltype(riblixAddresses->item))((void*)getAddressFromOffset(get(#item)))

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
	setLuaAddr(lua_newuserdatatagged);
	setLuaAddr(luaH_get);

#undef setLuaAddr
#undef setRiblixAddr

	validateAddresses();
}

uintptr_t Offsets::getAddressFromOffset(uintptr_t offset) const
{
	const static uintptr_t currentModule = (uintptr_t)GetModuleHandleW(NULL);
	return currentModule + offset;
}

Offsets::dumpInfo Offsets::parseAddressDumpFile(const std::wstring& fileName)
{
	dumpInfo result;

	std::ifstream file(fileName);

	std::string line;
	while (std::getline(file, line)) {
		size_t posAddressDelim = line.find('=');
		size_t posValueDelim = line.find('|');
		if (posAddressDelim != std::string::npos && posValueDelim != std::string::npos) {
			std::string name = line.substr(0, posAddressDelim);
			std::string parsedAddress = line.substr(posAddressDelim + 1, posValueDelim);
			std::string parsedValue = line.substr(posValueDelim + 1);

			try
			{
				uintptr_t address = std::stoull(parsedAddress, nullptr, 16);
				uintptr_t storedValue = strtoull(parsedValue.c_str(), nullptr, 16);

				result[name] = { address, storedValue };
			}
			catch (const std::exception& exception)
			{
				std::cout << defaultFormatter.format("failed to parse", line, ':', exception.what()) << std::endl;
			}
		}
	}

	file.close();

	return result;
}

// returns true if theres difference
bool Offsets::checkDifferentValue(const dumpInfo& info)
{
	bool hasDifference = false;
	for (auto& [name, offset] : info)
	{
		uintptr_t value = *(size_t*)getAddressFromOffset(offset.address);
		if (value != offset.value)
		{
			std::cout << defaultFormatter.format(name, "differs, stored:", offset.value, "actual:", value) << std::endl;
			hasDifference = true;
			break;
		}
	}

	return hasDifference;
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
