#include "App.h"

AppBase* GetAppBase()
{
	return App::GetInstance();
}

App* GetApp()
{
	return App::GetInstance();
}

App::App(const std::string& appName)
	: SuperType(appName, E_ST_DB)
{
        GenGuidService::CreateInstance();
	MySqlService::CreateInstance();
}

App::~App()
{
        GenGuidService::DestroyInstance();
	MySqlService::DestroyInstance();
}

bool App::Init()
{
	LOG_FATAL_IF(!SuperType::Init(), "super init fail!!!");
	// LOG_FATAL_IF(!DBMgr::GetInstance()->Init(), "dbmgr proc mgr init fail!!!");
	LOG_FATAL_IF(!GenGuidService::GetInstance()->Init(), "GenGuidService init fail!!!");

        GetSteadyTimer().StartWithRelativeTimeForever(1.0, [](TimedEventItem& eventData) {
                static int64_t loadVersionCnt = 0;
                static int64_t loadVersionSize = 0;
                static int64_t loadCnt = 0;
                static int64_t loadSize = 0;
                static int64_t saveCnt = 0;
                static int64_t saveSize = 0;

                (void)loadVersionCnt;
                (void)loadVersionSize;
                (void)loadCnt;
                (void)loadSize;
                (void)saveCnt;
                (void)saveSize;

                LOG_INFO_IF(0 != GetApp()->_loadVersionCnt - loadVersionCnt
                            || 0 != GetApp()->_loadCnt - loadCnt
                            || 0 != GetApp()->_saveCnt - saveCnt,
                            "avg[{}] actorCnt[{}] lvc[{}] lvs[{}] lc[{}] ls[{}] sc[{}] ss[{}]",
                            GetFrameController().GetAverageFrameCnt(),
                            SpecialActorMgr::GetInstance()->GetActorCnt(),
                            GetApp()->_loadVersionCnt - loadVersionCnt,
                            (GetApp()->_loadVersionSize - loadVersionSize) / (1024.0 * 1024.0),
                            GetApp()->_loadCnt - loadCnt,
                            (GetApp()->_loadSize - loadSize) / (1024.0 * 1024.0),
                            GetApp()->_saveCnt - saveCnt,
                            (GetApp()->_saveSize - saveSize) / (1024.0 * 1024.0)
                           );

                loadVersionCnt = GetApp()->_loadVersionCnt;
                loadVersionSize = GetApp()->_loadVersionSize;
                loadCnt = GetApp()->_loadCnt;
                loadSize = GetApp()->_loadSize;
                saveCnt = GetApp()->_saveCnt;
                saveSize = GetApp()->_saveSize;
        });
        
	_startPriorityTaskList->AddFinalTaskCallback([]() {
                auto dbInfo = GetAppBase()->GetServerInfo<stDBServerInfo>();
                MySqlService::GetInstance()->Start(dbInfo->_lobby_port);
                GenGuidService::GetInstance()->Start(dbInfo->_gen_guid_service_port);
	});

        _stopPriorityTaskList->AddFinalTaskCallback([]() {
                GenGuidService::GetInstance()->Terminate();
                GenGuidService::GetInstance()->WaitForTerminate();

                LOG_INFO("111111111111111111");
                MySqlService::GetInstance()->Terminate();
                LOG_INFO("111111111111111111");
                MySqlService::GetInstance()->WaitForTerminate();
                LOG_INFO("111111111111111111");
        });

        ::nl::util::SteadyTimer::StaticStart(10.0, []() {
                GetApp()->PostTask([]() { GetApp()->Stop(); });
        });

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
