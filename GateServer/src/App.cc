#include "App.h"

#include <malloc.h>

using namespace nl::af::impl;

AppBase* GetAppBase()
{
	return App::GetInstance();
}

App* GetApp()
{
	return App::GetInstance();
}

App::App()
{
        ServerListCfgMgr::CreateInstance();
	ServerCfgMgr::CreateInstance();

	NetProcMgr::CreateInstance();
	NetMgr::CreateInstance();
	PlayerMgr::CreateInstance();
}

App::~App()
{
	NetProcMgr::DestroyInstance();
	NetMgr::DestroyInstance();
	PlayerMgr::DestroyInstance();

        ServerListCfgMgr::DestroyInstance();
	ServerCfgMgr::DestroyInstance();
}

bool App::Init(const std::string& appName)
{
        LOG_FATAL_IF(!ServerListCfgMgr::GetInstance()->Init("./config_cx/server_list.json"), "服务器列表初始化失败!!!");
        LOG_FATAL_IF(!ServerCfgMgr::GetInstance()->Init("./config_cx/server_cfg.json"), "服务器配置初始化失败!!!");

        ServerListCfgMgr::GetInstance()->_gateServerList.Foreach([](const stGateServerInfoPtr& sInfo) {
                if (GetApp()->_gateInfo)
                        return;
                auto ip = GetIP(AF_INET, sInfo->_faName);
		if (ip == sInfo->_ip
		    && RunShellCmd(fmt::format("netstat -apn | grep 0.0.0.0:{}", sInfo->_client_port)).empty())
			GetApp()->_gateInfo = sInfo;
        });
	LOG_FATAL_IF(!_gateInfo, "server info not found!!!");
	_gateCfg = ServerCfgMgr::GetInstance()->_gateCfg;

	LOG_FATAL_IF(!SuperType::Init(appName, ServerListCfgMgr::GetInstance()->_rid, _gateInfo->_sid, 0), "super init fail!!!");
	LOG_FATAL_IF(!NetProcMgr::GetInstance()->Init(4, "Net"), "net proc mgr init fail!!!");
	LOG_FATAL_IF(!NetMgr::GetInstance()->Init(), "net mgr init fail!!!");
	LOG_FATAL_IF(!PlayerMgr::GetInstance()->Init(), "player mgr init fail!!!");

	int64_t idx = 0;
	auto proc = NetProcMgr::GetInstance()->Dist(++idx);
	// proc->StartListener("0.0.0.0", _gateInfo->_client_port, _gateCfg->_crt, _gateCfg->_key, []() { return CreateSession<GateClientSession>(); });
	proc->StartListener("0.0.0.0", _gateInfo->_client_port, "", "", []() { return CreateSession<GateClientSession>(); });

        ServerListCfgMgr::GetInstance()->_lobbyServerList.Foreach([&idx](const stLobbyServerInfoPtr& sInfo) {
		for (int64_t i=0; i<std::max<int64_t>(NetProcMgr::GetInstance()->GetProcCnt()-1, 1); ++i)
		{
			auto proc = NetProcMgr::GetInstance()->Dist(++idx);
			proc->Connect(sInfo->_ip, sInfo->_gate_port, false, []() { return CreateSession<GateLobbySession>(); });
		}
	});

	GetSteadyTimer().StartWithRelativeTimeForever(1.0, [](TimedEventItem& eventData) {
		std::size_t lobbyPlayerCnt = 0;
		std::size_t gamePlayerCnt = 0;
		NetMgr::GetInstance()->Foreach([&lobbyPlayerCnt, &gamePlayerCnt](const auto& ses) {
			auto lobbySes = std::dynamic_pointer_cast<GateLobbySession>(ses);
			if (lobbySes)
				lobbyPlayerCnt += lobbySes->GetPlayerCnt();
			auto gameSes = std::dynamic_pointer_cast<GateGameSession>(ses);
			if (gameSes)
				gamePlayerCnt += gameSes->GetPlayerCnt();
		});

		LOG_INFO_IF(true, "sesCnt[{}] pCnt[{}] lpCnt[{}] gpCnt[{}] avg[{}]",
			    NetMgr::GetInstance()->GetSessionCnt(),
			    PlayerMgr::GetInstance()->GetPlayerCnt(),
			    lobbyPlayerCnt,
			    gamePlayerCnt,
			    GetFrameController().GetAverageFrameCnt()
			   );

		// malloc_trim(0);
	});

	return true;
}

void App::Stop()
{
	NetProcMgr::GetInstance()->Terminate();

	NetProcMgr::GetInstance()->WaitForTerminate();

	SuperType::Stop();
}

// vim: fenc=utf8:expandtab:ts=8
