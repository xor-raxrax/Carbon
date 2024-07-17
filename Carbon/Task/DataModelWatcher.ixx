export module DataModelWatcher;

import RiblixStructures;
import TaskList;
import LuaStateWatcher;

import <mutex>;
import <map>;

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

export class DataModelWatcher
{
public:
	DataModelWatcher();

	void onDataModelClosing(DataModel* dataModel);
	void onDataModelInfoSet(DataModelInfo* info);
	void onDataModelFetchedForState(DataModel* dataModel);

	GlobalStateWatcher stateWatcher;
private:
	void addDataModel(DataModel* dataModel);
	bool tryAddDataModel(DataModel* dataModel);
	void removeDataModel(DataModel* dataModel);

	std::map<DataModel*, DataModelInfo> dataModels;
	std::mutex mutex;
};

export inline DataModelWatcher dataModelWatcher;