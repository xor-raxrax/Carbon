
// without this thing it will complain about
// some freaking define shenanigans in windows headers
#define __SPECSTRINGS_STRICT_LEVEL 0

#include "../Common/Windows.h"
#include <psapi.h>

#include "FunctionMarker.h"

#include "../Common/Luau/Luau.h"

FunctionMarker::FunctionMarker()
{
	ourLuaFunctionLineMarker = 413254432;
	setTextSectionRegion();
}

bool FunctionMarker::isOurClosure(Closure* func) const
{
	if (func->isC)
	{
		// check if address is in range of our .text segment
		return (uintptr_t)func->c.f >= textStart
			&& (uintptr_t)func->c.f < textEnd;
	}
	else
	{
		return func->l.p->linedefined == ourLuaFunctionLineMarker;
	}
}

void FunctionMarker::setOurProto(Proto* proto) const
{
	proto->linedefined = ourLuaFunctionLineMarker;
}


void FunctionMarker::setTextSectionRegion()
{
	auto modBaseAddr = (uintptr_t)GetModuleHandle(NULL);

	auto dosHeader = (IMAGE_DOS_HEADER*)modBaseAddr;
	auto ntHeaders = (IMAGE_NT_HEADERS*)(modBaseAddr + dosHeader->e_lfanew);
	auto sectionHeaders = (IMAGE_SECTION_HEADER*)(modBaseAddr + dosHeader->e_lfanew
		+ sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + ntHeaders->FileHeader.SizeOfOptionalHeader);

	for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i)
	{
		auto& section = sectionHeaders[i];
		if (strcmp((char*)section.Name, ".text") == 0)
		{
			textStart = modBaseAddr + section.VirtualAddress;
			textEnd = textStart + section.Misc.VirtualSize;
			return;
		}
	}
}