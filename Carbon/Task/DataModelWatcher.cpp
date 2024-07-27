module DataModelWatcher;

import RiblixStructures;
import TaskList;
import Console;
import GlobalState;

DataModelInfo::DataModelInfo(DataModel* dataModel)
	: dataModel(dataModel)
{
}

void DataModelInfo::setDataModel(DataModelType _type)
{
	if (type != _type)
	{
		type = _type;
		watcher->onDataModelInfoSet(this);
	}
}

DataModelType getDataModelType(const DataModel* dataModel)
{
	auto asInstance = dataModel->toInstance();
	if (asInstance->children->empty())
		return DataModelType::Invalid;

	std::string name;
	name.reserve(100);

	for (auto& child : *asInstance->children)
	{
		name = child->getClassName();
		if (name == "NetworkServer")
			return DataModelType::Server;

		if (name == "NetworkClient")
			return DataModelType::Client;
	}

	return DataModelType::Unknown;
}

bool DataModelInfo::tryFetchInfo()
{
	auto fetchedType = getDataModelType(dataModel);
	if (fetchedType == DataModelType::Invalid)
		return false;

	setDataModel(fetchedType);

	auto a = std::make_unique<AvailableLuaStateReportTask>();
	taskListProcessor.add(std::move(a));

	return true;
}

FetchDataModelInfoTask::FetchDataModelInfoTask(DataModelInfo* info)
	: Task()
	, info(info)
{

}

bool FetchDataModelInfoTask::execute()
{
	return info->tryFetchInfo();
}

const char* toString(DataModelType type)
{
	switch (type)
	{
	case DataModelType::Server: return "server";
	case DataModelType::Client: return "client";
	case DataModelType::Unknown: return "unknown";
	}
	return "invalid";
}

bool AvailableLuaStateReportTask::execute()
{
	reportAvailableLuaStates();
	return true;
}

void AvailableLuaStateReportTask::reportAvailableLuaStates() const
{
	std::scoped_lock<std::recursive_mutex> lock(dataModelWatcher.getMutex());
	// keep in sync with OnAvailableStatesReportReceived
	auto writer = globalState.createPrivateWriteBuffer(PipeOp::ReportAvailableEnvironments);

	writer.writeU32((uint32_t)dataModelWatcher.dataModels.size());

	for (const auto& [dataModel, info] : dataModelWatcher.dataModels)
	{
		writer.writeU64((uintptr_t)dataModel);
		writer.writeU8((uint8_t)info.type);

		auto states = dataModelWatcher.stateWatcher.getAssociatedStates(dataModel);
		writer.writeU32((uint32_t)states.size());

		for (const auto& info : states)
		{
			writer.writeU64((uintptr_t)info->mainThread);
		}

	}

	writer.send();
}

DataModelWatcher::DataModelWatcher()
{
}

void DataModelWatcher::onDataModelClosing(DataModel* dataModel)
{
	Console::getInstance() << "closed DM " << dataModel << std::endl;
	stateWatcher.onDataModelClosing(dataModel);
	removeDataModel(dataModel);
}

void DataModelWatcher::onDataModelInfoSet(DataModelInfo* info)
{
	if (!taskListProcessor.contains(Task::Type::AvailableLuaStateReport))
	{
		Console::getInstance() << "pushing inform task" << std::endl;
		auto task = std::make_unique<AvailableLuaStateReportTask>();
		taskListProcessor.add(std::move(task));
	}
}

void DataModelWatcher::onDataModelFetchedForState(DataModel* dataModel)
{
	if (tryAddDataModel(dataModel))
		Console::getInstance() << "added new DM from fetch " << dataModel << std::endl;
}

GlobalStateInfo* DataModelWatcher::getStateByAddress(uintptr_t address)
{
	return stateWatcher.getStateByAddress(address);
}

void DataModelWatcher::addDataModel(DataModel* dataModel)
{
	Console::getInstance() << "added DM " << dataModel << std::endl;

	auto emplaced = dataModels.emplace(dataModel, DataModelInfo(dataModel));

	auto& info = emplaced.first->second;
	bool needsDelayedFetch = !info.tryFetchInfo();

	if (needsDelayedFetch)
	{
		auto task = std::make_unique<FetchDataModelInfoTask>(&info);
		taskListProcessor.add(std::move(task));
	}
}

bool DataModelWatcher::tryAddDataModel(DataModel* dataModel)
{
	std::scoped_lock<std::recursive_mutex> lock(mutex);
	if (dataModels.contains(dataModel))
		return false;

	addDataModel(dataModel);
	return true;
}

void DataModelWatcher::removeDataModel(DataModel* dataModel)
{
	std::scoped_lock<std::recursive_mutex> lock(mutex);
	dataModels.erase(dataModel);

	auto pos = taskListProcessor.find<FetchDataModelInfoTask>(
		Task::Type::FetchDataModelInfo,
		[&](const FetchDataModelInfoTask* task) -> bool {
			return task->getInfo()->dataModel == dataModel;
		}
	);

	if (!pos)
		return;

	taskListProcessor.remove(pos.value());
}