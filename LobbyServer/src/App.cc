#include "App.h"

#include "Net/ISession.hpp"
#include "Player/PlayerMgr.h"
#include "Region/RegionMgr.h"

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
	: SuperType(appName, E_ST_Lobby)
	  , _gateSesList("App_gateSesList")
{
	GlobalSetup_CH::CreateInstance();
	RegionMgr::CreateInstance();
        RedisMgr::CreateInstance();

	PlayerMgr::CreateInstance();
        nl::net::NetMgr::CreateInstance();

        GenGuidService::CreateInstance();
}

App::~App()
{
	RegionMgr::DestroyInstance();
	MySqlService::DestroyInstance();

	PlayerMgr::DestroyInstance();

	GlobalSetup_CH::DestroyInstance();
        RedisMgr::DestroyInstance();
        nl::net::NetMgr::DestroyInstance();

        GenGuidService::DestroyInstance();
}

bool App::Init()
{
	LOG_FATAL_IF(!SuperType::Init(), "AppBase init error!!!");

	LOG_FATAL_IF(!GlobalSetup_CH::GetInstance()->Init(), "初始化策划全局配置失败!!!");
	LOG_FATAL_IF(!RedisMgr::GetInstance()->Init(ServerCfgMgr::GetInstance()->_redisCfg), "RedisMgr init error!!!");
	LOG_FATAL_IF(!RegionMgr::GetInstance()->Init(), "RegionMgr init error!!!");
	LOG_FATAL_IF(!PlayerMgr::GetInstance()->Init(), "PlayerMgr init error!!!");
	LOG_FATAL_IF(!MySqlService::GetInstance()->Init(), "MySqlService init error!!!");
	LOG_FATAL_IF(!GenGuidService::GetInstance()->Init(), "GenGuidService init error!!!");

	GetSteadyTimer().StartWithRelativeTimeForever(1.0, [](TimedEventItem& eventData) {
		static int64_t oldCnt = 0;
		(void)oldCnt;
		std::size_t agentCnt = 0;
		GetApp()->_gateSesList.Foreach([&agentCnt](const auto& weakSes) {
			auto ses = weakSes.lock();
			if (ses)
			{
				agentCnt += ses->GetAgentCnt();
			}
		});
#ifdef ____BENCHMARK____
		LOG_INFO_IF(true, "actorCnt[{}] agentCnt[{}] cnt[{}] flag[{}] avg[{}]",
			 PlayerMgr::GetInstance()->GetActorCnt(),
			 agentCnt,
			 GetApp()->_cnt - oldCnt,
			 // TimedEventLoop::_timedEventItemCnt,
			 PlayerBase::_playerFlag,
			 GetFrameController().GetAverageFrameCnt()
			 );
#else
		LOG_INFO_IF(false, "actorCnt[{}] agentCnt[{}] cnt[{}] avg[{}]",
			 PlayerMgr::GetInstance()->GetActorCnt(),
			 agentCnt,
			 GetApp()->_cnt - oldCnt,
			 GetFrameController().GetAverageFrameCnt()
			 );
#endif
		oldCnt = GetApp()->_cnt;
	});



	// {{{ start task
        _startPriorityTaskList->AddFinalTaskCallback([this]() {
		// Note: 多线程
		// TODO: 监听端口

                ServerListCfgMgr::GetInstance()->Foreach<stDBServerInfo>([](const auto& cfg) {
                        MySqlService::GetInstance()->Start(cfg->_ip, cfg->_lobby_port);
                });

		auto lobbyInfo = GetServerInfo<stLobbyServerInfo>();
                NetMgr::GetInstance()->Listen(lobbyInfo->_gate_port, [](auto&& s, auto& sslCtx) {
                        return std::make_shared<LobbyGateSession>(std::move(s));
                });

                /*
                std::set<std::string> keyList =
                {
                        "ctype", "player_guid", "newpwd", "safejinbi", "newplayerstate",
                        "blackliststate", "is_freeze", "jin_bi",
                };
                NetMgr::GetInstance()->StartHttpServer(GlobalSetup_CH::GetInstance()->mHttpServerForGMAddr.ip_,
                        GlobalSetup_CH::GetInstance()->mHttpServerForGMAddr.port_,
                        false,
                        keyList,
                        [](struct evhttp_request*, char* buff, ev_ssize_t, std::map<std::string, std::string>& paramList)
                {
                        NetMgr::GetInstance()->OnGM(paramList);
                        return true;
                });
                */

                auto dbInfo = ServerListCfgMgr::GetInstance()->GetFirst<stDBServerInfo>();
                GenGuidService::GetInstance()->Start(dbInfo->_ip, dbInfo->_gen_guid_service_port);

		LOG_WARN("server start success!!!");

		/*
		   _serviceDiscoveryActor = std::make_shared<ServiceDistcoveryActor>();
		   _serviceDiscoveryActor->Start();
		   */

		GetApp()->_globalVarActor = std::make_shared<GlobalVarActor>();
		GetApp()->_globalVarActor->Start();

		GetApp()->PostTask([]() {
			auto calNextDayChangeTimeFunc = []()
			{
				time_t zero = Clock::TimeClear_Slow(GetClock().GetTimeStamp(), Clock::E_CTT_DAY);
				zero = GetApp()->_globalVarActor->GetOrCreateIfNotExist("appLastDayChangeTimeZero", zero);
				return static_cast<double>(Clock::TimeAdd_Slow(zero, DAY_TO_SEC(1)));
			};
                        // LOG_WARN("app next day change time:{}", Clock::GetTimeString_Slow(calNextDayChangeTimeFunc()));
			GetTimer().StartWithAbsoluteTimeForever(calNextDayChangeTimeFunc(), 0.0, [calNextDayChangeTimeFunc](TimedEventItem& eventData) {
				GetApp()->OnDayChange();

				time_t zero = Clock::TimeClear_Slow(GetClock().GetTimeStamp(), Clock::E_CTT_DAY);
				GetApp()->_globalVarActor->AddVar("appLastDayChangeTimeZero", zero);
				TimedEventLoop::SetOverTime(eventData, calNextDayChangeTimeFunc());
			});

			auto calNextDataResetTimeFunc = []()
			{
				time_t zero = Clock::TimeClear_Slow(GetClock().GetTimeStamp(), Clock::E_CTT_DAY);
				zero = GetApp()->_globalVarActor->GetOrCreateIfNotExist("appLastDataResetTimeNoneZero", zero);
				time_t cfgSec = HOUR_TO_SEC(GlobalSetup_CH::GetInstance()->_dataResetNonZero);

				time_t now = GetClock().GetTimeStamp();
				return static_cast<double>(zero + (now < zero + cfgSec ? 0 : DAY_TO_SEC(1)) + cfgSec);
			};

                        // LOG_WARN("app next data reset time:{}", Clock::GetTimeString_Slow(calNextDataResetTimeFunc()));
			GetTimer().StartWithAbsoluteTimeForever(calNextDataResetTimeFunc(), 0.0, [calNextDataResetTimeFunc](TimedEventItem& eventData) {
				GetApp()->OnDataReset();

				time_t zero = Clock::TimeClear_Slow(GetClock().GetTimeStamp(), Clock::E_CTT_DAY);
				GetApp()->_globalVarActor->AddVar("appLastDataResetTimeNoneZero", zero);

				TimedEventLoop::SetOverTime(eventData, calNextDataResetTimeFunc());
			});
		});
        });

	std::vector<std::string> preTask;

	preTask.clear();
	_startPriorityTaskList->AddTask(preTask, LobbyGameMgrSession::_sPriorityTaskKey, [](const std::string& key) {
		auto gameMgrInfo = ServerListCfgMgr::GetInstance()->GetFirst<stGameMgrServerInfo>();
                LOG_INFO("777777777 ip[{}] port[{}]", gameMgrInfo->_ip, gameMgrInfo->_lobby_port);
                NetMgr::GetInstance()->Connect(gameMgrInfo->_ip, gameMgrInfo->_lobby_port, [](auto&& s) {
                        return std::make_shared<LobbyGameMgrSession>(std::move(s));
                });
	});

        /*
	preTask.clear();
        preTask.emplace_back(LobbyGameMgrSession::_sPriorityTaskKey);
        _startPriorityTaskList->AddTask(preTask, LobbyDBSession::scPriorityTaskKey, [](const std::string& key) {
                ServerListCfgMgr::GetInstance()->Foreach<stDBServerInfo>([](const auto& cfg) {
                        nl::net::NetMgr::GetInstance()->Connect(cfg->_ip, cfg->_lobby_port, [](auto&& s) {
                                return std::make_shared<LobbyDBSession>(std::move(s));
                        });
                });
        });
        */

	preTask.clear();
	preTask.emplace_back(LobbyGameMgrSession::_sPriorityTaskKey);
	_startPriorityTaskList->AddTask(preTask, LobbyGameSession::scPriorityTaskKey, [](const std::string& key) {
		auto lobbyInfo = GetApp()->GetServerInfo<stLobbyServerInfo>();
                NetMgr::GetInstance()->Listen(lobbyInfo->_game_port, [](auto&& s, auto& sslCtx) {
			return std::make_shared<LobbyGameSession>(std::move(s));
                });
	});

	// }}}

	// {{{ stop task
        _stopPriorityTaskList->AddFinalTaskCallback([this]() {
		if (_globalVarActor)
		{
			_globalVarActor->Terminate();
			_globalVarActor->WaitForTerminate();
		}

		PlayerMgr::GetInstance()->Terminate();
		PlayerMgr::GetInstance()->WaitForTerminate();

		RegionMgr::GetInstance()->Terminate();
		RegionMgr::GetInstance()->WaitForTerminate();
        });
	// }}}

	return true;
}

void App::OnDayChange()
{
        LOG_WARN("app day change now:{}", Clock::GetTimeString_Slow(GetClock().GetTimeStamp()));
	PlayerMgr::GetInstance()->OnDayChange();
}

void App::OnDataReset()
{
        LOG_WARN("app data reset now:{}", Clock::GetTimeString_Slow(GetClock().GetTimeStamp()));
	PlayerMgr::GetInstance()->OnDataReset(GlobalSetup_CH::GetInstance()->_dataResetNonZero);
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
