#include "../Common/Formatter.h"
#include "../Common/Windows.h"
#include "../Common/Pipes.h"
#include "../Common/Luau/Luau.h"
#include "../Common/Riblix.h"

#include "LuaEnv.h"

#include "Console.h"
#include "FunctionMarker.h"

import <fstream>;
import <thread>;
import <vector>;
import <map>;
import <algorithm>;
import <filesystem>;
import <fstream>;
import <iostream>;

class Runner
{
public:
	void run();
	void loadInitialData();
	void initOffsets(const std::wstring& dumpPath);
	Console console{ "coal" };
	std::string currentOperation;
	NamedPipeClient pipe;
};

bool isValidateStatePointer(lua_State* state)
{
	if (!state || (uintptr_t)state < 0xFFFF)
		return false;

	return true;
}

void Runner::run()
{
	while (true)
	{
		std::cout << "input state address:\n  ";

		uintptr_t address;
		console.setHex();
		std::cin.sync();
		std::cin >> std::hex >> address;
		std::cin.clear();
		auto targetState = (lua_State*)(address);

		if (!isValidateStatePointer(targetState))
		{
			std::cout << "passed pointer is not valid state address" << std::endl;
			continue;
		}

		LuaApiRegistrar::run(targetState);
	}
}

std::map<std::string, uintptr_t> parseAddressDumpFile(const std::wstring& fileName)
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

	file.close();
	return result;
}

void Runner::loadInitialData()
{
	if (!pipe.connect())
	{
		std::cout << "failed to connect to server" << std::endl;
		return;
	}

	auto reader = pipe.makeReadBuffer();

	auto readwstring = [&]() {
		auto size = reader.readU64();
		auto string = reader.readArray<wchar_t>(size);
		return std::wstring(string, size);
	};

	auto dumpPath = readwstring();
	initOffsets(dumpPath);
	apiSettings.userContentApiRootDirectory = readwstring();
}

void Runner::initOffsets(const std::wstring& dumpPath)
{
	auto map = parseAddressDumpFile(dumpPath);

	auto get = [&](const auto& name) -> uintptr_t {
		auto pos = map.find(name);
		if (pos == map.end())
		{
			std::cout << "missing " << name << '\n';
			return 0;
		}
		else
			return pos->second;

		};

	const uintptr_t currentModule = (uintptr_t)GetModuleHandleW(NULL);

#define getAddr(function) function = (decltype(function))((void*)(currentModule + get(#function)))

	getAddr(InstanceBridge_pushshared);

	getAddr(getCurrentContext);

	getAddr(luaO_nilobject);
	getAddr(lua_rawget);
	getAddr(lua_rawset);
	getAddr(lua_next);

	// TODO: unimplemented loadfile
	// getAddr(lua_pcall);
	getAddr(luaD_call);

	getAddr(lua_getinfo);

	getAddr(luaL_typeerrorL);
	getAddr(luaL_errorL);
	getAddr(luaL_typename);
	
	getAddr(lua_pushlstring);
	getAddr(lua_pushvalue);

	getAddr(lua_tolstring);

	getAddr(lua_settable);
	getAddr(lua_getfield);
	getAddr(lua_setfield);
	
	getAddr(lua_createtable);
	getAddr(luaH_clone);
	getAddr(lua_setmetatable);
	getAddr(lua_getmetatable);
	
	getAddr(lua_pushcclosurek);
	getAddr(luaF_newLclosure);
	getAddr(luaF_newproto);

#undef getAddr
}


LONG panic(_EXCEPTION_POINTERS* ep)
{
	std::string result = "AAAAAAAAAAAAAAA PANIC AAAAAAAAAAAAAAAA\n";
	result.reserve(200);
	result += "ExceptionCode: " + defaultFormatter.format(ep->ExceptionRecord->ExceptionCode) + '\n';
	result += "ExceptionFlags: " + defaultFormatter.format(ep->ExceptionRecord->ExceptionFlags) + '\n';
	result += "ExceptionAddress: " + Formatter::pointerToString(ep->ExceptionRecord->ExceptionAddress) + '\n';

	result += "Rip: " + Formatter::pointerToString((void*)ep->ContextRecord->Rax) + '\n';

	std::cout << result << std::endl;
	
	abort();
	return EXCEPTION_EXECUTE_HANDLER;
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	SetUnhandledExceptionFilter(panic);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		std::thread([](){
			try
			{
				Runner runner;

				runner.loadInitialData();
				runner.pipe.close();
				functionMarker = new FunctionMarker();
				runner.run();
			}
			catch (lua_exception& e)
			{
				auto what = e.what();
				std::cout << what;
			}
			catch (std::exception& e)
			{
				std::cout << e.what();
			}
			catch (...)
			{
				std::cout << "caught something bad in init";
			}

			std::cout << std::endl;
		}).detach();
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

