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
  : SuperType(appName)
{
	nl::af::impl::ServerListCfgMgr::CreateInstance();
	TimerProcMgr::CreateInstance();
	NetProcMgr::CreateInstance();
	PlayerMgr::CreateInstance();
}

App::~App()
{
	TimerProcMgr::DestroyInstance();
	NetProcMgr::DestroyInstance();
	PlayerMgr::DestroyInstance();

	nl::af::impl::ServerListCfgMgr::DestroyInstance();
}

bool App::Init()
{
	LOG_FATAL_IF(!nl::af::impl::ServerListCfgMgr::GetInstance()->Init("./config_cx/server_list.json"), "服务器列表初始化失败!!!");

	int64_t sid = 10;
        std::ifstream f("sid.txt");
	std::string str;
	getline(f, str);
	if (!str.empty())
	{
	    sid = atoll(str.c_str());
	    // printf("sid[%ld] str[%s]", sid, str.c_str());
	}
	f.close();

	std::ofstream f1("sid.txt", std::ios_base::trunc);
	f1 << ((sid+1>=100) ? 10 : sid + 1);
        f1.close();

	// const auto totalThCnt = std::thread::hardware_concurrency() * 2 + 2;
	LOG_FATAL_IF(!SuperType::Init(GetApp()->GetRID(), sid, 1), "AppBase init error!!!");
	LOG_FATAL_IF(!NetProcMgr::GetInstance()->Init(2, "Net"), "NetProcMgr init error!!!");
	LOG_FATAL_IF(!TimerProcMgr::GetInstance()->Init(1, "Timer"), "TimerProcMgr init error!!!");
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
	TimerProcMgr::GetInstance()->Terminate();
	NetProcMgr::GetInstance()->Terminate();

	TimerProcMgr::GetInstance()->WaitForTerminate();
	NetProcMgr::GetInstance()->WaitForTerminate();

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
