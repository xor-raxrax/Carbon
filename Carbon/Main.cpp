#include "../Common/Formatter.h"
#include "../Common/Windows.h"
#include "../Common/Exception.h"

import <fstream>;
import <thread>;
import <vector>;
import <map>;
import <filesystem>;
import <mutex>;

import Luau;
import Console;
import FunctionMarker;
import GlobalSettings;
import LuaEnv;
import HookHandler;
import Pipes;
import LuauOffsets;
import StringUtils;

class Runner
{
public:
	Runner();
	void readPipeData();
	void loadInitialData();
private:
	NamedPipeClient pipe;
};

Runner::Runner()
	: pipe(commonPipeName)
{

}

void Runner::readPipeData()
{
	auto reader = pipe.makeReadBuffer();
	switch (reader.getOp())
	{
	case PipeOp::RunScript:
	{
		auto size = reader.readU64();
		auto string = reader.readArray<char>(size);
		auto code = std::string(string, size);
		luaApiRuntimeState.runScript(code);
		break;
	}
	case PipeOp::InjectEnvironment:
	{
		auto address = reader.readU64();
		auto targetState = (lua_State*)(address);
		luaApiRuntimeState.injectEnvironment(targetState);
		break;
	}
	default:
		Console::getInstance() << "dropping pipe data, op: " << (uint8_t)reader.getOp() << std::endl;
	}
}

void Runner::loadInitialData()
{
	if (!pipe.connect())
	{
		Console::getInstance() << "failed to connect to server" << std::endl;
		return;
	}

	auto reader = pipe.makeReadBuffer();

	auto readwstring = [&]() {
		auto size = reader.readU64();
		auto string = reader.readArray<wchar_t>(size);
		return std::wstring(string, size);
	};

	// keep in sync with Inject()
	auto settingsPath = readwstring();
	auto userDirectory = readwstring();

	globalSettings.init(settingsPath);

	luaApiRuntimeState.setLuaSettings(&globalSettings.luaApiSettings);
	luaApiRuntimeState.userContentApiRootDirectory = userDirectory;
}

HMODULE ghModule;
std::mutex mainInitMutex;
bool mainThreadCreated = false;
Runner runner;

void realMain()
{
	try
	{
		hookHandler.getHook(HookId::lua_getfield).remove();
		runner.loadInitialData();
		functionMarker = new FunctionMarker(ghModule);
		hookHandler.getHook(HookId::growCI).setTarget(luaApiAddresses.luaD_growCI).setup();

		while (true)
			runner.readPipeData();
	}
	catch (lua_exception& e)
	{
		Console::getInstance() << e.what() << std::endl;
	}
	catch (std::exception& e)
	{
		Console::getInstance() << e.what() << std::endl;
	}
	catch (...)
	{
		Console::getInstance() << "caught something bad" << std::endl;
	}
}

int lua_getfield_Hook(lua_State* L, int idx, const char* k)
{
	std::lock_guard<std::mutex> guard(mainInitMutex);
	if (!mainThreadCreated)
	{
		mainThreadCreated = true;
		std::thread customThread(realMain);
		customThread.detach();
	}
	auto original = hookHandler.getHook(HookId::lua_getfield).getOriginal();
	return reinterpret_cast<decltype(luaApiAddresses.lua_getfield)>(original)(L, idx, k);
}

void loadOffsets()
{
	HandleScope mapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, sharedMemoryName);
	if (!mapFile)
		raise("failed to open shared memory", formatLastError());

	auto offsets = (SharedMemoryOffsets*)MapViewOfFile(mapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedMemoryOffsets));
	if (!offsets)
		raise("failed map view shared memory", formatLastError());

	luaApiAddresses = offsets->luaApiAddresses;
	riblixAddresses = offsets->riblixAddresses;
}

LONG panic(_EXCEPTION_POINTERS* ep)
{
	std::string result = "AAAAAAAAAAAAAAA PANIC AAAAAAAAAAAAAAAA\n";
	result.reserve(200);
	result += "ExceptionCode: " + defaultFormatter.format(ep->ExceptionRecord->ExceptionCode) + '\n';
	result += "ExceptionFlags: " + defaultFormatter.format(ep->ExceptionRecord->ExceptionFlags) + '\n';
	result += "ExceptionAddress: " + Formatter::pointerToString(ep->ExceptionRecord->ExceptionAddress) + '\n';

	result += "Rip: " + Formatter::pointerToString((void*)ep->ContextRecord->Rax) + '\n';

	Console::getInstance() << result << std::endl;

	abort();
	return EXCEPTION_EXECUTE_HANDLER;
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:

		ghModule = hModule;
		std::thread([hModule]() {
			try
			{
				SetUnhandledExceptionFilter(panic);
				loadOffsets();
				hookHandler.getHook(HookId::lua_getfield).setTarget(luaApiAddresses.lua_getfield).setup();
			}
			catch (std::exception& e)
			{
				Console::getInstance() << e.what() << std::endl;
			}
			catch (...)
			{
				Console::getInstance() << "caught something bad" << std::endl;
			}
			}).detach();
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

