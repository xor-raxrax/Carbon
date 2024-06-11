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
	void initAddressesFromFile(const std::wstring& dumpPath, const std::wstring& dumperPath);

private:
	struct OffsetInfo
	{
		uintptr_t address;
		uintptr_t value;
	};

	using dumpInfo = std::map<std::string, OffsetInfo>;
	dumpInfo parseAddressDumpFile(const std::wstring& fileName);
	void validateAddresses();
	bool checkDifferentValue(const dumpInfo&);
	uintptr_t getAddressFromOffset(uintptr_t offset) const;
};

inline Offsets offsets;