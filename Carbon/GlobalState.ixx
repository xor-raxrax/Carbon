module;
#include "../Common/CarbonWindows.h"
export module GlobalState;

import Pipes;
import LuaEnv;
import Console;
import GlobalSettings;
import TaskList;
import FunctionMarker;
import Formatter;
import DataModelWatcher;

class NamedPipeClientCommon : public NamedPipeClient
{
public:
	NamedPipeClientCommon(const std::string& name)
		: NamedPipeClient(name)
	{

	}

	void readPipeData()
	{
		auto reader = makeReadBuffer();
		handlePacket(reader.getOp(), reader);
	}

	virtual void handlePacket(PipeOp op, PipeReadBuffer reader) = 0;
};

class CommonPipe : public NamedPipeClient
{
public:
	CommonPipe(const std::string& name)
		: NamedPipeClient(name)
	{

	}

	void readPipeData()
	{
		auto reader = makeReadBuffer();
		switch (reader.getOp())
		{
		case PipeOp::Nop:
		default:
			Console::getInstance() << "dropping pipe data, op: " << (uint8_t)reader.getOp() << std::endl;
		}
	}
};

class PrivatePipe : public NamedPipeClient
{
public:
	enum Direction
	{
		ToClient,
		ToServer,
	};

	PrivatePipe(const std::string& name, Direction direction)
		: NamedPipeClient(name + (direction == Direction::ToServer ? "ToServer" : "ToClient"))
	{

	}

	void readPipeData()
	{
		Console::getInstance() << "reading " << name << std::endl;
		auto reader = makeReadBuffer();
		Console::getInstance() << "handling op " << (uint8_t)reader.getOp() << std::endl;
		switch (reader.getOp())
		{
		case PipeOp::RunScript:
		{
			uintptr_t stateAddress = reader.readU64();
			auto size = reader.readU64();
			auto string = reader.readArray<char>(size);
			auto code = std::string(string, size);

			auto info = dataModelWatcher.getStateByAddress(stateAddress);
			if (!info)
			{
				Console::getInstance() << "invalid state passed on RunScript " << (void*)stateAddress << std::endl;
				break;
			}

			luaApiRuntimeState.runScript(info, code);
			break;
		}
		default:
			Console::getInstance() << "dropping pipe data, op: " << (uint8_t)reader.getOp() << std::endl;
		}
	}
};

export class GlobalState
{
public:

	GlobalState();

	void init(HMODULE ghModule, const std::wstring& settingsPath, const std::wstring& userDirectory);
	void startPipesReading();
	PipeWriteBuffer createCommonWriteBuffer(PipeOp op);
	PipeWriteBuffer createPrivateWriteBuffer(PipeOp op);

private:

	HMODULE ghModule;
	TaskListProcessor taskListProcessor;
	CommonPipe commonPipe;
	PrivatePipe toServerPipe;
	PrivatePipe toClientPipe;
};

export inline GlobalState globalState;

GlobalState::GlobalState()
	: commonPipe(commonPipeName)
	, toServerPipe(privatePipeNamePrefix + std::to_string(GetCurrentProcessId()), PrivatePipe::Direction::ToServer)
	, toClientPipe(privatePipeNamePrefix + std::to_string(GetCurrentProcessId()), PrivatePipe::Direction::ToClient)
{

}

void GlobalState::init(HMODULE _ghModule, const std::wstring& settingsPath, const std::wstring& userDirectory)
{
	ghModule = _ghModule;

	if (!commonPipe.connect())
		Console::getInstance() << "failed to connect to" << commonPipe.getName()
			<< defaultFormatter.format(formatLastError()) << std::endl;

	globalSettings.init(settingsPath);

	luaApiRuntimeState.setLuaSettings(&globalSettings.luaApiSettings);
	luaApiRuntimeState.userContentApiRootDirectory = userDirectory;
	functionMarker = new FunctionMarker(ghModule);
}

void GlobalState::startPipesReading()
{
	std::thread([&]() {

		try
		{
			if (!toServerPipe.connect())
			{
				Console::getInstance() << "failed to connect to" << toServerPipe.getName()
					<< defaultFormatter.format(formatLastError()) << std::endl;
			}

			if (!toClientPipe.connect())
			{
				Console::getInstance() << "failed to connect to" << toClientPipe.getName()
					<< defaultFormatter.format(formatLastError()) << std::endl;
				return;
			}

			while (true)
				toClientPipe.readPipeData();
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

	auto writer = createCommonWriteBuffer(PipeOp::ProcessConnected);
	writer.writeU32(GetCurrentProcessId());
	writer.send();

	while (true)
		commonPipe.readPipeData();
}

PipeWriteBuffer GlobalState::createCommonWriteBuffer(PipeOp op)
{
	return commonPipe.makeWriteBuffer(op);
}

PipeWriteBuffer GlobalState::createPrivateWriteBuffer(PipeOp op)
{
	return toServerPipe.makeWriteBuffer(op);
}