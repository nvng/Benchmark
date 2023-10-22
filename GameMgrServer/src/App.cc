#include "App.h"

#include "GameMgrLobbySession.h"
#include "GameMgrGameSession.h"
#include "Net/SessionImpl.hpp"
#include "RegionMgr.h"
#include "RequestActor.h"
#include "PingPongBig.h"

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

        RobotService::CreateInstance();
        PingPongBigService::CreateInstance();
}

App::~App()
{
	RegionMgr::DestroyInstance();
	GlobalSetup_CH::DestroyInstance();
        RobotService::DestroyInstance();
        PingPongBigService::DestroyInstance();
}

bool App::Init()
{
	LOG_FATAL_IF(!SuperType::Init(), "AppBase init error!!!");
	LOG_FATAL_IF(!GlobalSetup_CH::GetInstance()->Init(), "GlobalSetup_CH init error!!!");
	LOG_FATAL_IF(!RegionMgr::GetInstance()->Init(), "RegionMgr init error!!!");
	LOG_FATAL_IF(!RobotService::GetInstance()->Init(), "RobotService init error!!!");
	LOG_FATAL_IF(!PingPongBigService::GetInstance()->Init(), "PingPongBigService init error!!!");

	GetSteadyTimer().StartWithRelativeTimeForever(1.0, [](TimedEventItem& eventData) {
                static int64_t oldCnt = 0;
                (void)oldCnt;
                static int64_t oldReqQueueCnt = 0;

                LOG_INFO_IF(true, "reqQ[{}] cnt[{}] ses[{}] avg[{}]",
                            GetApp()->_reqQueueCnt - oldReqQueueCnt,
                            GetApp()->_cnt - oldCnt,
                            NetMgr::GetInstance()->_sesList.Size(),
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
                NetMgr::GetInstance()->Listen(gameMgrInfo->_game_port, [](auto&& s, auto& ioCtx) {
                        return std::make_shared<GameMgrGameSession>(std::move(s));
                });
	});

	preTask.clear();
	preTask.emplace_back(GameMgrGameSession::scPriorityTaskKey);
	_startPriorityTaskList->AddTask(preTask, GameMgrLobbySession::scPriorityTaskKey, [](const std::string& key) {
		auto gameMgrInfo = ServerListCfgMgr::GetInstance()->GetFirst<stGameMgrServerInfo>();
                NetMgr::GetInstance()->Listen(gameMgrInfo->_lobby_port, [](auto&& s, auto& ioCtx) {
                        return std::make_shared<GameMgrLobbySession>(std::move(s));
                });
	});
	// }}}

	// {{{
	_stopPriorityTaskList->AddFinalTaskCallback([]() {
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

ACTOR_MAIL_HANDLE(RequestActor, 0xff, 0xf)
{
        auto cfg = std::make_shared<MailRegionCreateInfo>();
        auto ses = GetRegionMgrBase()->_gameSesArr[0].lock();
        auto agent = std::make_shared<GameMgrGameSession::ActorAgentType>(cfg, ses, shared_from_this());
        agent->BindActor(shared_from_this());
        ses->AddAgent(agent);

        /*
        auto gameAgent = GetRegionMgrBase()->CreateRegionAgent(cfg, ses, shared_from_this());
        gameAgent->BindActor(shared_from_this());
        ses->AddAgent(gameAgent);
        */
        auto regionMgr = GetRegionMgrBase()->GetRegionMgrActor(E_RT_PVE);
        while (agent)
        {
                ++GetApp()->_cnt;
                Call(MailResult, regionMgr, scRegionMgrActorMailMainType, 0xe, nullptr);
                Call(MailResult, agent, 0xff, 0xf, nullptr);
                // boost::this_fiber::sleep_for(std::chrono::milliseconds(100));
                // LOG_INFO("1111111111");
        }

        return nullptr;
}

// vim: fenc=utf8:expandtab:ts=8
