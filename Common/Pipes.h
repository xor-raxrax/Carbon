#include "Windows.h"
#include "Exception.h"

import <string>;
import <stdexcept>;

class NamedPipe;

class ReadBuffer
{
	friend class NamedPipe;
public:
	template <typename T>
	T readTyped()
	{
		if (std::cmp_less(std::distance(bufferReadPos, buffer.end()), sizeof(T)))
			throw std::out_of_range("buffer underflow");

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
			throw std::out_of_range("buffer underflow");

		auto result = reinterpret_cast<T*>(&*bufferReadPos);
		std::advance(bufferReadPos, size);
		return result;
	}

private:
	std::string buffer;
	std::string::iterator bufferReadPos;

	ReadBuffer(const std::string& buffer)
		: buffer(buffer)
		, bufferReadPos(this->buffer.begin())
	{

	}
};

class WriteBuffer
{
	friend class NamedPipe;
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

	void send();
private:
	std::string buffer;
	NamedPipe& pipe;
	bool didSend = false;

	WriteBuffer(NamedPipe& pipe)
		: pipe(pipe)
	{

	}
};

class NamedPipe
{
public:
	friend class WriteBuffer;

	static const size_t readBufferSize = 1 * 1024;
	static const size_t writeBufferSize = 1 * 1024;
	inline static const char* const defaultName = "coal_delivery";

	NamedPipe(const std::string& name = defaultName)
		: name("\\\\.\\pipe\\" + name)
	{
	
	}

	ReadBuffer makeReadBuffer()
	{
		return ReadBuffer(std::move(fetch()));
	}

	WriteBuffer makeWriteBuffer()
	{
		return WriteBuffer(*this);
	}

	void close()
	{
		pipe.close();
	}
protected:
	std::string name;
	HandleScope pipe;

	std::string fetch()
	{
		std::string result;
		
		char buffer[readBufferSize];
		
		do {
			DWORD bytesRead = 0;

			if (!ReadFile(
				pipe,
				&buffer,
				readBufferSize,
				&bytesRead,
				nullptr
			))
				raise("some shit happened: ", GetLastError());

			if (bytesRead > 0)
				result.append(buffer, bytesRead);

		} while (GetLastError() == ERROR_MORE_DATA);

		return result;
	}

	void send(const std::string& buffer)
	{
		DWORD totalBytesWritten = 0;
		DWORD bytesWritten = 0;
		size_t dataSize = buffer.size();
		BOOL success = FALSE;

		while (totalBytesWritten < dataSize) {
			DWORD chunkSize = static_cast<DWORD>(std::min(writeBufferSize, dataSize - totalBytesWritten));
			
			if (!WriteFile(
				pipe,
				buffer.c_str() + totalBytesWritten,
				chunkSize,
				&bytesWritten,
				nullptr
			))
				raise("some shit happened: ", GetLastError());

			totalBytesWritten += bytesWritten;
		}
	}
};

void WriteBuffer::send()
{
	if (didSend)
		throw std::exception("send pipe data twice");

	didSend = true;
	pipe.send(buffer);
}

class NamedPipeServer : public NamedPipe
{
public:
	NamedPipeServer(const std::string& name = defaultName)
		: NamedPipe(name)
	{}

	bool create()
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

	bool waitForClient() {
		return ConnectNamedPipe(pipe, NULL) ?
			TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
	}
};


class NamedPipeClient : public NamedPipe
{
public:
	NamedPipeClient(const std::string& name = defaultName)
		: NamedPipe(name)
	{}

	bool connect()
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
};