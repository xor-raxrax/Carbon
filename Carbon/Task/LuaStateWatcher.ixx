export module LuaStateWatcher;

import <vector>;
import <map>;
import <mutex>;

import LuauTypes;
import RiblixStructures;
import TaskList;

export struct GlobalStateInfo
{
	GlobalStateInfo(lua_State* mainThread);

	DataModel* dataModel = nullptr;
	lua_State* const mainThread = nullptr;
	lua_State* ourMainThread = nullptr;
	bool environmentInjected = false;
};

export class FetchDataModelForStateTask : public Task
{
public:
	FetchDataModelForStateTask(GlobalStateInfo* info);

	Type getType() const override { return Type::FetchDataModelInfo; }
	bool execute() override;

private:
	GlobalStateInfo* info;
};

export class GlobalStateWatcher
{
public:
	using map_t = std::map<lua_State*, GlobalStateInfo>;

	GlobalStateWatcher();

	void onDataModelClosing(DataModel* dataModel);
	void onGlobalStateCreated(lua_State* L);
	void onGlobalStateRemoving(lua_State* L);
	std::vector<const GlobalStateInfo*> getAssociatedStates(const DataModel* with);
	GlobalStateInfo* getStateByAddress(uintptr_t address);

private:
	void addState(lua_State* L);
	map_t::iterator removeState(map_t::iterator pos);
	map_t::iterator removeState(lua_State* L);
	void removeAssociatedStates(const DataModel* with);

	map_t states;
	std::recursive_mutex mutex;
};