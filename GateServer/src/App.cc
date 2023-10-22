#include "App.h"
#include "GateClientSession.h"
#include "GateGameSession.h"
#include "GateLobbySession.h"
#include "Net/ISession.hpp"
#include "Net/TcpSessionForClient.hpp"
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
  : SuperType(appName, E_ST_Gate)
{
	NetMgrImpl::CreateInstance();
	PlayerMgr::CreateInstance();
        ::nl::net::client::ClientNetMgr::CreateInstance();
}

App::~App()
{
	NetMgrImpl::DestroyInstance();
	PlayerMgr::DestroyInstance();
        ::nl::net::client::ClientNetMgr::DestroyInstance();
}

bool App::Init()
{
	LOG_FATAL_IF(!SuperType::Init(), "super init fail!!!");
	_gateCfg = ServerCfgMgr::GetInstance()->_gateCfg;
        auto serverInfo = GetServerInfo<stGateServerInfo>();

	LOG_FATAL_IF(!NetMgrImpl::GetInstance()->Init(), "net mgr init fail!!!");
	LOG_FATAL_IF(!PlayerMgr::GetInstance()->Init(), "player mgr init fail!!!");
	LOG_FATAL_IF(!::nl::net::client::ClientNetMgr::GetInstance()->Init(serverInfo->_netProcCnt, "client"), "client net mgr init fail!!!");

	GetSteadyTimer().StartWithRelativeTimeForever(1.0, [](TimedEventItem& eventData) {
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

                [[maybe_unused]] static int64_t clientRecvCnt = 0;
                [[maybe_unused]] static int64_t serverRecvCnt = 0;
                [[maybe_unused]] static int64_t gameRecvCnt = 0;
                [[maybe_unused]] static int64_t loginRecvCnt = 0;
		LOG_INFO_IF(true, "cnt[{}] sesCnt[{}] pCnt[{}] lpCnt[{}] gpCnt[{}] client[{}] lobby[{}] game[{}] login[{}] avg[{}]",
                            GetApp()->_cnt,
			    NetMgrImpl::GetInstance()->GetSessionCnt(),
			    PlayerMgr::GetInstance()->GetPlayerCnt(),
			    lobbyPlayerCnt,
			    gamePlayerCnt,
                            GetApp()->_clientRecvCnt - clientRecvCnt,
                            GetApp()->_serverRecvCnt - serverRecvCnt,
                            GetApp()->_gameRecvCnt - gameRecvCnt,
                            0,
			    GetFrameController().GetAverageFrameCnt()
			   );

                clientRecvCnt = GetApp()->_clientRecvCnt;
                serverRecvCnt = GetApp()->_serverRecvCnt;
                gameRecvCnt = GetApp()->_gameRecvCnt;
                loginRecvCnt = GetApp()->_loginRecvCnt;
	});

	// {{{ start task
	_startPriorityTaskList->AddFinalTaskCallback([this]() {
                auto gateInfo = GetServerInfo<stGateServerInfo>();
                ::nl::net::client::ClientNetMgr::GetInstance()->Listen(gateInfo->_client_port, [](auto&& s, const auto& sslCtx) {
                        return std::make_shared<GateClientSession>(std::move(s));
                });
	});

	std::vector<std::string> preTaskList;

	preTaskList.clear();
	_startPriorityTaskList->AddTask(preTaskList, GateGameSession::scPriorityTaskKey, [](const std::string& key) {
		ServerListCfgMgr::GetInstance()->Foreach<stGameServerInfo>([](const stGameServerInfoPtr& sInfo) {
                        ::nl::net::NetMgr::GetInstance()->Connect(sInfo->_ip, sInfo->_gate_port, [](auto&& s) {
                                return std::make_shared<GateGameSession>(std::move(s));
                        });
		});
	});

	preTaskList.clear();
	preTaskList.emplace_back(GateGameSession::scPriorityTaskKey);
	_startPriorityTaskList->AddTask(preTaskList, GateLobbySession::scPriorityTaskKey, [](const std::string& key) {
		ServerListCfgMgr::GetInstance()->Foreach<stLobbyServerInfo>([](const stLobbyServerInfoPtr& sInfo) {
                        ::nl::net::NetMgr::GetInstance()->Connect(sInfo->_ip, sInfo->_gate_port, [](auto&& s) {
                                return std::make_shared<GateLobbySession>(std::move(s));
                        });
		});
	});
	// }}}

	// {{{ stop task
	_stopPriorityTaskList->AddFinalTaskCallback([]() {
                ::nl::net::client::ClientNetMgr::GetInstance()->Terminate();
                ::nl::net::client::ClientNetMgr::GetInstance()->WaitForTerminate();
	});
	// }}}

	return true;
}

void App::Stop()
{
	SuperType::Stop();
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
