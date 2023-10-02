#include "App.h"

#include "GameLobbySession.h"
#include "GameGateSession.h"
#include "GameMgrSession.h"
#include "Jump/Region.h"
#include "Net/ISession.hpp"
#include "RegionMgr.h"
#include "NetMgrImpl.h"
#include "Tools/ServerList.hpp"
#include "Tools/TimerMgr.hpp"

AppBase* GetAppBase()
{
        return App::GetInstance();
}

App* GetApp()
{
        return App::GetInstance();
}

App::App(const std::string& appName)
: SuperType(appName, E_ST_Game)
{
        GlobalSetup_CH::CreateInstance();

        RegionMgr::CreateInstance();
        NetMgrImpl::CreateInstance();
}

App::~App()
{
        RegionMgr::DestroyInstance();
        NetMgrImpl::DestroyInstance();

        GlobalSetup_CH::DestroyInstance();
}

bool App::Init()
{
        LOG_FATAL_IF(!SuperType::Init(), "AppBase init error!!!");

        LOG_FATAL_IF(!GlobalSetup_CH::GetInstance()->Init(), "读取策划全局配置失败!!!");
        LOG_FATAL_IF(!RegionMgr::GetInstance()->Init(), "RegionMgr init error!!!");

        GetSteadyTimer().StartWithRelativeTimeForever(1.0, [](TimedEventItem& eventData) {
                static int64_t oldRegionCreateCnt = 0;
                static int64_t oldRegionDestroyCnt = 0;
                static int64_t oldCnt = 0;
                (void)oldRegionCreateCnt;
                (void)oldRegionDestroyCnt;
                (void)oldCnt;

                LOG_INFO_IF(true, "cnt[{}] actorCnt[{}] rc[{}] rd[{}] avg[{}]",
                            GetApp()->_cnt - oldCnt,
                            RegionMgr::GetInstance()->GetActorCnt(),
                            GetApp()->_regionCreateCnt - oldRegionCreateCnt,
                            GetApp()->_regionDestroyCnt - oldRegionDestroyCnt,
                            GetFrameController().GetAverageFrameCnt()
                            );

                oldRegionCreateCnt = GetApp()->_regionCreateCnt;
                oldRegionDestroyCnt = GetApp()->_regionDestroyCnt;
                oldCnt = GetApp()->_cnt;
        });

        // {{{ start task
        _startPriorityTaskList->AddFinalTaskCallback([]() {
                NetMgr::GetInstance()->Listen(GetApp()->GetServerInfo<stGameServerInfo>()->_gate_port, [](auto&& s, auto& sslCtx) {
                        return std::make_shared<GameGateSession>(std::move(s));
                });

                LOG_INFO("server start success");
        });

        std::vector<std::string> preTask;
        preTask.clear();
        _startPriorityTaskList->AddTask(preTask, GameMgrSession::scPriorityTaskKey, [](const std::string& key) {
                auto gameMgrServerInfo = ServerListCfgMgr::GetInstance()->GetFirst<stGameMgrServerInfo>();
                NetMgr::GetInstance()->Connect(gameMgrServerInfo->_ip, gameMgrServerInfo->_game_port, [](auto&& s) {
                        return std::make_shared<GameMgrSession>(std::move(s));
                });
        });


        preTask.clear();
        preTask.emplace_back(GameMgrSession::scPriorityTaskKey);
        _startPriorityTaskList->AddTask(preTask, GameLobbySession::scPriorityTaskKey, [](const std::string& key) {
                ServerListCfgMgr::GetInstance()->Foreach<stLobbyServerInfo>([](const stLobbyServerInfoPtr& sInfo) {
                        NetMgr::GetInstance()->Connect(sInfo->_ip, sInfo->_game_port, [](auto&& s) {
                                return std::make_shared<GameLobbySession>(std::move(s));
                        });
                });
        });
        // }}}

        // {{{ stop task
        _stopPriorityTaskList->AddFinalTaskCallback([]() {
                RegionMgr::GetInstance()->Terminate();
                RegionMgr::GetInstance()->WaitForTerminate();
        });
        // }}}

        return true;
}

int main(int argc, char* argv[])
{
        App::CreateInstance(argv[0]);
        INIT_OPT();

        LOG_FATAL_IF(!App::GetInstance()->Init(), "app init error!!!");
        App::GetInstance()->Start();
        App::DestroyInstance();

        return 0;
}

NET_MSG_HANDLE(GameMgrSession, 0xff, 0xf)
{
        ++GetApp()->_cnt;
        static auto ret = std::make_shared<MailResult>();
        SendPB(ret, 0xff, 0xf, MsgHeaderType::E_RMT_CallRet, msgHead._guid, msgHead._to, msgHead._from);
}

// vim: fenc=utf8:expandtab:ts=8
