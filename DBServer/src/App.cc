#include "App.h"

#include "DBLobbySession.h"
#include "DBMgr.h"

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
        DBMgr::CreateInstance();
}

App::~App()
{
        DBMgr::DestroyInstance();
}

bool App::Init()
{
	LOG_FATAL_IF(!SuperType::Init(E_ST_DB), "super init fail!!!");
        LOG_FATAL_IF(!DBMgr::GetInstance()->Init(), "dbmgr proc mgr init fail!!!");

        GetSteadyTimer().StartWithRelativeTimeForever(1.0, [](TimedEventItem& eventData) {
                static int64_t loadVersionCnt = 0;
                static int64_t loadVersionSize = 0;
                static int64_t loadCnt = 0;
                static int64_t loadSize = 0;
                static int64_t saveCnt = 0;
                static int64_t saveSize = 0;

                LOG_INFO_IF(true, "avg[{}] lvc[{}] lvs[{:5f}] lc[{}] ls[{:5f}] sc[{}] ss[{:5f}]",
                            GetFrameController().GetAverageFrameCnt(),
                            (GetApp()->_loadVersionCnt - loadVersionCnt) / 1000.0,
                            (GetApp()->_loadVersionSize - loadVersionSize) / (1024.0 * 1024.0),
                            (GetApp()->_loadCnt - loadCnt) / 1000.0,
                            (GetApp()->_loadSize - loadSize) / (1024.0 * 1024.0),
                            (GetApp()->_saveCnt - saveCnt) / 1000.0,
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
		auto proc = NetProcMgr::GetInstance()->Dist(1);
		proc->StartListener("0.0.0.0", GetApp()->GetServerInfo<stDBServerInfo>()->_lobby_port, "", "", []() { return CreateSession<DBLobbySession>(); });
	});

	_stopPriorityTaskList->AddFinalTaskCallback([]() {
                NetProcMgr::GetInstance()->ForeachProc([](const auto& proc) {
                        proc->PostTask([proc]() {
                                proc->stopAllListener();
                        });
                        proc->_sesList.Foreach([](auto ses) {
                                ses->AsyncClose(E_SCRT_ServiceTerminate);
                        });
                });

		DBMgr::GetInstance()->Terminate();
		DBMgr::GetInstance()->WaitForTerminate();
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
