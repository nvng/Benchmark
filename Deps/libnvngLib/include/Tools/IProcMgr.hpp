#pragma once

#include "Tools/LogHelper.h"
#include "Tools/Timer.h"
#include "Tools/Proc.h"

template <typename _Ty>
class IProcMgr
{
protected :
        IProcMgr()
        {
        }

        virtual ~IProcMgr()
        {
                for (int64_t i=0; i<=_cnt; ++i)
                        delete _procArr[i];
                delete[] _procArr;
                _procArr = nullptr;
        }

public :
        template <typename... Args>
        bool Init(int64_t cnt, const std::string& procName, const Args& ... args)
        {
                LOG_FATAL_IF(!CHECK_2N(cnt), "net proc list not 2^N !!!");

                _cnt = cnt-1;
                delete[] _procArr;
                _procArr = new _Ty*[_cnt+1];
                for (int64_t i=0; i<=_cnt; ++i)
                        _procArr[i] = Proc::Create<_Ty>(fmt::format("{}_{}", procName, i), i, std::forward<const Args>(args)...);

                for (int64_t i=0; i<=_cnt; ++i)
                        while (!_procArr[i]->IsStart()) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
                return true;
        }

        FORCE_INLINE int64_t GetProcCnt() const { return _cnt + 1; }
        FORCE_INLINE _Ty* Dist(uint64_t id) { return _procArr[ id & _cnt]; }

        FORCE_INLINE void ForeachProc(const auto& cb)
        {
                for (int64_t i=0; i<=_cnt; ++i)
                        cb(_procArr[i]);
        }

        FORCE_INLINE void DelayForever(const FrameEventLoop::EventCallbackType& cb)
        {
                ForeachProc([cb{std::move(cb)}](_Ty* proc) {
                        proc->PostTask([cb{std::move(cb)}]() { GetFrameEventLoop().DelayForever(std::move(cb)); });
                });
        }

        virtual void Terminate()
        {
                for (int64_t i=0; i<=_cnt; ++i)
                        _procArr[i]->Terminate();
        }

        virtual void WaitForTerminate()
        {
                for (int64_t i=0; i<=_cnt; ++i)
                        _procArr[i]->WaitForTerminate();
        }

private :
        int64_t _cnt = 0;
        _Ty** _procArr = nullptr;
};

// vim: fenc=utf8:expandtab:ts=8:noma
