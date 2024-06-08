#pragma once

import <array>;
import <string>;

class Hook
{
public:

	Hook(const std::string& name, void* hook);
	void setTarget(void* _target) { target = _target; };
	void* getOriginal() { return original; };
	bool setup();
	bool remove();
private:
	const std::string name;
	void* target = nullptr;
	void* hook = nullptr;
	void* original = nullptr;
};

enum class HookId
{
	growCI,
	_Size,
};

class HookHandler
{
public:
	HookHandler();
	~HookHandler();
	void setupAll();
	void removeAll();
	constexpr const Hook& getHook(HookId id) const { return hooks.at((int)id); }
	constexpr Hook& getHook(HookId id) { return hooks.at((int)id); }
private:
	std::array<Hook, (int)HookId::_Size> hooks;
};

inline HookHandler hookHandler;