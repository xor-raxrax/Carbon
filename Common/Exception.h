#pragma once

#include "Formatter.h"

template <typename... Args>
[[noreturn]] void raise(const Args&... args)
{
	throw std::exception(defaultFormatter.format(args...).c_str());
}