export module LuaStateWatcher;

import LuauTypes;
import RiblixStructures;
import TaskList;

import <map>;

export struct StateInfo
{
	StateInfo(lua_State* L);

	DataModel* dataModel = nullptr;
	lua_State* const L = nullptr;
};

export class FetchDataModelForStateTask : public Task
{
public:
	FetchDataModelForStateTask(StateInfo* info);

	Type getType() const override { return Type::FetchDataModelInfo; }
	bool execute() override;

private:
	StateInfo* info;
};

export class GlobalStateWatcher
{
public:
    using map_t = std::map<lua_State*, StateInfo>;

    GlobalStateWatcher();

    void onDataModelClosing(DataModel* dataModel);
    void onGlobalStateCreated(lua_State* L);
    void onGlobalStateRemoving(lua_State* L);

private:
    void addState(lua_State* L);
    map_t::iterator removeState(map_t::iterator pos);
    map_t::iterator removeState(lua_State* L);
    void removeAssociatedStates(DataModel* with);

    map_t states;
};