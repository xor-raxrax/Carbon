#include "../Common/Exception.h"
#include "../Common/Windows.h"

import <iostream>;
import <string>;
import <filesystem>;
import <fstream>;
import <map>;
import Pipes;
import StringUtils;
import LuauOffsets;

bool terminateCrashHandler()
{
	auto id = ::getProcessId(L"RobloxCrashHandler.exe");
	if (!id)
		return false;

	HandleScope process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, id);
	if (process == INVALID_HANDLE_VALUE)
		raise("failed to terminate crash handler");

	TerminateProcess(process, 0);
	return true;
}

bool inject(const std::filesystem::path& dllPath, HandleScope& process)
{
	auto absoluteDllPath = std::filesystem::absolute(dllPath);
	auto absoluteDllPathString = absoluteDllPath.string();

	auto remoteMemory = VirtualAllocEx(
		process,
		NULL,
		absoluteDllPathString.size(),
		MEM_COMMIT | MEM_RESERVE,
		PAGE_READWRITE
	);

	if (!remoteMemory)
		raise("failed to allocate memory");

	std::cout << "allocation address: " << remoteMemory << std::endl;

	BOOL writeResult = WriteProcessMemory(
		process,
		remoteMemory,
		(LPVOID)absoluteDllPathString.c_str(),
		absoluteDllPathString.size(),
		NULL
	);

	if (!writeResult)
		raise("failed to write process memory");

	HandleScope remoteThread = CreateRemoteThread(
		process,
		NULL,
		0,
		(LPTHREAD_START_ROUTINE)LoadLibraryA,
		remoteMemory,
		0,
		NULL
	);

	if (!remoteThread)
		raise("failed to create remote thread");

	WaitForSingleObject(remoteThread, INFINITE);
	VirtualFreeEx(process, remoteMemory, 0, MEM_RELEASE);

	return true;
}

NamedPipeServer& getServer()
{
	static NamedPipeServer server(commonPipeName);
	return server;
}

class DumpValidator
{
public:
	DumpValidator(HandleScope& process, void* moduleBaseAddress)
		: process(process)
		, moduleBaseAddress((uintptr_t)moduleBaseAddress)
	{

	}

	uintptr_t getAddressFromOffset(uintptr_t offset)
	{
		return moduleBaseAddress + offset;
	}

	struct OffsetInfo
	{
		uintptr_t address;
		uintptr_t value;
	};

	using dumpInfo = std::map<std::string, OffsetInfo>;

	dumpInfo parseAddressDumpFile(const std::wstring& filePath)
	{
		dumpInfo result;

		std::ifstream file(filePath);

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

	void initAddressesFromFile(const std::wstring& dumpPath, const std::wstring& dumperPath)
	{
		auto map = parseAddressDumpFile(dumpPath);

		if (checkDifferentValue(map))
		{
			redidDump = true;
			redoDump(map, dumpPath, dumperPath);
		}

		bool hasMissingAddress = false;

		auto get = [&](const auto& name) -> uintptr_t {
			auto pos = map.find(name);
			if (pos == map.end())
			{
				std::cout << "missing " << name << std::endl;
				hasMissingAddress = true;
				return invalidAddress;
			}
			else
				return pos->second.address;
		};

	addressInit:

#define setLuaAddr(item) luaApiAddresses.item = (decltype(luaApiAddresses.item))((void*)getAddressFromOffset(get(#item)))
#define setRiblixAddr(item) riblixAddresses.item = (decltype(riblixAddresses.item))((void*)getAddressFromOffset(get(#item)))

		setRiblixAddr(InstanceBridge_pushshared);

		setRiblixAddr(getCurrentContext);
		setRiblixAddr(luau_load);

		setLuaAddr(luaO_nilobject);
		setLuaAddr(lua_rawget);
		setLuaAddr(lua_rawset);
		setLuaAddr(lua_next);

		setLuaAddr(luaD_call);
		setLuaAddr(luaD_pcall);

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
		setLuaAddr(lua_newthread);

#undef setLuaAddr
#undef setRiblixAddr

		if (hasMissingAddress)
		{
			if (redidDump)
				std::cout << "did dump and missing values -> unapdated dumper?" << std::endl;
			else
			{
				redoDump(map, dumpPath, dumperPath);
				goto addressInit;
			}
		}

		validateAddresses();
	}

private:

	void redoDump(dumpInfo& result, const std::wstring& dumpPath, const std::wstring& dumperPath)
	{
		redidDump = true;
		std::cout << "redoing dump info" << std::endl;

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

			result = parseAddressDumpFile(dumpPath);

			if (checkDifferentValue(result))
				std::cout << "still has different value for some reason" << std::endl;
		}
		else
		{
			raise("failed to start process:", formatLastError());
		}
	}

	// returns true if theres difference
	bool checkDifferentValue(const dumpInfo& info)
	{
		bool hasDifference = false;
		for (auto& [name, offset] : info)
		{
			uintptr_t value;
			if (!ReadProcessMemory(process,
				(void*)getAddressFromOffset(offset.address),
				&value,
				sizeof(value),
				nullptr
			))

			if (value != offset.value)
			{
				std::cout << defaultFormatter.format(name, "differs, stored:", offset.value, "actual:", value) << std::endl;
				hasDifference = true;
				break;
			}
		}

		return hasDifference;
	}

	void validateAddresses()
	{
		auto validateCategory = [&](auto& addresses) {
			size_t numAddresses = sizeof(addresses) / sizeof(void*);
			void** addressArray = reinterpret_cast<void**>(&addresses);

			for (int i = 0; i < numAddresses; i++)
				if (addressArray[i] == nullptr)
					std::cout << defaultFormatter.format("unhandled address in Offsets at index", i) << std::endl;
			};

		validateCategory(luaApiAddresses);
		validateCategory(riblixAddresses);
	}

	HandleScope& process;
	uintptr_t moduleBaseAddress;
	bool redidDump = false;
	static const uintptr_t invalidAddress = 1;
};

#define API __declspec(dllexport)

extern "C" API const char* Inject(size_t dataSize, const char* paths);
extern "C" API void SendScript(size_t size, const char* source);
extern "C" API void InjectEnvironment(uintptr_t stateAddress);

static bool injected = false;

static HandleScope sharedMemoryMapFile;

void createOffsetsSharedMemory()
{
	SharedMemoryOffsets sharedData;
	sharedData.luaApiAddresses = luaApiAddresses;
	sharedData.riblixAddresses = riblixAddresses;

	sharedMemoryMapFile.assign(CreateFileMappingW(
		INVALID_HANDLE_VALUE,
		0,
		PAGE_READWRITE,
		0,
		sizeof(SharedMemoryOffsets),
		sharedMemoryName
	));

	if (!sharedMemoryMapFile)
		raise("failed to create shared memory", formatLastError());

	auto sharedMemory = (SharedMemoryOffsets*)MapViewOfFile(
		sharedMemoryMapFile,
		FILE_MAP_ALL_ACCESS,
		0,
		0,
		sizeof(SharedMemoryOffsets)
	);

	if (!sharedMemory)
		raise("failed to map view shared memory", formatLastError());

	*sharedMemory = sharedData;
}

const char* Inject(size_t dataSize, const char* paths)
{
	try
	{
		if (injected)
			raise("already injected");

		terminateCrashHandler();

		ReadBuffer reader(std::string(paths, dataSize));

		auto readwstring = [&]() {
			auto size = reader.readU64();
			auto string = reader.readArray<char>(size);
			return std::wstring(reinterpret_cast<wchar_t*>(string), size / 2);
		};
		
		// keep in sync with TryInject()
		auto dllPath = readwstring();
		auto settingsPath = readwstring();
		auto dumpPath = readwstring();
		auto dumperPath = readwstring();
		auto userDirectoryPath = readwstring();

		std::wstring processName = L"RobloxStudioBeta.exe";
		auto processId = getProcessId(processName);
		if (!processId)
			raise("process not found");

		HandleScope process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
		if (process == INVALID_HANDLE_VALUE)
			raise("failed to open process");

		DumpValidator dumpValidator(process, getFirstModule(processId, processName).modBaseAddr);
		dumpValidator.initAddressesFromFile(dumpPath, dumperPath);

		createOffsetsSharedMemory();

		auto& server = getServer();

		if (!server.create())
			raise("failed to create server pipe");

		if (!inject(dllPath, process))
			raise("failed to inject");

		if (!server.waitForClient())
			raise("client wait timeout");

		auto writer = server.makeWriteBuffer(PipeOp::Init);

		auto writePath = [&](const std::wstring& path)
		{
			writer.writeU64(path.size());
			writer.writeArray(path.c_str(), path.size());
		};

		// keep in sync with Runner::loadInitialData()
		writePath(settingsPath);
		writePath(dumpPath);
		writePath(dumperPath);
		writePath(userDirectoryPath);

		writer.send();
		injected = true;
		return nullptr;
	}
	catch (const std::exception& exception)
	{
		static std::string errorMessage;
		errorMessage.assign(exception.what());
		return errorMessage.c_str();
	}
}

void SendScript(size_t size, const char* source)
{
	auto writer = getServer().makeWriteBuffer(PipeOp::RunScript);
	writer.writeU64(size);
	writer.writeArray<char>(source, size);
	writer.send();
}

void InjectEnvironment(uintptr_t stateAddress)
{
	auto writer = getServer().makeWriteBuffer(PipeOp::InjectEnvironment);
	writer.writeU64(stateAddress);
	writer.send();
}