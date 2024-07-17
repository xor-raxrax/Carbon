export module TaskList;

import <list>;
import <functional>;
import <optional>;
import <mutex>;

export class Task
{
public:
    friend class TaskListProcessor;
    enum class Type
    {
        FetchDataModelInfo,
        FetchDataModelForState,
    };

    Task();

    virtual bool execute() = 0;
    virtual Type getType() const = 0;

private:
    int tries = 0;
};

export class TaskList
{
public:
    using taskList_t = std::list<std::unique_ptr<Task>>;
    using taskList_visitor = std::function<bool(const Task*)>;

    void add(std::unique_ptr<Task> task);
    void remove(taskList_t::iterator& iter);

    template <typename TaskDerived>
    std::optional<taskList_t::iterator> find(Task::Type type, std::function<bool(const TaskDerived*)> visitor)
        requires std::is_base_of_v<Task, TaskDerived>
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto pos = std::find_if(
            tasks.begin(),
            tasks.end(),
            [&](const std::unique_ptr<Task>& task) {
                if (task->getType() != type)
                    return false;

                return visitor(static_cast<const TaskDerived*>(task.get()));
            }
        );

        if (pos == tasks.end())
            return std::nullopt;

        return pos;
    }

protected:
    taskList_t tasks;
    std::mutex mutex;
};

export class TaskListProcessor : public TaskList
{
public:
    TaskListProcessor();
    void createRunThread();
private:
    void processTasks();
    int maxRetries = 30;
    int sleepTimeMs = 30;
    bool alive = true;
};

export inline TaskListProcessor taskListProcessor;