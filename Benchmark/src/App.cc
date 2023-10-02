#include "App.h"

#include "Player/PlayerMgr.h"
#include "Net/ClientGateSession.h"

AppBase* GetAppBase()
{
	return App::GetInstance();
}

App* GetApp()
{
	return App::GetInstance();
}

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

        _serverInfo = std::make_shared<stServerInfoBase>();
        _serverInfo->_sid = sid;
        _serverInfo->_workersCnt = 2;
        _serverInfo->_netProcCnt = 2;

	LOG_FATAL_IF(!SuperType::Init(), "AppBase init error!!!");
	LOG_FATAL_IF(!::nl::net::client::ClientNetMgr::GetInstance()->Init(_serverInfo->_netProcCnt, "client"), "ClientNetMgr init error!!!");
	LOG_FATAL_IF(!PlayerMgr::GetInstance()->Init(), "PlayerMgr init error!!!");

#if 1
	GetSteadyTimer().StartWithRelativeTimeForever(1.0, [](TimedEventItem& eventData) {
		static uint64_t cnt = 0;
                (void)cnt;
		// LOG_INFO("avgCost[{}] cnt[{}]", ClientGateSession::GetAvgCostTime(), GetApp()->_cnt - cnt);
		LOG_INFO("cnt[{}]", GetApp()->_cnt - cnt);
		cnt = GetApp()->_cnt;
	});
#endif

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
