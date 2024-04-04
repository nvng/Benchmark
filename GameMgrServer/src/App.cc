#include "App.h"

#include "ActorFramework/ServiceExtra.hpp"
#include "ActorFramework/SpecialActor.hpp"
#include "GameMgrLobbySession.h"
#include "GameMgrGameSession.h"
#include "Net/SessionImpl.hpp"
#include "RegionMgr.h"
#include "RequestActor.h"
#include "Tools/Util.h"
#include "PingPong.h"

SPECIAL_ACTOR_DEFINE_BEGIN(Test1ServiceActor);
public :
        Test1ServiceActor() : SuperType(SpecialActorMgr::GetInstance()->GenActorID(), IActor::scMailQueueMaxSize) { }
SPECIAL_ACTOR_DEFINE_END(Test1ServiceActor);

DECLARE_SERVICE_BEGIN(Test1Service, SessionDistributeMod, ServiceSession, ServiceExtraWapper);
DECLARE_SERVICE_END(Test1Service);

typedef Test1ServiceBase<nl::af::E_ServiceType_Server, stServerInfoBase> Test1ServiceServerType;

SERVICE_NET_HANDLE(Test1ServiceServerType::SessionType, 0xfff, 0x0)
{
        LOG_INFO("aaaaaaaaaaaaaaaaaaa mt[{:#x}], st[{:#x}]", 0xfff, 0x0);
        Send(0xf3f, 0x0, MsgHeaderType::E_RMT_None, 0, 0, 0);
        return nullptr;
}

SPECIAL_ACTOR_DEFINE_BEGIN(Test2ServiceActor);
public :
        Test2ServiceActor() : SuperType(SpecialActorMgr::GetInstance()->GenActorID(), IActor::scMailQueueMaxSize) { }
SPECIAL_ACTOR_DEFINE_END(Test2ServiceActor);

DECLARE_SERVICE_BEGIN(Test2Service, SessionDistributeMod, ServiceSession, ServiceExtraWapper);
DECLARE_SERVICE_END(Test2Service);

typedef Test2ServiceBase<nl::af::E_ServiceType_Server, stServerInfoBase> Test2ServiceServerType;

MAIN_FUNC();

App::App(const std::string& appName)
	: SuperType(appName, E_ST_GameMgr)
{
	GlobalSetup_CH::CreateInstance();
	RegionMgr::CreateInstance();

        RobotService::CreateInstance();

        Test1ServiceServerType::CreateInstance();
        Test2ServiceServerType::CreateInstance();

        PingPongService::CreateInstance();
}

App::~App()
{
	RegionMgr::DestroyInstance();
	GlobalSetup_CH::DestroyInstance();
        RobotService::DestroyInstance();

        Test1ServiceServerType::DestroyInstance();
        Test2ServiceServerType::DestroyInstance();

        PingPongService::DestroyInstance();
}

bool App::Init()
{
	LOG_FATAL_IF(!SuperType::Init(), "AppBase init error!!!");
	LOG_FATAL_IF(!GlobalSetup_CH::GetInstance()->Init(), "GlobalSetup_CH init error!!!");
	LOG_FATAL_IF(!RegionMgr::GetInstance()->Init(), "RegionMgr init error!!!");
	LOG_FATAL_IF(!RobotService::GetInstance()->Init(), "RobotService init error!!!");

        LOG_FATAL_IF(!PingPongService::GetInstance()->Init(), "");

        /*
	LOG_FATAL_IF(!Test1ServiceServerType::GetInstance()->Init(), "Test1Service init error!!!");
        Test1ServiceServerType::GetInstance()->Start(9999);

	LOG_FATAL_IF(!Test2ServiceServerType::GetInstance()->Init(), "Test2Service init error!!!");
        Test2ServiceServerType::GetInstance()->Start(9999);
        */

        ::nl::util::SteadyTimer::StartForever(1.0, [](double&) {
                [[maybe_unused]] static uint64_t oldCnt = 0;
                [[maybe_unused]] static uint64_t oldReqQueueCnt = 0;

                LOG_INFO_IF(true, "reqQ[{}] cnt[{}] ses[{}]"
                            , GetApp()->_reqQueueCnt - oldReqQueueCnt
                            , GetApp()->_cnt - oldCnt
                            , NetMgr::GetInstance()->_sesList.Size()
                           );

                oldCnt = GetApp()->_cnt;
                oldReqQueueCnt = GetApp()->_reqQueueCnt;

                return true;
	});

	// {{{ start task
        _startPriorityTaskList->AddFinalTaskCallback([]() {
        });

	/*
	 * 先连 game server，恢复好 region 关系之后，再向 lobby 同步消息。
	 * 若出现删除 region 而没通知到 lobby 的情况，在 lobby 端，game server
	 * 连接上来之后，若无 region 则 region destroy，若有，则发送 reconnect
	 * 到 game server。如果在 game server 上没找到对应 region，则 lobby 端
	 * 强制切换在主场景。
	 */

	_startPriorityTaskList->AddTask(GameMgrGameSession::scPriorityTaskKey, [](const std::string& key) {
		auto gameMgrInfo = ServerListCfgMgr::GetInstance()->GetFirst<stGameMgrServerInfo>();
                NetMgr::GetInstance()->Listen(gameMgrInfo->_game_port, [](auto&& s, auto& ioCtx) {
                        return std::make_shared<GameMgrGameSession>(std::move(s));
                });
	}, {}, ServerListCfgMgr::GetInstance()->GetSize<stGameServerInfo>());

	_startPriorityTaskList->AddTask(GameMgrLobbySession::scPriorityTaskKey, [](const std::string& key) {
		auto gameMgrInfo = ServerListCfgMgr::GetInstance()->GetFirst<stGameMgrServerInfo>();
                NetMgr::GetInstance()->Listen(gameMgrInfo->_lobby_port, [](auto&& s, auto& ioCtx) {
                        return std::make_shared<GameMgrLobbySession>(std::move(s));
                });
	}
        , { GameMgrGameSession::scPriorityTaskKey }
        , ServerListCfgMgr::GetInstance()->GetSize<stLobbyServerInfo>());
	// }}}

	// {{{
	_stopPriorityTaskList->AddFinalTaskCallback([]() {
		RegionMgr::GetInstance()->Terminate();
		RegionMgr::GetInstance()->WaitForTerminate();
	});
	// }}}

	return true;
}

// vim: fenc=utf8:expandtab:ts=8
