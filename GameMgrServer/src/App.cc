#include "App.h"

#include "GameMgrLobbySession.h"
#include "GameMgrGameSession.h"
#include "Net/NetProcMgr.h"
#include "RegionMgr.h"

AppBase* GetAppBase()
{
	return App::GetInstance();
}

App* GetApp()
{
	return App::GetInstance();
}

App::App(const std::string& appName)
	: SuperType(appName, E_ST_GameMgr)
{
	GlobalSetup_CH::CreateInstance();
	RegionMgr::CreateInstance();
}

App::~App()
{
	RegionMgr::DestroyInstance();
	GlobalSetup_CH::DestroyInstance();
}

bool App::Init()
{
	LOG_FATAL_IF(!SuperType::Init(), "AppBase init error!!!");
	LOG_FATAL_IF(!GlobalSetup_CH::GetInstance()->Init(), "GlobalSetup_CH init error!!!");
	LOG_FATAL_IF(!RegionMgr::GetInstance()->Init(), "RegionMgr init error!!!");

	GetSteadyTimer().StartWithRelativeTimeForever(1.0, [](TimedEventItem& eventData) {
                static int64_t oldCnt = 0;
                (void)oldCnt;
                static int64_t oldReqQueueCnt = 0;

                LOG_INFO_IF(true, "reqQ[{}] cnt[{}] avg[{}]",
                            GetApp()->_reqQueueCnt - oldReqQueueCnt,
                            GetApp()->_cnt - oldCnt,
                            GetFrameController().GetAverageFrameCnt()
                           );

                oldCnt = GetApp()->_cnt;
                oldReqQueueCnt = GetApp()->_reqQueueCnt;
	});

	// {{{ start task
	std::vector<std::string> preTask;
        _startPriorityTaskList->AddFinalTaskCallback([]() {
        });

	/*
	 * 先连 game server，恢复好 region 关系之后，再向 lobby 同步消息。
	 * 若出现删除 region 而没通知到 lobby 的情况，在 lobby 端，game server
	 * 连接上来之后，若无 region 则 region destroy，若有，则发送 reconnect
	 * 到 game server。如果在 game server 上没找到对应 region，则 lobby 端
	 * 强制切换在主场景。
	 */

	preTask.clear();
	_startPriorityTaskList->AddTask(preTask, GameMgrGameSession::scPriorityTaskKey, [](const std::string& key) {
		auto gameMgrInfo = ServerListCfgMgr::GetInstance()->GetFirst<stGameMgrServerInfo>();
		auto proc = NetProcMgr::GetInstance()->Dist(1);
		proc->StartListener("0.0.0.0", gameMgrInfo->_game_port, "", "", []() {
			return CreateSession<GameMgrGameSession>();
		});
	});

	preTask.clear();
	// preTask.emplace_back(GameMgrGameSession::scPriorityTaskKey);
	_startPriorityTaskList->AddTask(preTask, GameMgrLobbySession::scPriorityTaskKey, [](const std::string& key) {
		auto gameMgrInfo = ServerListCfgMgr::GetInstance()->GetFirst<stGameMgrServerInfo>();
		auto proc = NetProcMgr::GetInstance()->Dist(0);
		proc->StartListener("0.0.0.0", gameMgrInfo->_lobby_port, "", "", []() {
			return CreateSession<GameMgrLobbySession>();
		});
	});
	// }}}

	// {{{
	_stopPriorityTaskList->AddFinalTaskCallback([]() {
                NetProcMgr::GetInstance()->Terminate();
                NetProcMgr::GetInstance()->WaitForTerminate();

		RegionMgr::GetInstance()->Terminate();
		RegionMgr::GetInstance()->WaitForTerminate();
	});
	// }}}

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
