#include "../Common/Exception.h"
#include "../Common/Windows.h"
#include "../Common/Pipes.h"

import <iostream>;
import <string>;
import <filesystem>;

class Circus
{
public:
	DWORD getProcessId()
	{
		auto processId = ::getProcessId(L"RobloxStudioBeta.exe");
		if (!processId)
			raise("process not found");
		return processId;
	}

	void checkDll()
	{
		if (!std::filesystem::exists(dllPath))
			raise("missing dll at path", std::filesystem::absolute(dllPath));
	}

	void checkDump()
	{
		if (!std::filesystem::exists(dumpPath))
		{
			std::cout << "missing dump file, creating new one\n";

			if (!std::filesystem::exists(dumperPath))
				raise("missing dumper at path", std::filesystem::absolute(dumperPath));

			std::string command = dumperPath.string() + " " + dumpPath.string();

			if (std::system(command.c_str()))
				raise("AddressDumper is dead XD");
		}
	}

	void checkUserDirectory()
	{
		if (!std::filesystem::exists(userDirectoryPath))
			std::filesystem::create_directory(userDirectoryPath);
	}

	std::filesystem::path getDllPath()
	{
		return dllPath;
	}

	std::filesystem::path getDumpPath()
	{
		return dumpPath;
	}

	std::filesystem::path getUserDirectoryPath()
	{
		return userDirectoryPath;
	}

	std::filesystem::path getSettingsPath()
	{
		return settingsPath;
	}

	bool terminateCrashHandler()
	{
		auto id = ::getProcessId(L"RobloxCrashHandler.exe");
		if (!id)
			return false;

		HandleScope process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, id);
		if (process == INVALID_HANDLE_VALUE)
			throw std::exception("failed to terminate crash handler");

		TerminateProcess(process, 0);
		return true;
	}

private:
	// WorkPLACE XFDDDDDDDDDDD
	const std::filesystem::path userDirectoryPath = "Workplace";
	const std::filesystem::path dumpPath = "dumpresult.txt";
	const std::filesystem::path dllPath = "Coal.dll";
	const std::filesystem::path dumperPath = "AddressDumper.exe";
	const std::filesystem::path settingsPath = "Settings.cfg";
};

Circus circus;

class Injector
{
public:

	bool inject(const std::filesystem::path& dllPath, DWORD processId)
	{
		auto absoluteDllPath = std::filesystem::absolute(dllPath);
		auto absoluteDllPathString = absoluteDllPath.string();

		HandleScope process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
		if (process == INVALID_HANDLE_VALUE)
			raise("failed to open process");

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
};

int main()
{
	try
	{
		circus.terminateCrashHandler();

		circus.checkUserDirectory();
		auto process = circus.getProcessId();
		circus.checkDll();
		circus.checkDump();

		NamedPipeServer server;
		Injector injector;

		if (injector.inject(circus.getDllPath(), process))
		{
			if (!server.create())
				throw std::exception("failed to create server pipe");

			if (!server.waitForClient())
				throw std::exception("where is my kid");

			auto writer = server.makeWriteBuffer();

			auto writePath = [&](std::filesystem::path path)
			{
				auto absolute = std::filesystem::absolute(path);
				writer.writeU64(absolute.native().size());
				writer.writeArray(absolute.native().c_str(), absolute.native().size());
			};

			writePath(circus.getSettingsPath());
			writePath(circus.getDumpPath());
			writePath(circus.getUserDirectoryPath());

			writer.send();
		}
	}
	catch (const std::exception& exception)
	{
		std::cout << exception.what() << std::endl;
	}

	return 0;
}