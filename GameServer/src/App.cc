#include "App.h"

#include "GameLobbySession.h"
#include "GameGateSession.h"
#include "GameMgrSession.h"
#include "GameRobotMgrSession.h"
#include "Jump/Region.h"
#include "RegionMgr.h"
#include "NetMgr.h"

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
        NetMgr::CreateInstance();
}

App::~App()
{
        RegionMgr::DestroyInstance();
        NetMgr::DestroyInstance();

        GlobalSetup_CH::DestroyInstance();
}

bool App::Init()
{
        LOG_FATAL_IF(!SuperType::Init(), "AppBase init error!!!");
        LOG_FATAL_IF(!NetMgr::GetInstance()->Init(), "NetMgr init error!!!");

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

                auto proc = NetProcMgr::GetInstance()->Dist(1);
                proc->StartListener("0.0.0.0", GetApp()->GetServerInfo<stGameServerInfo>()->_gate_port, "", "", []() {
                        return CreateSession<GameGateSession>();
                });

                LOG_INFO("server start success");
        });

        std::vector<std::string> preTask;
        preTask.clear();
        _startPriorityTaskList->AddTask(preTask, GameMgrSession::scPriorityTaskKey, [](const std::string& key) {
                auto proc = NetProcMgr::GetInstance()->Dist(0);
                auto gameMgrServerInfo = ServerListCfgMgr::GetInstance()->GetFirst<stGameMgrServerInfo>();
                proc->Connect(gameMgrServerInfo->_ip, gameMgrServerInfo->_game_port, false, []() {
                        return CreateSession<GameMgrSession>();
                });
        });


        preTask.clear();
        preTask.emplace_back(GameMgrSession::scPriorityTaskKey);
        _startPriorityTaskList->AddTask(preTask, GameLobbySession::scPriorityTaskKey, [](const std::string& key) {
                int64_t idx = 0;
                ServerListCfgMgr::GetInstance()->Foreach<stLobbyServerInfo>([&idx](const stLobbyServerInfoPtr& sInfo) {
                        auto proc = NetProcMgr::GetInstance()->Dist(++idx);
                        proc->Connect(sInfo->_ip, sInfo->_game_port, false, []() {
                                return CreateSession<GameLobbySession>();
                        });
                });
        });
        // }}}

        // {{{ stop task
        _stopPriorityTaskList->AddFinalTaskCallback([]() {
                RegionMgr::GetInstance()->Terminate();
                NetMgr::GetInstance()->Terminate();

                RegionMgr::GetInstance()->WaitForTerminate();
                NetMgr::GetInstance()->WaitForTerminate();
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

// vim: fenc=utf8:expandtab:ts=8
