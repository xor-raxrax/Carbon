#include "../Common/Formatter.h"
#include "../Common/Windows.h"
#include "../Common/Exception.h"

#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
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
import RiblixStructures;
import libs.closurelib;
import TaskList;
import DataModelWatcher;

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

lua_State* lua_newstate_hook(void* allocator, void* userdata)
{
	auto original = hookHandler.getHook(HookId::lua_newstate).getOriginal();
	auto result = reinterpret_cast<decltype(luaApiAddresses.lua_newstate)>(original)(allocator, userdata);

	dataModelWatcher.stateWatcher.onGlobalStateCreated(result);

	return result;
}

void flog1_hook(void* junk, const char* formatString, void* object)
{

	if (!strcmp(formatString, "[FLog::CloseDataModel] doCloseDataModel - %p"))
	{
		dataModelWatcher.onDataModelClosing((DataModel*)object);
	}

	auto original = hookHandler.getHook(HookId::FLOG1).getOriginal();
	reinterpret_cast<decltype(riblixAddresses.FLOG1)>(original)(junk, formatString, object);
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

		hookHandler.getHook(HookId::growCI)
			.setTarget(luaApiAddresses.luaD_growCI)
			.setHook(luaD_growCI_hook)
			.setup();
		

		hookHandler.getHook(HookId::FLOG1)
			.setTarget(riblixAddresses.FLOG1)
			.setHook(flog1_hook)
			.setup();

		taskListProcessor.createRunThread();

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

std::string getStackTrace(CONTEXT* context)
{
	auto process = GetCurrentProcess();
	auto thread = GetCurrentThread();

	SymInitialize(process, NULL, TRUE);

	STACKFRAME64 stackFrame;
	memset(&stackFrame, 0, sizeof(STACKFRAME64));

	stackFrame.AddrPC.Offset = context->Rip;
	stackFrame.AddrPC.Mode = AddrModeFlat;
	stackFrame.AddrFrame.Offset = context->Rbp;
	stackFrame.AddrFrame.Mode = AddrModeFlat;
	stackFrame.AddrStack.Offset = context->Rsp;
	stackFrame.AddrStack.Mode = AddrModeFlat;

	std::ostringstream result;
	result << "call Stack:\n";

	int frameIndex = 0;
	while (StackWalk64(
		IMAGE_FILE_MACHINE_AMD64,
		process,
		thread,
		&stackFrame,
		context,
		0,
		SymFunctionTableAccess64,
		SymGetModuleBase64,
		0
	))
	{
		uintptr_t address = stackFrame.AddrPC.Offset;
		if (!address)
			break;

		const int nameMaxSize = 40;
		char buffer[sizeof(SYMBOL_INFO) + nameMaxSize * sizeof(TCHAR)];
		PSYMBOL_INFO symbol = (PSYMBOL_INFO)buffer;
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
		symbol->MaxNameLen = nameMaxSize;

		result << '[' << frameIndex++ << "] ";

		DWORD64 displacement = 0;
		if (SymFromAddr(process, address, &displacement, symbol))
		{
			std::string_view symbolName(symbol->Name, nameMaxSize);
			result << symbolName << " at " << (void*)symbol->Address;

			IMAGEHLP_LINE64 line;
			DWORD lineDisplacement = 0;
			if (SymGetLineFromAddr64(process, address, &lineDisplacement, &line))
			{

				std::string_view fullPath = line.FileName;
				std::string_view shortPath = fullPath;

				// qwe\asd\ { Carbon\Main.cpp } ";
				size_t lastBackslashPos = fullPath.rfind('\\');
				if (lastBackslashPos != std::string::npos)
				{
					size_t secondLastBackslashPos = fullPath.rfind('\\', lastBackslashPos - 1);
					shortPath = fullPath.substr(secondLastBackslashPos + 1);
				}

				result << " in " << shortPath << " line " << line.LineNumber << " + " << lineDisplacement;
			}

		}
		else
			result << "unknown at " << (void*)address;

		result << '\n';
	}

	SymCleanup(process);
	return result.str();
}

LONG panic(_EXCEPTION_POINTERS* ep)
{
	std::string result = "AAAAAAAAAAAAAAA PANIC AAAAAAAAAAAAAAAA\n";
	result.reserve(1000);
	result += "ExceptionCode: " + defaultFormatter.format(ep->ExceptionRecord->ExceptionCode) + '\n';
	result += "ExceptionFlags: " + defaultFormatter.format(ep->ExceptionRecord->ExceptionFlags) + '\n';
	result += "ExceptionAddress: " + Formatter::pointerToString(ep->ExceptionRecord->ExceptionAddress) + '\n';

	result += "Rip: " + Formatter::pointerToString((void*)ep->ContextRecord->Rax) + '\n';

	result += getStackTrace(ep->ContextRecord);
	Console::getInstance() << result << std::endl;

	Sleep(600'000);
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
				hookHandler.getHook(HookId::lua_getfield)
					.setTarget(luaApiAddresses.lua_getfield)
					.setHook(lua_getfield_Hook)
					.setup();

				hookHandler.getHook(HookId::lua_newstate)
					.setTarget(luaApiAddresses.lua_newstate)
					.setHook(lua_newstate_hook)
					.setup();
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

