#pragma once

#include "Tools/Util.h"
#include "Tools/Clock.h"
#include "Tools/Proc.h"
#include "Tools/PriorityTaskFinishList.h"
#include "Tools/ServerList.hpp"
#include "Tools/TimerMgr.hpp"
#include <chrono>
#include <ratio>

enum EAppFlagType
{
        E_AFT_Maintance = 1ul << 63,
        E_AFT_Daemon = 1ul << 62,
        E_AFT_Crash = 1ul << 61,
        E_AFT_InDocker = 1ul << 60,
        E_AFT_InitSuccess = 1ul << 59,
        E_AFT_StartSilent = 1ul << 58,
        E_AFT_StartStop = 1ul << 57,
};

#define DEFINE_APP_FLAG(flag) \
        FORCE_INLINE void Set##flag() { FLAG_ADD(_internalFlag, E_AFT_##flag); } \
        FORCE_INLINE bool Is##flag() const { return FLAG_HAS(_internalFlag, E_AFT_##flag); }

class AppBase;
extern AppBase* GetAppBase();

class AppBase : public Proc
{
        typedef Proc SuperType;
protected :
        AppBase(const std::string& appName, EServerType t);
        virtual ~AppBase();

public :
	bool Init() override;
        bool Start() override
        {
                // LOG_INFO("proc {} start success!!!", GetProcName());
                ::nl::util::SteadyTimer::StartForever(1.0, []() {
                        LogHelper::GetInstance()->Flush();
                        return true;
                });

                ::nl::util::SteadyTimer::StartForever(0.01, []() {
                        GetClock().UpdateTime();
                        return true;
                });

                _startPriorityTaskList->CheckAndExecute();
                ::nl::util::SteadyTimer::StartForever(10.0, 1.0, []() {
                        const auto taskKeyList = GetAppBase()->_startPriorityTaskList->GetRunButNotFinishedTask();
                        if (!taskKeyList.empty())
                        {
                                std::string printStr;
                                for (auto& taskKey : taskKeyList)
                                {
                                        printStr += taskKey;
                                        printStr += " ";
                                }
                                LOG_WARN("init task not finish : {}", printStr);
                                return true;
                        }
                        return false;
                });
                return SuperType::Start();
        }

        void FrameUpdate(bool updateTime) override;
        virtual void Stop()
        {
                SetStartStop();

                /*
                // 全部线程需要统一时间，由主线程统一更新，
                // 但在 stop 后，由于 WaitForTerminate 导致
                // 无法更新时间，从而有些 timer 相关的逻辑可能无法退出。
                std::thread([]() {
                        while (nullptr != GetAppBase() && !GetAppBase()->IsTerminate())
                        {
                                GetClock().UpdateTime();
                                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        }
                }).detach();
                */

                _stopPriorityTaskList->CheckAndExecute();
                ::nl::util::SteadyTimer::StartForever(10.0, 1.0, []() {
                        std::vector<std::string> taskKeyList = GetAppBase()->_stopPriorityTaskList->GetRunButNotFinishedTask();
                        if (!taskKeyList.empty())
                        {
                                std::string printStr;
                                for (auto& taskKey : taskKeyList)
                                {
                                        printStr += taskKey;
                                        printStr += " ";
                                }
                                LOG_WARN("stop task not finish : {}", printStr);
                                return true;
                        }
                        return false;
                });
        }

        template <typename _Ty>
        FORCE_INLINE std::shared_ptr<_Ty> GetServerInfo() const { return std::dynamic_pointer_cast<_Ty>(_serverInfo); }
        FORCE_INLINE const std::string& GetAppName() const { return GetProcName(); }
        FORCE_INLINE int64_t GetRID() const { return ServerListCfgMgr::GetInstance()->_rid; }
        FORCE_INLINE int64_t GetSID() const { return _serverInfo->_sid; }
        FORCE_INLINE EServerType GetServerType() const { return _serverType; }

        DEFINE_APP_FLAG(InitSuccess);
        DEFINE_APP_FLAG(Crash);
        DEFINE_APP_FLAG(Daemon);
        DEFINE_APP_FLAG(StartSilent);
        DEFINE_APP_FLAG(StartStop);
        DEFINE_APP_FLAG(InDocker);
        DEFINE_APP_FLAG(Maintance);

private :
        std::string _markFileName;
        const EServerType _serverType = E_ST_None;
protected :
        stServerInfoBasePtr _serverInfo;

public :
        boost::fibers::buffered_channel<std::function<void()>> _mainChannel;
        std::vector<std::thread> _threadList;
        PriorityTaskFinishListPtr _startPriorityTaskList = std::make_shared<PriorityTaskFinishList>();
        PriorityTaskFinishListPtr _stopPriorityTaskList = std::make_shared<PriorityTaskFinishList>();

public :
        FORCE_INLINE Clock& GetClockInternal() { return _clock; }
private :
        Clock _clock;

protected :
	DISABLE_COPY_AND_ASSIGN(AppBase);
};

#undef DEFINE_APP_FLAG

// vim: fenc=utf8:expandtab:ts=8:noma
