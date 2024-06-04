#pragma once

#include <Zydis/Zydis.h>

ZyanStatus DisassembleInstruction(ZyanU64 runtime_address,
	const void* buffer, ZyanUSize length,
	ZydisDisassembledInstruction* instruction);

ZyanStatus DisassembleInstructionWithText(ZyanU64 runtime_address,
	const void* buffer, ZyanUSize length,
	ZydisDisassembledInstruction* instruction);