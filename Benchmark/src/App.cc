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

App::App()
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

bool App::Init(const std::string& appName)
{
	LOG_FATAL_IF(!nl::af::impl::ServerListCfgMgr::GetInstance()->Init("./config_cx/server_list.json"), "服务器列表初始化失败!!!");

	const auto totalThCnt = std::thread::hardware_concurrency() * 2 + 2;
	LOG_FATAL_IF(!SuperType::Init(appName, INT64_MAX, INT64_MAX, totalThCnt - 3), "AppBase init error!!!");
	LOG_FATAL_IF(!NetProcMgr::GetInstance()->Init(4, "Net"), "NetProcMgr init error!!!");
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

// vim: fenc=utf8:expandtab:ts=8
