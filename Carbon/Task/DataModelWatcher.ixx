export module DataModelWatcher;

import RiblixStructures;
import TaskList;
import LuaStateWatcher;

import <mutex>;
import <map>;

// keep in sync with InjectionApi.cs
enum class DataModelType
{
	Invalid, // when type cannot be determined yet
	Server,
	Client,
	Unknown,
};

export class DataModelInfo
{
public:
	DataModelInfo(DataModel* dataModel);

	void setDataModel(DataModelType type);
	bool tryFetchInfo();

	DataModelType type = DataModelType::Invalid;
	DataModel* dataModel = nullptr;
	class DataModelWatcher* watcher;

};

export class FetchDataModelInfoTask : public Task
{
public:
	FetchDataModelInfoTask(DataModelInfo* info);

	bool execute() override;

	Type getType() const override { return Type::FetchDataModelForState; };
	const DataModelInfo* getInfo() const { return info; };
private:
	DataModelInfo* info;
};

class AvailableLuaStateReportTask : public Task
{
public:
	Type getType() const override { return Type::AvailableLuaStateReport; }
	bool execute() override;
private:
	void reportAvailableLuaStates() const;
};

export class DataModelWatcher
{
public:
	friend class AvailableLuaStateReportTask;

	DataModelWatcher();

	void onDataModelClosing(DataModel* dataModel);
	void onDataModelInfoSet(DataModelInfo* info);
	void onDataModelFetchedForState(DataModel* dataModel);

	GlobalStateInfo* getStateByAddress(uintptr_t address);
	GlobalStateWatcher stateWatcher;
	std::recursive_mutex& getMutex() { return mutex; };
private:
	void addDataModel(DataModel* dataModel);
	bool tryAddDataModel(DataModel* dataModel);
	void removeDataModel(DataModel* dataModel);

	std::map<DataModel*, DataModelInfo> dataModels;
	std::recursive_mutex mutex;
};

export inline DataModelWatcher dataModelWatcher;