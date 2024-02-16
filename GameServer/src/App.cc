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

MAIN_FUNC();

App::App(const std::string& appName)
        : SuperType(appName, E_ST_Game)
{
        ::nl::net::client::ClientNetMgr::CreateInstance();
        GlobalSetup_CH::CreateInstance();

        RegionMgr::CreateInstance();
        NetMgrImpl::CreateInstance();
}

App::~App()
{
        ::nl::net::client::ClientNetMgr::DestroyInstance();
        RegionMgr::DestroyInstance();
        NetMgrImpl::DestroyInstance();

        GlobalSetup_CH::DestroyInstance();
}

bool App::Init()
{
        LOG_FATAL_IF(!SuperType::Init(), "AppBase init error!!!");
        LOG_FATAL_IF(!::nl::net::client::ClientNetMgr::GetInstance()->Init(1, "gate"), "ClientNetMgr init error!!!");

        LOG_FATAL_IF(!GlobalSetup_CH::GetInstance()->Init(), "读取策划全局配置失败!!!");
        LOG_FATAL_IF(!RegionMgr::GetInstance()->Init(), "RegionMgr init error!!!");

        GetSteadyTimer().StartWithRelativeTimeForever(1.0, [](TimedEventItem& eventData) {
                [[maybe_unused]] static int64_t oldRegionCreateCnt = 0;
                [[maybe_unused]] static int64_t oldRegionDestroyCnt = 0;
                [[maybe_unused]] static int64_t oldCnt = 0;

                int64_t clientCnt = 0;
                NetMgrImpl::GetInstance()->_gateSesList.Foreach([&clientCnt](const auto& s) {
                        auto ses = s.lock();
                        if (ses)
                                clientCnt += ses->GetAgentCnt();
                });

                LOG_INFO_IF(true, "cnt[{}] client[{}] actorCnt[{}] rc[{}] rd[{}] avg[{}]",
                            GetApp()->_cnt,
                            clientCnt,
                            // GetApp()->_cnt - oldCnt,
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
                ::nl::net::client::ClientNetMgr::GetInstance()->Listen(GetApp()->GetServerInfo<stGameServerInfo>()->_gate_port, [](auto&& s, auto& sslCtx) {
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
                ::nl::net::client::ClientNetMgr::GetInstance()->Terminate();
                ::nl::net::client::ClientNetMgr::GetInstance()->WaitForTerminate();

                RegionMgr::GetInstance()->Terminate();
                RegionMgr::GetInstance()->WaitForTerminate();
        });
        // }}}

        return true;
}

NET_MSG_HANDLE(GameMgrSession, 0xff, 0xf)
{
        ++GetApp()->_cnt;
        static auto ret = std::make_shared<MailResult>();
        SendPB(ret, 0xff, 0xf, MsgHeaderType::E_RMT_CallRet, msgHead._guid, msgHead._to, msgHead._from);
}

// vim: fenc=utf8:expandtab:ts=8
