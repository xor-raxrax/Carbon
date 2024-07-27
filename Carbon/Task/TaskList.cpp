module TaskList;

import Console;

import <memory>;
import <functional>;
import <optional>;
import <mutex>;
import <thread>;

void TaskList::add(std::unique_ptr<Task> task)
{
	std::scoped_lock<std::mutex> lock(mutex);
	tasks.push_back(std::move(task));
}

void TaskList::remove(taskList_t::iterator& iter)
{
	std::unique_lock<std::mutex> lock(mutex);
	tasks.erase(iter);
}

bool TaskList::contains(Task::Type type)
{
	return tasks.end() == std::find_if(
		tasks.begin(),
		tasks.end(),
		[&](const std::unique_ptr<Task>& task) {
			return task->getType() == type;
		}
	);
}

void TaskListProcessor::createRunThread()
{
	std::thread([&]() {
		try
		{
			while (alive)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(sleepTimeMs));
				processTasks();
			}
		}
		catch (std::exception& e)
		{
			Console::getInstance() << e.what() << std::endl;
		}
		catch (...)
		{
			Console::getInstance() << "caught something bad" << std::endl;
		}
	}).detach();
}

const char* toString(Task::Type type)
{
	switch (type)
	{
	case Task::Type::FetchDataModelInfo: return "FetchDataModelInfo";
	case Task::Type::FetchDataModelForState: return "FetchDataModelForState";
	}
	return "unknown";
}

void TaskListProcessor::processTasks()
{
	auto iter = tasks.begin();
	while (iter != tasks.end())
	{
		auto& task = *iter;
		if (task->execute())
		{
			iter = tasks.erase(iter);
			continue;
		}

		task->tries++;
		if (task->tries > maxRetries)
		{
			Console::getInstance() << "dropping task " << toString(task->getType()) << std::endl;
			iter = tasks.erase(iter);
		}
		else
		{
			iter++;
		}
	}
}