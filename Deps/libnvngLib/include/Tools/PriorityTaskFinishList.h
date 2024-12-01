#pragma once

#include <functional>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <memory>

struct TaskInfo;
class PriorityTaskFinishList : public std::enable_shared_from_this<PriorityTaskFinishList>
{
public :
        typedef std::function<void(const std::string& key)> TaskFuncType;
        typedef std::function<void(void)> FinallyTaskFuncType;

        PriorityTaskFinishList();
        ~PriorityTaskFinishList();

        bool AddTask(const std::vector<std::string>& preTaskList, std::string_view key, const TaskFuncType& cb);
        void Clear();
        bool IsEmpty();
        bool IsFinish(std::string_view key);
        void Finish(std::string_view key);
        void AddFinalTaskCallback(const FinallyTaskFuncType& finalTaskCallback);
        std::vector<std::string> GetRunButNotFinishedTask();
        void CheckAndExecute();

private :
	std::map<std::string, std::shared_ptr<TaskInfo>> mTaskList;
	std::vector<FinallyTaskFuncType> mFinalTaskCallbackList;
	bool mIsRunFinalTaskCallback = false;
        SpinLock _mutex;
};
typedef std::shared_ptr<PriorityTaskFinishList> PriorityTaskFinishListPtr;

// vim: fenc=utf8:expandtab:ts=8:noma
