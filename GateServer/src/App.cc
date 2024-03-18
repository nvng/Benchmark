#include "App.h"

MAIN_FUNC();

App::App(const std::string& appName)
	: SuperType(appName, E_ST_Gate)
{
	NetMgrImpl::CreateInstance();
	PlayerMgr::CreateInstance();
        ::nl::net::client::ClientNetMgr::CreateInstance();

	GlobalSetup_CH::CreateInstance();
}

App::~App()
{
	NetMgrImpl::DestroyInstance();
	PlayerMgr::DestroyInstance();
        ::nl::net::client::ClientNetMgr::DestroyInstance();

	GlobalSetup_CH::DestroyInstance();
}

bool App::Init()
{
	LOG_FATAL_IF(!SuperType::Init(), "super init fail!!!");
	_gateCfg = ServerCfgMgr::GetInstance()->_gateCfg;
        auto serverInfo = GetServerInfo<stGateServerInfo>();

	LOG_FATAL_IF(!GlobalSetup_CH::GetInstance()->Init(), "初始化策划全局配置失败!!!");
	LOG_FATAL_IF(!NetMgrImpl::GetInstance()->Init(), "net mgr init fail!!!");
	LOG_FATAL_IF(!PlayerMgr::GetInstance()->Init(), "player mgr init fail!!!");
	LOG_FATAL_IF(!::nl::net::client::ClientNetMgr::GetInstance()->Init(serverInfo->_netProcCnt, "cli"), "client net mgr init fail!!!");

        ::nl::util::SteadyTimer::StartForever(1.0, []() {
		std::size_t lobbyPlayerCnt = 0;
		std::size_t gamePlayerCnt = 0;
		NetMgrImpl::GetInstance()->Foreach([&lobbyPlayerCnt, &gamePlayerCnt](const auto& ses) {
			auto lobbySes = std::dynamic_pointer_cast<GateLobbySession>(ses);
			if (lobbySes)
				lobbyPlayerCnt += lobbySes->GetPlayerCnt();
			auto gameSes = std::dynamic_pointer_cast<GateGameSession>(ses);
			if (gameSes)
				gamePlayerCnt += gameSes->GetPlayerCnt();
		});

                [[maybe_unused]] static int64_t oldCnt = 0;
                [[maybe_unused]] static int64_t oldLobbyPlayerCnt = 0;
                [[maybe_unused]] static int64_t oldGamePlayerCnt = 0;
                [[maybe_unused]] static int64_t clientRecvCnt = 0;
                [[maybe_unused]] static int64_t serverRecvCnt = 0;
                [[maybe_unused]] static int64_t gameRecvCnt = 0;
                [[maybe_unused]] static int64_t loginRecvCnt = 0;
                LOG_INFO_IF(0 != GetApp()->_testCnt
                            || 0 != lobbyPlayerCnt - oldLobbyPlayerCnt
                            || 0 != gamePlayerCnt - oldGamePlayerCnt
                            || 0 != GetApp()->_clientRecvCnt - clientRecvCnt
                            || 0 != GetApp()->_serverRecvCnt - serverRecvCnt
                            || 0 != GetApp()->_gameRecvCnt - gameRecvCnt
                            || 0 != GetApp()->_loginRecvCnt - loginRecvCnt,
                            "cnt[{}] sesCnt[{}] pCnt[{}] lpCnt[{}] gpCnt[{}] client[{}] lobby[{}] game[{}] login[{}] avg[{}]",
                            GetApp()->_testCnt,
                            NetMgrImpl::GetInstance()->GetSessionCnt(),
                            PlayerMgr::GetInstance()->GetPlayerCnt(),
                            lobbyPlayerCnt,
                            gamePlayerCnt,
                            GetApp()->_clientRecvCnt - clientRecvCnt,
                            GetApp()->_serverRecvCnt - serverRecvCnt,
                            GetApp()->_gameRecvCnt - gameRecvCnt,
                            GetApp()->_loginRecvCnt - loginRecvCnt,
                            GetFrameController().GetAverageFrameCnt()
                           );

                oldCnt = GetApp()->_testCnt;
                oldLobbyPlayerCnt = lobbyPlayerCnt;
                oldGamePlayerCnt = gamePlayerCnt;
                clientRecvCnt = GetApp()->_clientRecvCnt;
                serverRecvCnt = GetApp()->_serverRecvCnt;
                gameRecvCnt = GetApp()->_gameRecvCnt;
                loginRecvCnt = GetApp()->_loginRecvCnt;

                return true;
	});

	// {{{ start task
	_startPriorityTaskList->AddFinalTaskCallback([this]() {
                auto gateInfo = GetServerInfo<stGateServerInfo>();
                ::nl::net::client::ClientNetMgr::GetInstance()->Listen(gateInfo->_client_port, [](auto&& s, const auto& sslCtx) {
                        return std::make_shared<GateClientSession>(std::move(s));
                });
	});

	_startPriorityTaskList->AddTask(GateGameSession::scPriorityTaskKey, [](const std::string& key) {
		ServerListCfgMgr::GetInstance()->Foreach<stGameServerInfo>([](const stGameServerInfoPtr& sInfo) {
                        ::nl::net::NetMgr::GetInstance()->Connect(sInfo->_ip, sInfo->_gate_port, [](auto&& s) {
                                return std::make_shared<GateGameSession>(std::move(s));
                        });
		});
	}, {}, ServerListCfgMgr::GetInstance()->GetSize<stGameServerInfo>());

	_startPriorityTaskList->AddTask(GateLobbySession::scPriorityTaskKey, [](const std::string& key) {
		ServerListCfgMgr::GetInstance()->Foreach<stLobbyServerInfo>([](const stLobbyServerInfoPtr& sInfo) {
                        ::nl::net::NetMgr::GetInstance()->Connect(sInfo->_ip, sInfo->_gate_port, [](auto&& s) {
                                return std::make_shared<GateLobbySession>(std::move(s));
                        });
		});
	}
        , { GateGameSession::scPriorityTaskKey }
        , ServerListCfgMgr::GetInstance()->GetSize<stLobbyServerInfo>());

        /*
	_startPriorityTaskList->AddTask(GateLoginSession::scPriorityTaskKey, [](const std::string& key) {
		ServerListCfgMgr::GetInstance()->Foreach<stLoginServerInfo>([](const auto& sInfo) {
                        ::nl::net::NetMgr::GetInstance()->Connect(sInfo->_ip, sInfo->_gate_port, [](auto&& s) {
                                return std::make_shared<GateLoginSession>(std::move(s));
                        });
		});
	}, { GateLobbySession::scPriorityTaskKey });
        */

	// }}}

	// {{{ stop task
	_stopPriorityTaskList->AddFinalTaskCallback([]() {
                ::nl::net::client::ClientNetMgr::GetInstance()->Terminate();
                ::nl::net::client::ClientNetMgr::GetInstance()->WaitForTerminate();
	});
	// }}}

	return true;
}

// vim: fenc=utf8:expandtab:ts=8
