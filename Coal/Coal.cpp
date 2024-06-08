#include "../Common/Formatter.h"
#include "../Common/Windows.h"
#include "../Common/Pipes.h"
#include "../Common/Luau/Luau.h"
#include "../Common/Riblix.h"

#include "LuaEnv.h"
#include "GlobalSettings.h"
#include "Offsets.h"

#include "Console.h"
#include "FunctionMarker.h"
#include "HookHandler.h"

import <fstream>;
import <thread>;
import <vector>;
import <map>;
import <filesystem>;
import <iostream>;

class Runner
{
public:
	void run();
	void loadInitialData();
	Console console{ "coal" };
private:
	void onInitDataLoaded();
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
	if (globalSettings.showStateAddressTip)
	{
		std::cout << "run this snippet to see state address in console:\n"
			"\nprint({[coroutine.running()]=1})\n"
			"while not getreg do wait() end\n"
			"\n(second line prevents possible gcing of thread until functions would be added)\n"
			<< std::endl;
	}

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

		luaApiRuntimeState.injectEnvironment(targetState);
	}
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

	auto settingsPath = readwstring();
	globalSettings.init(settingsPath);

	auto dumpPath = readwstring();
	offsets.initAddressesFromFile(dumpPath);

	luaApiRuntimeState.setLuaSettings(&globalSettings.luaApiSettings);
	luaApiRuntimeState.userContentApiRootDirectory = readwstring();

	pipe.close();

	onInitDataLoaded();
}

void Runner::onInitDataLoaded()
{
	hookHandler.setupAll();
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

