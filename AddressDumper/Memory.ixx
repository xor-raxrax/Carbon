export module Memory;

import <vector>;
import <string>;
import <vector>;
import <functional>;
import <map>;
import <mutex>;
import <iostream>;

import StringUtils;
import ExceptionBase;

typedef unsigned char BYTE;

export 
{
	struct ByteArray
	{
		const BYTE* array;
		size_t size;
	};

	std::pair<int, size_t> getThreadCountAndChunkSize(ByteArray buffer)
	{
		const size_t chunkMinSize = 0x10000;

		int nThreads;
		size_t chunkSize = chunkMinSize;

		if (buffer.size <= chunkMinSize || 1)
		{
			nThreads = 1;
		}
		else
		{
			nThreads = std::thread::hardware_concurrency();
			chunkSize = buffer.size / nThreads;
			while (chunkSize < chunkMinSize)
			{
				size_t chunkSize = buffer.size / nThreads;
				nThreads--;
			}
		}

		return { nThreads, chunkSize };
	}

	void searchSequence(ByteArray sequence, const BYTE* bytes,
		size_t start, size_t end,
		size_t& resultIndex, std::mutex& mutex
	)
	{
		for (size_t i = start; i <= end; i++)
		{
			if (memcmp(bytes + i, sequence.array, sequence.size) == 0)
			{
				std::scoped_lock<std::mutex> lock(mutex);
				resultIndex = i;
				return;
			}
		}
	}

	void searchSequences(ByteArray sequence, const BYTE* bytes,
		size_t start, size_t end,
		std::vector<size_t>& resultIndices, std::mutex& mutex
	)
	{
		for (size_t i = start; i <= end; i++)
		{
			if (memcmp(bytes + i, sequence.array, sequence.size) == 0)
			{
				std::scoped_lock<std::mutex> lock(mutex);
				resultIndices.push_back(i);
			}
		}
	}

	std::vector<size_t> findSequences(ByteArray buffer, ByteArray sequence)
	{
		auto [nThreads, chunkSize] = getThreadCountAndChunkSize(buffer);

		std::vector<std::thread> threads;
		threads.reserve(nThreads);
		std::mutex mutex;

		std::vector<size_t> resultIndices;

		for (size_t i = 0; i < nThreads; i++)
		{
			size_t start = i * chunkSize;
			size_t end = (i == nThreads - 1) ? buffer.size - 1 : start + chunkSize - 1;
			threads.push_back(
				std::thread(
					searchSequences, sequence, buffer.array,
					start, end, std::ref(resultIndices), std::ref(mutex)
				)
			);
		}

		for (auto& thread : threads)
			thread.join();

		return resultIndices;
	}

	size_t findSequence(ByteArray buffer, ByteArray sequence)
	{
		auto [nThreads, chunkSize] = getThreadCountAndChunkSize(buffer);

		std::vector<std::thread> threads;
		threads.reserve(nThreads);
		std::mutex mutex;

		size_t resultIndex = 0;

		for (size_t i = 0; i < nThreads; i++)
		{
			size_t start = i * chunkSize;
			size_t end = (i == nThreads - 1) ? buffer.size - 1 : start + chunkSize - 1;
			threads.push_back(
				std::thread(searchSequence, sequence, buffer.array,
					start, end,
					std::ref(resultIndex), std::ref(mutex)
				)
			);
		}

		for (auto& thread : threads)
			thread.join();

		return resultIndex;
	}

	void printMemoryRange(const BYTE* startAddress, int numBytes)
	{
		for (int i = 0; i < numBytes; ++i)
			std::cout << startAddress[i];
		std::cout << std::endl;
	}

	void skipZeros(const BYTE* data, size_t& offset)
	{
		while (!*(data + offset))
			offset++;
	}
}
