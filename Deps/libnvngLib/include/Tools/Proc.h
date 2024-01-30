#pragma once

#include <functional>
#include <thread>
#include <atomic>
#include <assert.h>

#include "Tools/Util.h"
#include "Tools/Timer.h"
#include "Tools/FrameController.h"
#include "Tools/DoubleQueue.hpp"
#include "Tools/Clock.h"

#define CHECK_PROC_THREAD_SAFE(proc)	assert(std::this_thread::get_id() == (proc)->_threadID)

enum EProcFlagType
{
        E_PFT_IsStart = 1ul << 0,
        E_PFT_IsTerminate = 1ul << 1,
};

class Proc
{
protected :
        explicit Proc(const std::string& procName, int64_t idx=0)
                : _procName(procName)
        {
        }
        virtual ~Proc() {}

public :
        template <typename _Ty, typename... Args>
        static _Ty* Create(const std::string& procName, const Args& ... args)
        {
                auto proc = new _Ty(procName, std::forward<const Args>(args)...);
                std::thread th([proc]()
                               {
                                       proc->Init();
                                       proc->Start();
                               });
                proc->_thread.swap(th);
                return proc;
        }

        virtual bool Init()
        {
                SetThreadName("{}", _procName);

                GetCurProcRef() = this;
                _threadID = std::this_thread::get_id();
                GetFrameController().SetFrameCntPerSec(109);
                GetFrameController().Update(false);
                return true;
        }

        virtual bool Start()
        {
                FLAG_ADD(_internalFlag, E_PFT_IsStart);
                while (!IsTerminate())
                        FrameUpdate(false);

                return true;
        }

        virtual void FrameUpdate(bool updateTime)
        {
                CHECK_PROC_THREAD_SAFE(this);

                GetFrameController().Update(updateTime);

                GetTimer().Update();
                GetSteadyTimer().Update();
                GetFrameEventLoop().Update();

                DealTaskList();
        }

        FORCE_INLINE bool IsStart() const { return FLAG_HAS(_internalFlag, E_PFT_IsStart); }
        FORCE_INLINE bool IsTerminate() const { return FLAG_HAS(_internalFlag, E_PFT_IsTerminate); }
        virtual void Terminate() { FLAG_ADD(_internalFlag, E_PFT_IsTerminate); }
        virtual void WaitForTerminate()
        {
                if (_thread.joinable())
                        _thread.join();
        }

public :
        std::thread::id	 _threadID;
        std::thread      _thread;

public :
        FORCE_INLINE const std::string& GetProcName() const { return _procName; }
protected :
        std::string _procName;
        std::atomic_uint64_t _internalFlag = 0;

public :
        typedef std::function<void(void)> TaskFuncType;
        FORCE_INLINE void PostTask(TaskFuncType&& cb)
        { _taskList.Push(std::forward<TaskFuncType>(cb)); }

        FORCE_INLINE void DealTaskList()
        {
                auto* taskList = _taskList.GetQueue();
                for (auto& cb : *taskList)
                        cb();
        }

private :
        DoubleQueue<TaskFuncType> _taskList;

protected :
        DISABLE_COPY_AND_ASSIGN(Proc);
};

// vim: fenc=utf8:expandtab:ts=8:noma
