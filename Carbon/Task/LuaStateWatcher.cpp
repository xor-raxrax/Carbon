module LuaStateWatcher;

import LuauTypes;
import RiblixStructures;
import Console;
import TaskList;
import DataModelWatcher;

StateInfo::StateInfo(lua_State* L)
    : L(L)
{

}

DataModel* getAssociatedDataModel(const lua_State* L)
{
    if (auto extraSpace = L->userdata)
        if (auto scriptContext = extraSpace->scriptContext)
            if (auto parent = scriptContext->parent)
                if (parent->getClassName() == "DataModel")
                    return DataModel::toDataModel(parent);

    return nullptr;
}


FetchDataModelForStateTask::FetchDataModelForStateTask(StateInfo* info)
	: info(info)
{
}

bool FetchDataModelForStateTask::execute()
{
	auto dataModel = getAssociatedDataModel(info->L);
	if (!dataModel)
		return false;

	info->dataModel = dataModel;
    dataModelWatcher.onDataModelFetchedForState(dataModel);
	return true;
}

GlobalStateWatcher::GlobalStateWatcher()
{

}

void GlobalStateWatcher::onDataModelClosing(DataModel* dataModel)
{
    removeAssociatedStates(dataModel);
}

void GlobalStateWatcher::onGlobalStateCreated(lua_State* L)
{
    Console::getInstance() << "adding global state " << L << std::endl;
    addState(L);
}

void GlobalStateWatcher::onGlobalStateRemoving(lua_State* L)
{
    Console::getInstance() << "removing global state " << L << std::endl;
    removeState(L);
}

void GlobalStateWatcher::addState(lua_State* L)
{
    auto pos = states.find(L);
    if (pos != states.end())
    {
        Console::getInstance() << "attempt to add duplicate global state " << L << std::endl;
        return;
    }

    auto dataModel = getAssociatedDataModel(L);
    StateInfo info(L);
    info.dataModel = dataModel;

    auto emplaced = states.emplace(L, std::move(info));
    if (!dataModel)
    {
        auto info = &emplaced.first->second;
        auto task = std::make_unique<FetchDataModelForStateTask>(info);
        taskListProcessor.add(std::move(task));
    }
}

GlobalStateWatcher::map_t::iterator GlobalStateWatcher::removeState(map_t::iterator pos)
{
    return states.erase(pos);
}

GlobalStateWatcher::map_t::iterator GlobalStateWatcher::removeState(lua_State* L)
{
    auto pos = states.find(L);
    if (pos == states.end())
    {
        Console::getInstance() << "attempt to remove unknown global state " << L << std::endl;
        return pos;
    }
    return removeState(pos);
}

void GlobalStateWatcher::removeAssociatedStates(DataModel* with)
{
    auto iter = states.begin();
    while (iter != states.end())
    {
        if (iter->second.dataModel == with)
        {
            onGlobalStateRemoving(iter->first);
            iter = removeState(iter);
        }
        else
        {
            iter++;
        }
    }
}