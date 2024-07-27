module LuaStateWatcher;

import LuauTypes;
import RiblixStructures;
import Console;
import TaskList;
import DataModelWatcher;

GlobalStateInfo::GlobalStateInfo(lua_State* mainThread)
	: mainThread(mainThread)
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


FetchDataModelForStateTask::FetchDataModelForStateTask(GlobalStateInfo* info)
	: Task()
	, info(info)
{
}

bool FetchDataModelForStateTask::execute()
{
	auto dataModel = getAssociatedDataModel(info->mainThread);
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

	auto task = std::make_unique<AvailableLuaStateReportTask>();
	taskListProcessor.add(std::move(task));
}

std::vector<const GlobalStateInfo*> GlobalStateWatcher::getAssociatedStates(const DataModel* with)
{
	std::scoped_lock<std::recursive_mutex> lock(mutex);

	std::vector<const GlobalStateInfo*> result;
	for (const auto& [_, info] : states)
		if (info.dataModel == with)
			result.push_back(&info);
	
	return result;
}

GlobalStateInfo* GlobalStateWatcher::getStateByAddress(uintptr_t address)
{
	std::scoped_lock<std::recursive_mutex> lock(mutex);
	auto pos = states.find((lua_State*)address);
	if (pos == states.end())
		return nullptr;
	return &pos->second;
}

void GlobalStateWatcher::addState(lua_State* L)
{
	std::scoped_lock<std::recursive_mutex> lock(mutex);
	auto pos = states.find(L);
	if (pos != states.end())
	{
		Console::getInstance() << "attempt to add duplicate global state " << L << std::endl;
		return;
	}

	auto dataModel = getAssociatedDataModel(L);
	GlobalStateInfo info(L);
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
	std::scoped_lock<std::recursive_mutex> lock(mutex);
	return states.erase(pos);
}

GlobalStateWatcher::map_t::iterator GlobalStateWatcher::removeState(lua_State* L)
{
	std::scoped_lock<std::recursive_mutex> lock(mutex);
	auto pos = states.find(L);
	if (pos == states.end())
	{
		Console::getInstance() << "attempt to remove unknown global state " << L << std::endl;
		return pos;
	}
	return removeState(pos);
}

void GlobalStateWatcher::removeAssociatedStates(const DataModel* with)
{
	std::scoped_lock<std::recursive_mutex> lock(mutex);

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