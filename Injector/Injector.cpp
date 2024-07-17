#include "../Common/CarbonWindows.h"

import <iostream>;
import <string>;
import <filesystem>;

import Pipes;
import SharedAddresses;
import Exception;
import DumpValidator;

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