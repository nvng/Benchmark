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
	PlayerMgr::CreateInstance();
}

App::~App()
{
	PlayerMgr::DestroyInstance();
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
	    // printf("sid[%ld] str[%s]", sid, str.c_str());
	}
	f.close();

	std::ofstream f1("sid.txt", std::ios_base::trunc);
	f1 << ((sid+1>=100) ? 10 : sid + 1);
        f1.close();

	LOG_FATAL_IF(!SuperType::Init(E_ST_None), "AppBase init error!!!");
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
