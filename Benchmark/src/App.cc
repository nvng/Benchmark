#include "App.h"

#include "Net/TcpSessionForClient.hpp"
#include "Player/PlayerMgr.h"
#include "Net/ClientGateSession.h"

MAIN_FUNC();

App::App(const std::string& appName)
  : SuperType(appName, E_ST_None)
{
	PlayerMgr::CreateInstance();
        ::nl::net::client::ClientNetMgr::CreateInstance();
}

App::~App()
{
	PlayerMgr::DestroyInstance();
        ::nl::net::client::ClientNetMgr::DestroyInstance();
}

bool App::Init()
{
	int64_t sid = 10;
        std::ifstream f("sid.txt");
	std::string str;
	getline(f, str);
	if (!str.empty())
	{
	    sid = atoll(str.c_str());
	    // printf("%s\n", fmt::format("sid[{}] str[{}]", sid, str.c_str()).c_str());
	}
	f.close();

	std::ofstream f1("sid.txt", std::ios_base::trunc);
	f1 << ((sid+1>=20) ? 10 : sid + 1);
        f1.close();

        _serverInfo = std::make_shared<stServerInfoBase>(E_ST_None);
        _serverInfo->_sid = sid;
        _serverInfo->_workersCnt = 2;
        _serverInfo->_netProcCnt = 4;

	LOG_FATAL_IF(!SuperType::Init(), "AppBase init error!!!");
	LOG_FATAL_IF(!::nl::net::client::ClientNetMgr::GetInstance()->Init(_serverInfo->_netProcCnt, "cli"), "ClientNetMgr init error!!!");
	LOG_FATAL_IF(!PlayerMgr::GetInstance()->Init(), "PlayerMgr init error!!!");

#if 1
        ::nl::util::SteadyTimer::StartForever(1.0, []() {
                [[maybe_unused]] static uint64_t cnt = 0;
                [[maybe_unused]] static uint64_t oldRecvCnt  = 0;
                // LOG_INFO("avgCost[{}] cnt[{}]", ClientGateSession::GetAvgCostTime(), GetApp()->_cnt - cnt);
                LOG_INFO("cnt[{}] rc[{}] avg[{}]"
                         , GetApp()->_cnt - cnt
                         , GetApp()->_recvCnt - oldRecvCnt
                         , GetFrameController().GetAverageFrameCnt()
                        );

                cnt = GetApp()->_cnt;
                oldRecvCnt = GetApp()->_recvCnt;

                return true;
        });
#endif

        _stopPriorityTaskList->AddFinalTaskCallback([]() {
                ::nl::net::client::ClientNetMgr::GetInstance()->Terminate();
                ::nl::net::client::ClientNetMgr::GetInstance()->WaitForTerminate();

                PlayerMgr::GetInstance()->Terminate();
                PlayerMgr::GetInstance()->WaitForTerminate();
        });

	return true;
}

void App::Stop()
{
	SuperType::Stop();
}

// vim: fenc=utf8:expandtab:ts=8
