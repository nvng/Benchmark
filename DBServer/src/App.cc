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

	_startPriorityTaskList->AddFinalTaskCallback([]() {
		auto proc = NetProcMgr::GetInstance()->Dist(1);
		proc->StartListener("0.0.0.0", GetApp()->GetServerInfo<stDBServerInfo>()->_lobby_port, "", "", []() { return CreateSession<DBLobbySession>(); });
	});

	_stopPriorityTaskList->AddFinalTaskCallback([]() {
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
