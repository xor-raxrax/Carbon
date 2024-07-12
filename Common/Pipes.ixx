module;
#include "Windows.h"
#include "Exception.h"
export module Pipes;

import <string>;
import <stdexcept>;

class NamedPipe;

export enum class PipeOp
{
	Init,
	RunScript,
	InjectEnvironment,
};

export class ReadBuffer
{
public:

	ReadBuffer(const std::string& buffer);

	template <typename T>
	T readTyped()
	{
		if (std::cmp_less(std::distance(bufferReadPos, buffer.end()), sizeof(T)))
			raise("buffer underflow");

		T value;
		std::memcpy(&value, &*bufferReadPos, sizeof(T));
		std::advance(bufferReadPos, sizeof(T));
		return value;
	}

	int8_t readI8() { return readTyped<int8_t>(); }
	uint8_t readU8() { return readTyped<uint8_t>(); }
	int32_t readI32() { return readTyped<int32_t>(); }
	uint32_t readU32() { return readTyped<uint32_t>(); }
	int64_t readI64() { return readTyped<int64_t>(); }
	uint64_t readU64() { return readTyped<uint64_t>(); }

	template <typename T = unsigned char>
	T* readArray(size_t itemCount)
	{
		size_t size = itemCount * sizeof(T);
		if (std::cmp_less(std::distance(bufferReadPos, buffer.end()), size))
			raise("buffer underflow");

		auto result = reinterpret_cast<T*>(&*bufferReadPos);
		std::advance(bufferReadPos, size);
		return result;
	}

protected:
	std::string buffer;
	std::string::iterator bufferReadPos;
};

export class WriteBuffer
{
public:
	template <typename T>
	void writeTyped(T value)
	{
		buffer.append(reinterpret_cast<const char*>(&value), sizeof(T));
	}

	void writeI8(int8_t value) { writeTyped<int8_t>(value); }
	void writeU8(uint8_t value) { writeTyped<uint8_t>(value); }
	void writeI32(int32_t value) { writeTyped<int32_t>(value); }
	void writeU32(uint32_t value) { writeTyped<uint32_t>(value); }
	void writeI64(int64_t value) { writeTyped<int64_t>(value); }
	void writeU64(uint64_t value) { writeTyped<uint64_t>(value); }

	template <typename T = unsigned char>
	void writeArray(const T* value, size_t itemCount)
	{
		buffer.append(reinterpret_cast<const char*>(value), itemCount * sizeof(T));
	}

protected:
	std::string buffer;
};


export class PipeReadBuffer : public ReadBuffer
{
	friend class NamedPipe;
public:
	PipeOp getOp() const { return op; }

private:
	PipeReadBuffer(const std::string& buffer);

	PipeOp op;
};

export
{
	class PipeWriteBuffer : public WriteBuffer
	{
		friend class NamedPipe;
	public:
		void send();
		PipeOp getOp() const { return op; }

	private:
		PipeWriteBuffer(NamedPipe& pipe, PipeOp op);

		NamedPipe& pipe;
		bool didSend = false;
		const PipeOp op;
	};

	class NamedPipe
	{
	public:
		friend class PipeWriteBuffer;

		static const size_t readBufferSize = 1 * 1024;
		static const size_t writeBufferSize = 1 * 1024;

		NamedPipe(const std::string& name);
		PipeReadBuffer makeReadBuffer();
		PipeWriteBuffer makeWriteBuffer(PipeOp op);
		void close();
	protected:
		std::string name;
		HandleScope pipe;

		std::string fetch();
		void send(const std::string& buffer);
	};

	class NamedPipeServer : public NamedPipe
	{
	public:
		NamedPipeServer(const std::string& name);
		bool create();
		bool waitForClient();
	};

	class NamedPipeClient : public NamedPipe
	{
	public:
		NamedPipeClient(const std::string& name);
		bool connect();
	};

	inline const char* const commonPipeName = "carbon_delivery";
	inline const char* const scriptPipeName = "coal_delivery";
}


NamedPipe::NamedPipe(const std::string& name)
	: name("\\\\.\\pipe\\" + name)
{

}

PipeReadBuffer NamedPipe::makeReadBuffer()
{
	return PipeReadBuffer(std::move(fetch()));
}

PipeWriteBuffer NamedPipe::makeWriteBuffer(PipeOp op)
{
	return PipeWriteBuffer(*this, op);
}

void NamedPipe::close()
{
	pipe.close();
}

std::string NamedPipe::fetch()
{
	std::string result;

	char buffer[readBufferSize];

	DWORD chunkSize = 0;
	while (ReadFile(
		pipe,
		&buffer,
		readBufferSize,
		&chunkSize,
		nullptr
	))
	{
		result.append(buffer + 1, chunkSize - 1);
		if (!buffer[0])
			break;
	}

	return result;
}

void NamedPipe::send(const std::string& data)
{
	DWORD totalDataWritten = 0;
	size_t dataTotalSize = data.size();

	if (dataTotalSize == 0)
		raise("buffer is empty");

	char buffer[writeBufferSize];

	const size_t chunkDataMaxSize = writeBufferSize - 1;

	while (totalDataWritten < dataTotalSize) {
		DWORD dataSize = (DWORD)std::min(chunkDataMaxSize, dataTotalSize - totalDataWritten);
		DWORD chunkSize = dataSize + 1;

		bool hasContinuation = totalDataWritten + dataSize < dataTotalSize;
		buffer[0] = hasContinuation;

		memcpy(buffer + 1, data.c_str() + totalDataWritten, dataSize);

		if (!WriteFile(
			pipe,
			buffer,
			chunkSize,
			nullptr,
			nullptr
		))
			raise("something bad happened:", formatLastError());

		totalDataWritten += dataSize;
	}
}

ReadBuffer::ReadBuffer(const std::string& buffer)
	: buffer(buffer)
	, bufferReadPos(this->buffer.begin())
{

}

PipeReadBuffer::PipeReadBuffer(const std::string& buffer)
	: ReadBuffer(buffer)
{
	op = (PipeOp)readU8();
}

PipeWriteBuffer::PipeWriteBuffer(NamedPipe& pipe, PipeOp op)
	: WriteBuffer()
	, pipe(pipe)
	, op(op)
{
	writeU8((uint8_t)op);
}

void PipeWriteBuffer::send()
{
	if (didSend)
		raise("send pipe data twice");

	pipe.send(buffer);
	didSend = true;
}

NamedPipeServer::NamedPipeServer(const std::string& name)
	: NamedPipe(name)
{}

bool NamedPipeServer::create()
{
	pipe.assign(CreateNamedPipeA(
		name.c_str(),
		PIPE_ACCESS_DUPLEX,
		PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
		PIPE_UNLIMITED_INSTANCES,
		readBufferSize,
		writeBufferSize,
		0,
		nullptr
	));

	return pipe != INVALID_HANDLE_VALUE;
}

bool NamedPipeServer::waitForClient()
{
	return ConnectNamedPipe(pipe, NULL) ?
		TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
}

NamedPipeClient::NamedPipeClient(const std::string& name)
	: NamedPipe(name)
{}

bool NamedPipeClient::connect()
{
	pipe.assign(CreateFileA(
		name.c_str(),
		GENERIC_READ | GENERIC_WRITE,
		0,
		nullptr,
		OPEN_EXISTING,
		0,
		NULL
	));

	if (pipe == INVALID_HANDLE_VALUE)
		return false;

	DWORD dwMode = PIPE_READMODE_MESSAGE;
	return SetNamedPipeHandleState(
		pipe,
		&dwMode,
		nullptr,
		nullptr
	);
}