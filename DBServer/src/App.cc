#include "App.h"

#include "DBLobbySession.h"
#include "DBMgr.h"

#include "Tools/LogHelper.h"
#include "Tools/ServerList.hpp"

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
	DBMgr::CreateInstance();
}

App::~App()
{
	DBMgr::DestroyInstance();
}

bool App::Init()
{
	LOG_FATAL_IF(!SuperType::Init(), "super init fail!!!");
	LOG_FATAL_IF(!DBMgr::GetInstance()->Init(), "dbmgr proc mgr init fail!!!");

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

                LOG_INFO_IF(true, "avg[{}] actorCnt[{}] lvc[{}] lvs[{:.6f}] lc[{}] ls[{:.6f}] sc[{}] ss[{:.6f}]",
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
                nl::net::NetMgr::GetInstance()->Listen(GetAppBase()->GetServerInfo<stDBServerInfo>()->_lobby_port, [](auto&& s, auto& ioCtx) {
                        return std::make_shared<DBLobbySession>(std::move(s));
                });
	});

        _stopPriorityTaskList->AddFinalTaskCallback([]() {
                DBMgr::GetInstance()->Terminate();
                DBMgr::GetInstance()->WaitForTerminate();
        });

	return true;
}

#include "msg_client_type.pb.h"
#include "msg_client.pb.h"
NET_MSG_HANDLE(DBLobbySession, E_MCMT_ClientCommon, E_MCCCST_Login, MsgClientLogin)
{
        SendPB(msg, E_MCMT_ClientCommon, E_MCCCST_Login, MsgHeaderType::ERemoteMailType::E_RMT_Send, 0, 0, 0);
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
