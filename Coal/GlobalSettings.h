#pragma once
#include "LuaEnv.h"

import <fstream>;
import <vector>;
import <iostream>;

class GlobalSettings
{
public:
	GlobalSettings();

	LuaApiSettings luaApiSettings;

	bool showStateAddressTip = true;

	void init(const std::wstring& settingsPath);

private:
	class Descriptor
	{
	public:
		Descriptor(const char* name, size_t offset);
		void deserialize(const GlobalSettings* self, const std::string& value) const;
		void serialize(const GlobalSettings* self, std::ofstream& stream) const;
		bool& getValue(const GlobalSettings* self) const;
		const std::string& getName() const { return name; }
	private:
		std::string name;
		size_t offset = 0; // from GlobalSettings
	};

	bool readFromFile(const std::wstring& settingsPath);
	void writeToFile(const std::wstring& settingsPath);

	void createDescriptors();

	template<typename... Args>
	void newDescriptor(Args&&... args)
	{
		descriptors.push_back(Descriptor(std::forward<Args>(args)...));
	}

	Descriptor* getDescriptor(const std::string& name);

	std::vector<Descriptor> descriptors;
};

inline GlobalSettings globalSettings;