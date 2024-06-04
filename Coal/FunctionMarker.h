#pragma once

struct Closure;
struct Proto;

using uintptr_t = size_t;

class FunctionMarker
{
public:
	FunctionMarker();

	bool isOurClosure(Closure* func) const;
	void setOurProto(Proto* proto) const;

	uintptr_t textStart = 0;
	uintptr_t textEnd = 0;
private:

	void setTextSectionRegion();

	int ourLuaFunctionLineMarker;
};

inline FunctionMarker* functionMarker = nullptr;