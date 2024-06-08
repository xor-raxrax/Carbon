#include "../Common/Exception.h"
#include "../Common/Windows.h"

#include <psapi.h>

#include <inttypes.h>

extern "C"
{
	#include "Zydis.h"
}

// remove when intelli sense gets healed or if code magically stopped breaking it
#if (_MSC_VER && 1)
#include <vector>
#include <fstream>
#else
import <vector>;
import <fstream>;
#endif

import <string>;
import <vector>;
import <functional>;
import <map>;
import <iostream>;
import <thread>;
import <mutex>;

MODULEENTRY32 getFirstModule(DWORD processId, const std::wstring& moduleName) {
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
	if (hSnapshot == INVALID_HANDLE_VALUE)
		return {};

	HandleScope handleScope(hSnapshot);

	MODULEENTRY32 moduleEntry;
	moduleEntry.dwSize = sizeof(MODULEENTRY32);

	if (!Module32First(hSnapshot, &moduleEntry))
		return {};

	do
	{
		if (moduleName == moduleEntry.szModule)
			return moduleEntry;
	} while (Module32Next(hSnapshot, &moduleEntry));

	return {};
}

struct MainInfo
{
	MODULEENTRY32 module;
	IMAGE_SECTION_HEADER text;
	IMAGE_SECTION_HEADER rdata;
	IMAGE_SECTION_HEADER data;
};

MainInfo getCodeSection(HANDLE hProcess, DWORD processId, const std::wstring& moduleName)
{
	MODULEENTRY32 module = getFirstModule(processId, moduleName);
	if (module.modBaseSize == 0)
		raise("failed to get first module");

	IMAGE_DOS_HEADER dosHeader;
	if (!ReadProcessMemory(hProcess, module.modBaseAddr, &dosHeader, sizeof(dosHeader), nullptr))
		raise("failed to read DOS header; error code:", GetLastError());

	IMAGE_NT_HEADERS ntHeaders;
	if (!ReadProcessMemory(
		hProcess,
		reinterpret_cast<LPVOID>(uintptr_t(module.modBaseAddr) + dosHeader.e_lfanew),
		&ntHeaders,
		sizeof(ntHeaders),
		nullptr
	))
		raise("failed to read to read NT headers; error code:", GetLastError());

	IMAGE_SECTION_HEADER sectionHeaders[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
	if (!ReadProcessMemory(hProcess,
		reinterpret_cast<LPVOID>(uintptr_t(module.modBaseAddr)
			+ dosHeader.e_lfanew
			+ sizeof(DWORD)
			+ sizeof(IMAGE_FILE_HEADER)
			+ ntHeaders.FileHeader.SizeOfOptionalHeader
		),
		&sectionHeaders,
		sizeof(sectionHeaders),
		nullptr
	))
		raise("failed to read to read section headers; error code:", GetLastError());

	MainInfo result{ module };

	for (int i = 0; i < ntHeaders.FileHeader.NumberOfSections; ++i) {
		auto& section = sectionHeaders[i];

		if (strcmp((char*)section.Name, ".text") == 0)
			result.text = section;
		else if (strcmp((char*)section.Name, ".rdata") == 0)
			result.rdata = section;
		else if (strcmp((char*)section.Name, ".data") == 0)
			result.data = section;
	}

	return result;
}

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
			std::lock_guard<std::mutex> lock(mutex);
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
			std::lock_guard<std::mutex> lock(mutex);
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

bool isPrologueCandidate(ZydisDisassembledInstruction& instruction)
{
	if (instruction.info.mnemonic == ZYDIS_MNEMONIC_PUSH)
	{
		// push rdi/rbx
		return instruction.operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER
			&& (instruction.operands[0].reg.value == ZYDIS_REGISTER_RDI
				|| instruction.operands[0].reg.value == ZYDIS_REGISTER_RBX);
	}
	else if (instruction.info.mnemonic == ZYDIS_MNEMONIC_SUB)
	{
		// sub rsp, ?
		return instruction.operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER
			&& instruction.operands[0].reg.value == ZYDIS_REGISTER_RSP
			&& instruction.operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE;
	}
	else if (instruction.info.mnemonic == ZYDIS_MNEMONIC_MOV)
	{
		// mov [rsp + ?], rbx
		return instruction.operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY
			&& instruction.operands[0].mem.base == ZYDIS_REGISTER_RSP
			&& instruction.operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER
			&& instruction.operands[1].reg.value == ZYDIS_REGISTER_RBX;

	}

	return false;
}

struct FunctionData
{
	ByteArray buffer;
	uintptr_t prologueRuntimeAddress;
	size_t prologueOffset;
};

class DisassemblerState
{
public:

	DisassemblerState(FunctionData functionData)
		: buffer(functionData.buffer)
		, runtimeAddress(functionData.prologueRuntimeAddress)
		, offset(functionData.prologueOffset)
		, initialRuntimeAddress(functionData.prologueRuntimeAddress)
	{

	}

	DisassemblerState(ByteArray buffer, uintptr_t runtimeAddress, size_t offset)
		: buffer(buffer)
		, runtimeAddress(runtimeAddress)
		, offset(offset)
		, initialRuntimeAddress(runtimeAddress)
	{

	}

	ZydisDisassembledInstruction instruction;
	ByteArray buffer;
	uintptr_t runtimeAddress;
	size_t offset;

	bool currentIsAlign = false;
	bool currentIsReturn = false;

	bool lastWasAlign = false;
	bool lastWasReturn = false;

	bool isFirstPrologueInstruction = false;

	const unsigned functionAlignment = 16;

	bool next(bool addText = false)
	{
		if (!disassemble(addText))
			return false;
		
		currentIsAlign = false;
		currentIsReturn = false;

		if (isAlignInstruction(instruction))
		{
			currentIsAlign = true;
		}
		else if (isReturnInstruction(instruction))
		{
			currentIsReturn = true;
		}
		
		isFirstPrologueInstruction = false;

		if (offset % functionAlignment == 0)
		{
			if (lastWasAlign || lastWasReturn)
			{
				isFirstPrologueInstruction = isPrologueCandidate(instruction);
			}
			// otherwise we are in the middle of function
		}

		return true;
	}

	bool disassemble(bool addText)
	{
		if (addText)
		{
			return ZYAN_SUCCESS(DisassembleInstructionWithText(runtimeAddress,
				/* buffer:          */ buffer.array + offset,
				/* length:          */ buffer.size - offset,
				/* instruction:     */ &instruction
			));
		}
		else
		{
			return ZYAN_SUCCESS(DisassembleInstruction(runtimeAddress,
				/* buffer:          */ buffer.array + offset,
				/* length:          */ buffer.size - offset,
				/* instruction:     */ &instruction
			));
		}
	}

	void reset()
	{
		offset = 0;
		runtimeAddress = initialRuntimeAddress;
		currentIsAlign = false;
		currentIsReturn = false;

		lastWasAlign = false;
		lastWasReturn = false;

		isFirstPrologueInstruction = false;
	}

	void post()
	{
		offset += instruction.info.length;
		runtimeAddress += instruction.info.length;

		lastWasAlign = currentIsAlign;
		lastWasReturn = currentIsReturn;
	}

	void skip()
	{
		if (next())
			post();
		else
			skipByte();
	}

	bool isEmpty() const
	{
		return buffer.size <= offset;
	}

	void skipByte()
	{
		offset++;
		runtimeAddress++;
	}

	bool isPrologue() const { return isFirstPrologueInstruction; }
	const ZydisDisassembledInstruction& getInstruction() const { return instruction; }
	uintptr_t getRuntimeAddress() const { return runtimeAddress; }
	size_t getOffset() const { return offset; }

private:

	uintptr_t initialRuntimeAddress;

	static bool isAlignInstruction(ZydisDisassembledInstruction& instruction)
	{
		return instruction.info.mnemonic == ZYDIS_MNEMONIC_INT3
			|| (instruction.info.mnemonic == ZYDIS_MNEMONIC_NOP && instruction.info.length == 1);
	}

	static bool isReturnInstruction(ZydisMnemonic mnemonic)
	{
		return mnemonic == ZYDIS_MNEMONIC_RET;
	}

	static bool isReturnInstruction(ZydisDisassembledInstruction& instruction)
	{
		return isReturnInstruction(instruction.info.mnemonic);
	}
};

struct getInstructionResult
{
	ZydisDisassembledInstruction instruction;
	uintptr_t runtimeAddress;
	size_t offset;
};

std::vector<uintptr_t> getCallingFunctions(FunctionData functionData)
{
	std::vector<uintptr_t> result;

	DisassemblerState state(functionData);

	do
	{
		if (!state.next())
			raise("getInstruction disassemble failed");

		auto& instruction = state.getInstruction();
		if (instruction.info.mnemonic == ZYDIS_MNEMONIC_CALL)
		{
			uintptr_t callingAddress = 0;
			ZydisCalcAbsoluteAddress(&instruction.info, &instruction.operands[0], state.getRuntimeAddress(), &callingAddress);
			result.push_back(callingAddress);
		}

		state.post();

	} while (!state.isPrologue());

	return result;
}

std::vector<uintptr_t> getLeaSources(FunctionData functionData)
{
	std::vector<uintptr_t> result;

	DisassemblerState state(functionData);

	do
	{
		if (!state.next())
			raise("getInstruction disassemble failed");

		auto& instruction = state.getInstruction();
		if (instruction.info.mnemonic == ZYDIS_MNEMONIC_LEA)
		{
			uintptr_t callingAddress = 0;
			ZydisCalcAbsoluteAddress(&instruction.info, &instruction.operands[1], state.getRuntimeAddress(), &callingAddress);
			result.push_back(callingAddress);
		}

		state.post();

	} while (!state.isPrologue());

	return result;
}

getInstructionResult getInstruction(FunctionData functionData,
	std::function<bool(const ZydisDisassembledInstruction&)> callback)
{
	DisassemblerState state(functionData);

	do
	{
		if (!state.next())
			raise("getInstruction disassemble failed");

		auto& instruction = state.getInstruction();
		if (callback(instruction))
			return { instruction, state.getRuntimeAddress(), state.getOffset() };

		state.post();

	} while (!state.isPrologue());

	raise("getInstruction disassemble failed");
}

FunctionData getNextFunction(FunctionData functionData)
{
	DisassemblerState state(functionData);

	while (true)
	{
		if (!state.next())
			raise("getNextFunction disassemble failed");

		// avoid incrementing offset by .post() as we need to return first instruction in prologue
		if (state.isPrologue())
			return { functionData.buffer, state.getRuntimeAddress(), state.getOffset() };

		state.post();
	}
}

inline void skipZeros(const BYTE* data, size_t& offset)
{
	while (!*(data + offset))
		offset++;
}

class Dumper
{
public:
	using lua_lib = std::map<std::string, uintptr_t>;

	void run()
	{
		setupMemoryData();

		uintptr_t _VERSION_address = getAddress_VERSION();
		std::cout << "_VERSION at " << (void*)_VERSION_address << std::endl;

		uintptr_t lastPrologue = 0;

		auto functionDataFromAddress = [&](uintptr_t address) -> FunctionData {
			return { text.newBuffer(), address, address - text.address};
		};

		DisassemblerState state(text.newBuffer(), text.address, 0);

		uintptr_t luaL_register = 0;
		while (true)
		{
			if (!state.next())
			{
				state.skipByte();
				continue;
			}

			auto& instruction = state.getInstruction();
			if (state.isPrologue())
			{
				lastPrologue = state.getRuntimeAddress();
			}
			else if (instruction.info.mnemonic == ZYDIS_MNEMONIC_LEA)
			{
				uintptr_t lea_VERSION_source = 0;
				ZydisCalcAbsoluteAddress(&instruction.info, &instruction.operands[1], state.getRuntimeAddress(), &lea_VERSION_source);
				if (lea_VERSION_source == _VERSION_address)
				{
					auto disassembleFromBaseLib = [&]() {

						dumpInfo.add("disassemble all", "lea _VERSION", state.getRuntimeAddress());

						FunctionData luaopen_base = functionDataFromAddress(lastPrologue);

						dumpInfo.add("lea _VERSION", "luaopen_base", luaopen_base.prologueRuntimeAddress);

						{
							auto calls = getCallingFunctions(luaopen_base);

							dumpInfo.newRegistrar("luaopen_base")
								.add("lua_pushvalue", calls.at(0))
								.add("lua_setfield", calls.at(1))
								.add("luaL_register", calls.at(2))
								.add("lua_pushlstring", calls.at(3))
								.add("lua_setfield", calls.at(4))
								.add("lua_pushcclosurek", calls.at(5))
								.add("lua_pushcclosurek", calls.at(6));

							luaL_register = dumpInfo.get("luaL_register");
						}

						{
							auto calls = getCallingFunctions(functionDataFromAddress(luaL_register));

							dumpInfo.newRegistrar("luaL_register")
								.add("luaL_findtable", calls.at(0))
								.add("lua_getfield", calls.at(1))
								.add("lua_type", calls.at(2))
								.add("lua_settop", calls.at(3))
								.add("luaL_findtable", calls.at(4))
								.add("lua_pushvalue", calls.at(5))
								.add("lua_setfield", calls.at(6))
								.add("lua_remove", calls.at(7))
								.add("lua_pushcclosurek", calls.at(8))
								.add("lua_setfield", calls.at(9))
								.add("luaL_errorL", calls.at(10));
						}

						{
							auto lua_getfield = dumpInfo.get("lua_getfield");
							auto calls = getCallingFunctions(functionDataFromAddress(lua_getfield));

							dumpInfo.newRegistrar("lua_getfield")
								.add("luaC_barrierback", calls.at(0))
								.add("pseudo2addr", calls.at(1))
								.add("luaS_newlstr", calls.at(2))
								.add("luaV_gettable", calls.at(3));
						}

						{
							auto luaV_gettable = dumpInfo.get("luaV_gettable");
							auto calls = getCallingFunctions(functionDataFromAddress(luaV_gettable));

							dumpInfo.newRegistrar("luaV_gettable")
								.add("luaH_get", calls.at(0))
								.add("luaT_gettm", calls.at(1))
								.add("luaT_gettmbyobj", calls.at(2))
								.add("callTMres", calls.at(3))
								.add("luaG_indexerror", calls.at(4))
								.add("luaG_runerrorL", calls.at(5));
						}

						{
							auto luaG_runerrorL = dumpInfo.get("luaG_runerrorL");
							auto calls = getCallingFunctions(functionDataFromAddress(luaG_runerrorL));

							dumpInfo.newRegistrar("luaG_runerrorL")
								.add("luaD_throw", calls.back());
						}

						{
							auto luaH_get = dumpInfo.get("luaH_get");
							auto calls = getCallingFunctions(functionDataFromAddress(luaH_get));

							dumpInfo.newRegistrar("luaH_get")
								.add("luaH_getstr", calls.at(0))
								.add("luaH_getnum", calls.at(1));
						}

						{
							auto luaL_findtable = dumpInfo.get("luaL_findtable");
							auto calls = getCallingFunctions(functionDataFromAddress(luaL_findtable));

							dumpInfo.newRegistrar("luaL_findtable")
								.add("lua_pushvalue", calls.at(0))
								.add("lua_pushlstring", calls.at(2))
								.add("lua_rawget", calls.at(3))
								.add("lua_type", calls.at(4))
								.add("lua_settop", calls.at(5))
								.add("lua_createtable", calls.at(6))
								.add("lua_pushlstring", calls.at(7))
								.add("lua_pushvalue", calls.at(8))
								.add("lua_settable", calls.at(9));
						}

						uintptr_t base_funcs = 0;
						{
							auto leas = getLeaSources(luaopen_base);

							base_funcs = leas.at(1);
							dumpInfo.newRegistrar("luaopen_base lea")
								.add("luaB_inext", leas.at(5))
								.add("luaB_pcallcont", leas.at(17))
								.add("luaB_pcally", leas.at(19))
								.add("luaB_xpcallcont", leas.at(22))
								.add("luaB_xpcally", leas.at(24));
						}

						{
							auto luaB_xpcally = dumpInfo.get("luaB_xpcally");
							auto calls = getCallingFunctions(functionDataFromAddress(luaB_xpcally));

							dumpInfo.newRegistrar("luaB_xpcally")
								.add("luaL_checktype", calls.at(0))
								.add("lua_pushvalue", calls.at(1))
								.add("lua_pushvalue", calls.at(2))
								.add("lua_replace", calls.at(3))
								.add("lua_replace", calls.at(4))
								.add("luaD_pcall", calls.at(5))
								.add("lua_rawcheckstack", calls.at(6));
						}

						{
							auto luaB_xpcallcont = functionDataFromAddress(dumpInfo.get("luaB_xpcallcont"));
							auto leas = getLeaSources(luaB_xpcallcont);
							dumpInfo.add("luaB_xpcallcont lea", "luaB_xpcallerr", leas.at(0));

							auto luaB_xpcallerr = functionDataFromAddress(leas.at(0));

							auto [instruction, runtimeAddress, offset] = getInstruction(luaB_xpcallerr,
								[&](const ZydisDisassembledInstruction& instruction) {
									return instruction.info.mnemonic == ZYDIS_MNEMONIC_JMP;
								});

							uintptr_t luaD_call = 0;
							ZydisCalcAbsoluteAddress(&instruction.info, &instruction.operands[0], runtimeAddress, &luaD_call);
							dumpInfo.add("luaB_xpcallerr", "luaD_call", luaD_call);
						}

						{
							auto luaD_call = dumpInfo.get("luaD_call");
							auto calls = getCallingFunctions(functionDataFromAddress(luaD_call));

							dumpInfo.newRegistrar("luaD_call")
								.add("luau_precall", calls.at(0));
						}

						{
							auto luau_precall = dumpInfo.get("luau_precall");
							auto calls = getCallingFunctions(functionDataFromAddress(luau_precall));

							dumpInfo.newRegistrar("luau_precall")
								.add("luaV_tryfuncTM", calls.at(0))
								.add("luaD_growCI", calls.at(1));
						}

						{
							auto luaD_growCI = dumpInfo.get("luaD_growCI");
							auto calls = getCallingFunctions(functionDataFromAddress(luaD_growCI));

							dumpInfo.newRegistrar("luaD_growCI")
								.add("luaD_reallocCI", calls.at(0));
						}

						{
							auto luaB_inext = dumpInfo.get("luaB_inext");
							auto calls = getCallingFunctions(functionDataFromAddress(luaB_inext));

							dumpInfo.newRegistrar("luaB_inext")
								.add("luaL_checkinteger", calls.at(0))
								.add("luaL_checktype", calls.at(1))
								.add("lua_pushinteger", calls.at(2))
								.add("lua_rawgeti", calls.at(3))
								.add("lua_type", calls.at(4));
						}

						auto base_lib = parseLuaLib(base_funcs, "base_funcs");

						for (auto& [name, funcAddress] : base_lib)
							dumpInfo.add("base_funcs", "luaB_" + name, funcAddress);

						{
							auto luaB_getfenv = base_lib.at("getfenv");
							auto calls = getCallingFunctions(functionDataFromAddress(luaB_getfenv));

							dumpInfo.newRegistrar("luaB_getfenv")
								.add("getfunc", calls.at(0))
								.add("lua_iscfunction", calls.at(1))
								.add("lua_pushvalue", calls.at(2))
								.add("lua_getfenv", calls.at(3))
								.add("lua_setsafeenv", calls.at(4));
						}

						{
							auto luaB_setfenv = base_lib.at("setfenv");
							auto calls = getCallingFunctions(functionDataFromAddress(luaB_setfenv));

							dumpInfo.newRegistrar("luaB_setfenv")
								.add("luaL_checktype", calls.at(0))
								.add("getfunc", calls.at(1))
								.add("lua_pushvalue", calls.at(2))
								.add("lua_setsafeenv", calls.at(3))
								.add("lua_isnumber", calls.at(4))
								.add("lua_tonumberx", calls.at(5))
								.add("lua_pushthread", calls.at(6))
								.add("lua_insert", calls.at(7))
								.add("lua_setfenv", calls.at(8))
								.add("lua_iscfunction", calls.at(9))
								.add("lua_setfenv", calls.at(10))
								.add("luaL_errorL", calls.at(11));
						}

						{
							auto luaB_rawequal = base_lib.at("rawequal");
							auto calls = getCallingFunctions(functionDataFromAddress(luaB_rawequal));

							dumpInfo.newRegistrar("luaB_rawequal")
								.add("luaL_checkany", calls.at(0))
								.add("luaL_checkany", calls.at(1))
								.add("lua_rawequal", calls.at(2))
								.add("lua_pushboolean", calls.at(3));
						}

						{
							auto luaB_rawget = base_lib.at("rawget");
							auto calls = getCallingFunctions(functionDataFromAddress(luaB_rawget));

							dumpInfo.newRegistrar("luaB_rawget")
								.add("luaL_checktype", calls.at(0))
								.add("luaL_checkany", calls.at(1))
								.add("lua_settop", calls.at(2))
								.add("lua_rawget", calls.at(3));
						}

						{
							auto luaB_rawset = base_lib.at("rawset");
							auto calls = getCallingFunctions(functionDataFromAddress(luaB_rawset));

							dumpInfo.newRegistrar("luaB_rawset")
								.add("luaL_checktype", calls.at(0))
								.add("luaL_checkany", calls.at(1))
								.add("luaL_checkany", calls.at(2))
								.add("lua_settop", calls.at(3))
								.add("lua_rawset", calls.at(4));
						}

						{
							auto luaB_rawlen = base_lib.at("rawlen");
							auto calls = getCallingFunctions(functionDataFromAddress(luaB_rawlen));

							dumpInfo.newRegistrar("luaB_rawlen")
								.add("lua_type", calls.at(0))
								.add("lua_objlen", calls.at(1))
								.add("lua_pushinteger", calls.at(2))
								.add("luaL_argerrorL", calls.at(3));
						}

						{
							auto luaB_type = base_lib.at("type");
							auto calls = getCallingFunctions(functionDataFromAddress(luaB_type));

							dumpInfo.newRegistrar("luaB_type")
								.add("luaL_checkany", calls.at(0))
								.add("lua_type", calls.at(1))
								.add("lua_typename", calls.at(2))
								.add("lua_pushstring", calls.at(3));
						}

						{
							auto luaB_typeof = base_lib.at("typeof");
							auto calls = getCallingFunctions(functionDataFromAddress(luaB_typeof));

							dumpInfo.newRegistrar("luaB_typeof")
								.add("luaL_checkany", calls.at(0))
								.add("luaL_typename", calls.at(1))
								.add("lua_pushstring", calls.at(2));
						}

						{
							auto luaB_next = base_lib.at("next");
							auto calls = getCallingFunctions(functionDataFromAddress(luaB_next));

							dumpInfo.newRegistrar("luaB_next")
								.add("luaL_checktype", calls.at(0))
								.add("lua_settop", calls.at(1))
								.add("lua_next", calls.at(2))
								.add("lua_pushnil", calls.at(3));
						}

						{
							auto luaB_assert = base_lib.at("assert");
							auto calls = getCallingFunctions(functionDataFromAddress(luaB_assert));

							dumpInfo.newRegistrar("luaB_assert")
								.add("luaL_checkany", calls.at(0))
								.add("lua_toboolean", calls.at(1))
								.add("luaL_optlstring", calls.at(2))
								.add("luaL_errorL", calls.at(3));
						}

						{
							auto luaB_select = base_lib.at("select");
							auto calls = getCallingFunctions(functionDataFromAddress(luaB_select));

							dumpInfo.newRegistrar("luaB_select")
								.add("lua_gettop", calls.at(0))
								.add("lua_type", calls.at(1))
								.add("lua_tolstring", calls.at(2))
								.add("lua_pushinteger", calls.at(3))
								.add("luaL_checkinteger", calls.at(4))
								.add("luaL_argerrorL", calls.at(5));
						}

						{
							auto luaB_tostring = base_lib.at("tostring");
							auto calls = getCallingFunctions(functionDataFromAddress(luaB_tostring));

							dumpInfo.newRegistrar("luaB_tostring")
								.add("luaL_checkany", calls.at(0))
								.add("luaL_tolstring", calls.at(1));
						}

						{
							auto luaB_newproxy = base_lib.at("newproxy");
							auto calls = getCallingFunctions(functionDataFromAddress(luaB_newproxy));

							dumpInfo.newRegistrar("luaB_newproxy")
								.add("lua_type", calls.at(0))
								.add("lua_toboolean", calls.at(1))
								.add("lua_newuserdatatagged", calls.at(2))
								.add("lua_createtable", calls.at(3))
								.add("lua_setmetatable", calls.at(4))
								.add("luaL_typeerrorL", calls.at(5));
						}

						{
							auto luaB_tonumber = base_lib.at("tonumber");
							auto calls = getCallingFunctions(functionDataFromAddress(luaB_tonumber));

							dumpInfo.newRegistrar("luaB_tonumber")
								.add("luaL_optinteger", calls.at(0))
								.add("lua_tonumberx", calls.at(1))
								.add("lua_pushnumber", calls.at(2))
								.add("luaL_checkany", calls.at(3))
								.add("lua_pushnil", calls.at(4))
								.add("luaL_checkstring", calls.at(5));
						}

						{
							auto luaB_getmetatable = base_lib.at("getmetatable");
							auto calls = getCallingFunctions(functionDataFromAddress(luaB_getmetatable));

							dumpInfo.newRegistrar("luaB_getmetatable")
								.add("luaL_checkany", calls.at(0))
								.add("lua_getmetatable", calls.at(1))
								.add("lua_pushnil", calls.at(2))
								.add("luaL_getmetafield", calls.at(3));
						}

						{
							auto luaB_setmetatable = base_lib.at("setmetatable");
							auto calls = getCallingFunctions(functionDataFromAddress(luaB_setmetatable));

							dumpInfo.newRegistrar("luaB_setmetatable")
								.add("lua_type", calls.at(0))
								.add("luaL_checktype", calls.at(1))
								.add("luaL_getmetafield", calls.at(2))
								.add("lua_settop", calls.at(3))
								.add("lua_setmetatable", calls.at(4))
								.add("luaL_typeerrorL", calls.at(5))
								.add("luaL_errorL", calls.at(6));
						}

						{
							auto luaB_error = base_lib.at("error");
							auto calls = getCallingFunctions(functionDataFromAddress(luaB_error));

							dumpInfo.newRegistrar("luaB_error")
								.add("luaL_optinteger", calls.at(0))
								.add("lua_settop", calls.at(1))
								.add("lua_isstring", calls.at(2))
								.add("luaL_where", calls.at(3))
								.add("lua_pushvalue", calls.at(4))
								.add("lua_concat", calls.at(5))
								.add("lua_error", calls.at(6));
						}

						{
							auto luaL_typename = dumpInfo.get("luaL_typename");
							auto calls = getCallingFunctions(functionDataFromAddress(luaL_typename));

							dumpInfo.add("luaL_typename", "luaA_toobject", calls.at(0));
						}

						{
							auto luaL_typeerrorL = dumpInfo.get("luaL_typeerrorL");
							auto calls = getCallingFunctions(functionDataFromAddress(luaL_typeerrorL));

							dumpInfo.newRegistrar("luaL_typeerrorL")
								.add("currfuncname", calls.at(0))
								.add("luaA_toobject", calls.at(1))
								.add("luaT_objtypename", calls.at(2));

						}
					
						{
							auto luaL_optinteger = dumpInfo.get("luaL_optinteger");
							auto calls = getCallingFunctions(functionDataFromAddress(luaL_optinteger));

							dumpInfo.newRegistrar("luaL_optinteger")
								.add("lua_type", calls.at(0))
								.add("lua_tointegerx", calls.at(1))
								.add("tag_error", calls.at(2));
						}

						{
							auto lua_type = dumpInfo.get("lua_type");
							auto leas = getLeaSources(functionDataFromAddress(lua_type));

							dumpInfo.add("lua_type lea", "luaO_nilobject", leas.at(0));

							auto calls = getCallingFunctions(functionDataFromAddress(lua_type));

							dumpInfo.add("lua_type", "pseudo2addr", calls.at(0));
						}

						{
							auto getfunc = dumpInfo.get("getfunc");
							auto calls = getCallingFunctions(functionDataFromAddress(getfunc));

							dumpInfo.newRegistrar("getfunc")
								.add("lua_type", calls.at(0))
								.add("lua_pushvalue", calls.at(1))
								.add("luaL_optinteger", calls.at(2))
								.add("luaL_checkinteger", calls.at(3))
								.add("lua_getinfo", calls.at(4));
						}

						{
							auto lua_pushcclosurek = dumpInfo.get("lua_pushcclosurek");
							auto calls = getCallingFunctions(functionDataFromAddress(lua_pushcclosurek));

							dumpInfo.newRegistrar("lua_pushcclosurek")
								.add("luaC_barrierback", calls.at(1))
								.add("luaF_newCclosure", calls.at(2));
						}

						{
							auto luaF_newCclosure = dumpInfo.get("luaF_newCclosure");
							auto luaF_newLclosure = getNextFunction(functionDataFromAddress(luaF_newCclosure));
							dumpInfo.add("luaF_newCclosure", "luaF_newLclosure", luaF_newLclosure.prologueRuntimeAddress);
						}

						{
							auto luaF_newLclosure = dumpInfo.get("luaF_newLclosure");
							auto luaF_newproto = getNextFunction(functionDataFromAddress(luaF_newLclosure));
							dumpInfo.add("luaF_newLclosure", "luaF_newproto", luaF_newproto.prologueRuntimeAddress);
						}

					};
					disassembleFromBaseLib();
					break;
				}
			}

			state.post();
		}
	
		state.reset();

		// find every lib as now we have luaL_register
		{
			uintptr_t lastLastLea = 0;
			uintptr_t lastLea = 0;

			uintptr_t lastLeaR8Source = 0; // lib address
			uintptr_t lastLoadRDXSource = 0; // lib name
			bool lastLoadWasMov = false;
			uintptr_t lastLoadRDXAt = 0;
			uintptr_t lastXorRDXAt = 0;

			while (!state.isEmpty())
			{
				if (!state.next())
				{
					state.skipByte();
					continue;
				}

				auto& instruction = state.getInstruction();
				if (state.isPrologue())
				{
					lastPrologue = state.getRuntimeAddress();
				}
				else if (instruction.info.mnemonic == ZYDIS_MNEMONIC_CALL)
				{
					uintptr_t callingAddress = 0;
					ZydisCalcAbsoluteAddress(&instruction.info, &instruction.operands[0], state.getRuntimeAddress(), &callingAddress);
					
					if (callingAddress == luaL_register)
					{
						uintptr_t libAddress = 0;
						const char* libName = nullptr;
						if (lastLoadRDXAt > lastXorRDXAt)
						{
							if (lastLoadWasMov)
							{
								auto p1 = *(const char**)translatePointerNoThrow(lastLoadRDXSource);
								p1 = (const char*)translatePointerNoThrow((uintptr_t)p1);
								if (auto translated = p1)
									libName = translated;
							}
							else
							{
								libName = (const char*)translatePointerNoThrow(lastLoadRDXSource);
							}
						}

						if (libName)
							namedLibs[libName] = lastLeaR8Source;
						else
							unnamedLibs.push_back(lastLeaR8Source);
					}
				}
				else if (instruction.info.mnemonic == ZYDIS_MNEMONIC_LEA)
				{
					if (instruction.operands[0].reg.value == ZYDIS_REGISTER_R8)
					{
						ZydisCalcAbsoluteAddress(&instruction.info, &instruction.operands[1], state.getRuntimeAddress(), &lastLeaR8Source);
					}
					else if (instruction.operands[0].reg.value == ZYDIS_REGISTER_RDX)
					{
						ZydisCalcAbsoluteAddress(&instruction.info, &instruction.operands[1], state.getRuntimeAddress(), &lastLoadRDXSource);
						lastLoadRDXAt = state.getRuntimeAddress();
						lastLoadWasMov = false;
					}
				}
				else if (instruction.info.mnemonic == ZYDIS_MNEMONIC_MOV)
				{
					if (instruction.operands[0].reg.value == ZYDIS_REGISTER_RDX)
					{
						ZydisCalcAbsoluteAddress(&instruction.info, &instruction.operands[1], state.getRuntimeAddress(), &lastLoadRDXSource);
						lastLoadRDXAt = state.getRuntimeAddress();
						lastLoadWasMov = true;
					}
				}
				else if (instruction.info.mnemonic == ZYDIS_MNEMONIC_XOR)
				{
					if (instruction.operands[0].type == instruction.operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER)
					{
						if (instruction.operands[0].reg.value == ZYDIS_REGISTER_EDX)
						{
							lastXorRDXAt = state.getRuntimeAddress();
						}
					}
				}

				state.post();
			}

		}

		identifyUnnamedLibs();

		{
			auto table_lib = parseLuaLib(namedLibs.at("table"), "table_lib");
			for (auto& [name, funcAddress] : table_lib)
				dumpInfo.add("table_lib", "t" + name, funcAddress);

			{
				auto tforeach = table_lib.at("foreach");
				auto calls = getCallingFunctions(functionDataFromAddress(tforeach));

				dumpInfo.newRegistrar("tforeach")
					.add("luaL_checktype", calls.at(0))
					.add("luaL_checktype", calls.at(1))
					.add("lua_pushnil", calls.at(2))
					.add("lua_next", calls.at(3))
					.add("lua_pushvalue", calls.at(4))
					.add("lua_pushvalue", calls.at(5))
					.add("lua_pushvalue", calls.at(6))
					.add("lua_call", calls.at(7))
					.add("lua_type", calls.at(8))
					.add("lua_settop", calls.at(9))
					.add("lua_next", calls.at(10));
			}

			{
				auto tclone = table_lib.at("clone");
				auto calls = getCallingFunctions(functionDataFromAddress(tclone));

				dumpInfo.newRegistrar("tclone")
					.add("luaL_checktype", calls.at(0))
					.add("luaL_getmetafield", calls.at(1))
					.add("luaH_clone", calls.at(2))
					.add("luaA_pushobject", calls.at(3))
					.add("luaL_argerrorL", calls.at(4));
			}
		}


		{
			auto script_lib = parseLuaLib(namedLibs.at("script_lib"), "script_lib");
			for (auto& [name, funcAddress] : script_lib)
				dumpInfo.add("script_lib", "s" + name, funcAddress);

			{
				auto ssettings = script_lib.at("settings");
				auto calls = getCallingFunctions(functionDataFromAddress(ssettings));

				auto ScriptContext__getCurrentContext = calls.at(0);
				auto getCurrentContext = getCallingFunctions(functionDataFromAddress(ScriptContext__getCurrentContext)).at(0);
				dumpInfo.add("ScriptContext__getCurrentContext", "getCurrentContext", getCurrentContext);

				dumpInfo.newRegistrar("ScriptContext__settings")
					.add("ScriptContext__getCurrentContext", ScriptContext__getCurrentContext)
					.add("throwLackingCapability", calls.at(2))
					.add("InstanceBridge_pushshared", calls.at(4));
			}
		}
	}

	void writeToFile(const std::string& fileName) const
	{
		std::ofstream outFile(fileName);
		if (!outFile)
			raise("unable to open file", fileName, "for writing");

		for (auto& [name, address] : dumpInfo.getResult())
			outFile << name << "=" << (void*)(address - imageStart) << std::endl;

		outFile.close();
	}

	void print() const
	{
		for (auto& [name, address] : dumpInfo.getResult())
			std::cout << name << " " << (void*)(address - imageStart) << std::endl;
	}

	void identifyUnnamedLibs()
	{
		for (auto libAddress : unnamedLibs)
		{
			auto parsed = parseLuaLib(libAddress, "unnamed");

			if (parsed.find("profileend") != parsed.end())
				namedLibs["debug_ex"] = libAddress;

			if (parsed.find("graphemes") != parsed.end())
				namedLibs["utf8_ex"] = libAddress;

			if (parsed.find("settings") != parsed.end())
				namedLibs["script_lib"] = libAddress;

			if (parsed.find("defer") != parsed.end())
				namedLibs["task"] = libAddress;
		}
	}
private:

	lua_lib parseLuaLib(uintptr_t start, const std::string& debugName)
	{
		std::cout << defaultFormatter.format("parsing", debugName, "at", (void*)start, '\n');
		lua_lib result;

		auto currentPtr = (uintptr_t*)translatePointer(start);

		// luaL_Reg uses nullptrs as array terminating element
		while (*(uintptr_t*)currentPtr)
		{
			std::string name = (const char*)translatePointer(*(currentPtr++));
			auto funcAddress = *(currentPtr++);
			result[name] = funcAddress;
		}

		return result;
	}

	uintptr_t translatePointerNoThrow(uintptr_t original) const
	{
		if (text.address <= original && original < text.address + text.header.Misc.VirtualSize)
		{
			ptrdiff_t offset = original - text.address;
			return (uintptr_t)text.data.get() + offset;
		}

		if (rdata.address <= original && original < rdata.address + rdata.header.Misc.VirtualSize)
		{
			ptrdiff_t offset = original - rdata.address;
			return (uintptr_t)rdata.data.get() + offset;
		}

		if (data.address <= original && original < data.address + data.header.Misc.VirtualSize)
		{
			ptrdiff_t offset = original - data.address;
			return (uintptr_t)data.data.get() + offset;
		}

		return 0;
	};

	uintptr_t translatePointer(uintptr_t original) const
	{
		if (auto result = translatePointerNoThrow(original))
			return result;

		throw std::out_of_range("pointer does not point to a valid section");
	};

	void setupMemoryData()
	{
		std::wstring processName(L"RobloxStudioBeta.exe");

		auto processId = getProcessId(processName);
		if (!processId)
			raise(processName.c_str(), "not found");

		auto processHandle = HandleScope(OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, processId));
		if (processHandle == NULL)
			raise("failed to open processId", processId, "; error code:", GetLastError());

		auto [module, textHeader, rdataHeader, dataHeader] = getCodeSection(processHandle, processId, processName);

		imageStart = (uintptr_t)module.modBaseAddr;
		dumpInfo.setImageStart(imageStart);

		text = { textHeader, imageStart, processHandle, ".text" };
		rdata = { rdataHeader, imageStart, processHandle, ".rdata" };
		data = { dataHeader, imageStart, processHandle, ".data" };
	}

	uintptr_t getAddress_VERSION() const
	{
		const BYTE toFind[] = "_VERSION";
		auto offsets = findSequences(rdata.newBuffer(), {toFind, sizeof(toFind) - 1});

		for (auto& offset : offsets)
		{
			// pretty unreliable way to filter them out
			const BYTE xpcall[] = "xpcall";
			auto xpcallOffset = offset + sizeof(toFind);

			skipZeros(rdata.data.get(), xpcallOffset);

			if (std::memcmp(rdata.data.get() + xpcallOffset, xpcall, sizeof(xpcall)) == 0)
				return rdata.address + offset;
		}

		raise("unable to find _VERSION");
	}

	class DumpInfo
	{
	public:
		friend class Registrar;

		DumpInfo()
		{

		}

		void add(const std::string& source, const std::string& name, uintptr_t address)
		{
			Name key{ source, name };
			auto containingIter = registered.find(key);
			if (containingIter != registered.end())
			{
				if (containingIter->second != address)
				{
					std::string discoveredName;
					for (auto& [key, val] : registered)
						if (val == address)
							discoveredName = key.name;

					raise(
						"registered function", name,
						"differs from new address"
						"\n\tlast source:", containingIter->first.source,
						"\n\tat", (void*)containingIter->second, (void*)(containingIter->second - imageStart),
						"\n\tnew source:", source,
						"\n\tat", (void*)address, (void*)(address - imageStart),
						(discoveredName.empty() ? "" : "\n\tnew address was already discovered as " + discoveredName)
					);
				}
			}
			else
			{
				std::cout << defaultFormatter.format("added", name, "from", source, "at", (void*)address, (void*)(address - imageStart)) << std::endl;
				registered[key] = address;
			}
		}

		uintptr_t get(const std::string& name) const
		{
			for (auto& [key, address] : registered)
				if (key.name == name)
					return address;

			raise("function", name, "was not registered");
		}

		std::map<std::string, uintptr_t> getResult() const
		{
			std::map<std::string, uintptr_t> result;

			for (auto& [key, address] : registered)
				result[key.name] = address;

			return result;
		}

		void setImageStart(uintptr_t imageStart_)
		{
			imageStart = imageStart_;
		}

	private:

		class Registrar
		{
			friend class DumpInfo;

		public:

			Registrar& add(const std::string& name, uintptr_t address)
			{
				self->add(source, name, address);
				return *this;
			}

		private:

			Registrar(const std::string& name, DumpInfo* self)
				: source(name)
				, self(self)
			{

			}

			std::string source;
			DumpInfo* self;
		};

	public:

		Registrar newRegistrar(const std::string& name)
		{
			return Registrar(name, this);
		}

	private:

		struct Name
		{
			std::string source;
			std::string name;

			bool operator<(const Name& other) const
			{
				return name < other.name;
			}
		};

		uintptr_t imageStart;
		std::map<Name, uintptr_t> registered;
	};

	DumpInfo dumpInfo;

	struct Section
	{
		Section()
		{

		}

		Section(const IMAGE_SECTION_HEADER& header, uintptr_t imageStart, HANDLE processHandle, const char* debugName)
			: header(header)
		{
			address = imageStart + header.VirtualAddress;

			size = header.Misc.VirtualSize;
			data = std::make_unique<BYTE[]>(size);

			if (!ReadProcessMemory(processHandle,
				(LPVOID)address,
				data.get(),
				size,
				nullptr
			))
				raise("failed to read", debugName, "segment; error code : ", GetLastError());
		}

		ByteArray newBuffer() const
		{
			return { data.get(), size };
		}

		IMAGE_SECTION_HEADER header;
		size_t size = 0;
		uintptr_t address = 0;
		std::unique_ptr<BYTE[]> data;

	};

	Section text;
	Section rdata;
	Section data;

	uintptr_t imageStart = 0;
	std::map<std::string, uintptr_t> namedLibs;
	std::vector<uintptr_t> unnamedLibs;
};

int main(int argc, char** argv)
{
	try
	{
		Dumper dumper;
		dumper.run();
		dumper.print();
		std::string filename = (argc > 1) ? argv[1] : "dumpresult.txt";
		dumper.writeToFile(filename);
	}
	catch (const std::exception& exception)
	{
		std::cout << exception.what() << std::endl;
		getchar();
	}

	return 0;
}