#pragma once

import <string>;
import <map>;

class Offsets
{
public:
	Offsets();
	struct LuaApiAddresses* luaApiAddresses;
	struct RiblixAddresses* riblixAddresses;

	static const uintptr_t invalidAddress = UINTPTR_MAX;
	void initAddressesFromFile(const std::wstring& dumpPath);

private:
	std::map<std::string, uintptr_t> parseAddressDumpFile(const std::wstring& fileName);
	void validateAddresses();
};

inline Offsets offsets;