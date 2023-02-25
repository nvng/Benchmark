#include "App.h"

#include "Player/PlayerMgr.h"
#include "Net/LobbyGateSession.h"
#include "Net/LobbyDBSession.h"
#include "Region/RegionMgr.h"
#include "DB/DBMgr.h"

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
	nl::af::impl::ServerCfgMgr::CreateInstance();

	GlobalSetup_CH::CreateInstance();
	TimerProcMgr::CreateInstance();
	NetProcMgr::CreateInstance();
	RedisProcMgr::CreateInstance();

	RegionMgr::CreateInstance();

	DBMgr::CreateInstance();
	PlayerMgr::CreateInstance();

	SpecialActorMgr::CreateInstance();
}

App::~App()
{
	GetTimer().Stop(_dayChangeTimerGuid);
	_dayChangeTimerGuid = INVALID_TIMER_GUID;

	GetTimer().Stop(_dataResetTimerGuid);
	_dataResetTimerGuid = INVALID_TIMER_GUID;

	TimerProcMgr::DestroyInstance();
	NetProcMgr::DestroyInstance();
	RedisProcMgr::DestroyInstance();

	RegionMgr::DestroyInstance();

	PlayerMgr::DestroyInstance();
	DBMgr::DestroyInstance();

	SpecialActorMgr::DestroyInstance();
	GlobalSetup_CH::DestroyInstance();
	nl::af::impl::ServerListCfgMgr::DestroyInstance();
	nl::af::impl::ServerCfgMgr::DestroyInstance();
}

bool App::Init(const std::string& appName, int32_t coCnt /*= 100*/, int32_t thCnt /*= 1*/)
{
        LOG_FATAL_IF(!nl::af::impl::ServerListCfgMgr::GetInstance()->Init("./config_cx/server_list.json"), "服务器列表初始化失败!!!");
        LOG_FATAL_IF(!nl::af::impl::ServerCfgMgr::GetInstance()->Init("./config_cx/server_cfg.json"), "服务器配置初始化失败!!!");

	nl::af::impl::ServerListCfgMgr::GetInstance()->_lobbyServerList.Foreach([](const nl::af::impl::stLobbyServerInfoPtr& sInfo) {
		if (GetApp()->_lobbyInfo)
			return;
                auto ip = GetIP(AF_INET, sInfo->_faName);
		if (ip == sInfo->_ip
		    && RunShellCmd(fmt::format("netstat -apn | grep 0.0.0.0:{}", sInfo->_game_port)).empty()
		    && RunShellCmd(fmt::format("netstat -apn | grep 0.0.0.0:{}", sInfo->_gate_port)).empty())
			GetApp()->_lobbyInfo = sInfo;
        });
	LOG_FATAL_IF(!_lobbyInfo, "server info not found!!!");

	LOG_FATAL_IF(!SuperType::Init(appName, nl::af::impl::ServerListCfgMgr::GetInstance()->_rid, _lobbyInfo->_sid, _lobbyInfo->_workers_cnt), "AppBase init error!!!");
	LOG_FATAL_IF(!NetProcMgr::GetInstance()->Init(_lobbyInfo->_net_proc_cnt, "Net"), "NetProcMgr init error!!!");
	LOG_FATAL_IF(!TimerProcMgr::GetInstance()->Init(_lobbyInfo->_timer_proc_cnt, "Timer"), "TimerProcMgr init error!!!");
	LOG_FATAL_IF(!RedisProcMgr::GetInstance()->Init(_lobbyInfo->_redis_conn_cnt, "Redis", nl::af::impl::ServerCfgMgr::GetInstance()->_redisCfg), "RedisProcMgr init error!!!");

	LOG_FATAL_IF(!GlobalSetup_CH::GetInstance()->Init(), "初始化策划全局配置失败!!!");
	LOG_FATAL_IF(!DBMgr::GetInstance()->Init(), "DBMgr init error!!!");
	LOG_FATAL_IF(!RegionMgr::GetInstance()->Init(), "RegionMgr init error!!!");
	LOG_FATAL_IF(!PlayerMgr::GetInstance()->Init(), "PlayerMgr init error!!!");

#if 0
	int64_t idx = 0;
	ServerListCfgMgr::GetInstance()->_dbServerList.Foreach([&idx](const auto& sInfo) {
		auto proc = NetProcMgr::GetInstance()->Dist(++idx);
		proc->Connect(sInfo->_ip, sInfo->_lobby_port, false, []() { return CreateSession<LobbyDBSession>(); });
	});
#endif

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
			 nl::af::impl::PlayerBase::_playerFlag,
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

	InitPreTask();
	return true;
}

void App::InitPreTask()
{
	GetSteadyTimer().StartWithRelativeTimeForever(10.0, 1.0,[](TimedEventItem& eventData) {
		std::vector<std::string> taskKeyList = GetApp()->_startPriorityTaskList.GetRunButNotFinishedTask();
		if (taskKeyList.empty())
		{
			GetSteadyTimer().Stop(eventData);
		}
		else
		{
			std::string printStr;
			for (auto& taskKey : taskKeyList)
			{
				printStr += taskKey;
				printStr += " ";
			}
			LOG_WARN("init task not finish : {}", printStr);
		}
	});

	std::vector<std::string> preTask;
        _startPriorityTaskList.SetFinalTaskCallback([]() {
		// Note: 多线程
		// TODO: 监听端口

		auto proc = NetProcMgr::GetInstance()->Dist(0);
		LOG_FATAL_IF(nullptr == proc, "proc dist fail!!!");

		proc->StartListener("0.0.0.0", GetApp()->_lobbyInfo->_gate_port, "", "", []() {
			return CreateSession<LobbyGateSession>();
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

		LOG_WARN("server start success!!!");

		/*
		   _serviceDiscoveryActor = std::make_shared<ServiceDistcoveryActor>();
		   _serviceDiscoveryActor->Start();
		   */

		GetApp()->_globalVarActor = std::make_shared<nl::af::impl::GlobalVarActor>();
		GetApp()->_globalVarActor->Start();

		GetApp()->PostTask([]() {
			auto calNextDayChangeTimeFunc = []()
			{
				time_t zero = Clock::TimeClear_Slow(GetClock().GetTimeStamp(), Clock::E_CTT_DAY);
				zero = GetApp()->_globalVarActor->GetOrCreateIfNotExist("appLastDayChangeTimeZero", zero);
				return static_cast<double>(Clock::TimeAdd_Slow(zero, DAY_TO_SEC(1)));
			};
                        // LOG_WARN("app next day change time:{}", Clock::GetTimeString_Slow(calNextDayChangeTimeFunc()));
			GetApp()->_dayChangeTimerGuid = GetTimer().StartWithAbsoluteTimeForever(calNextDayChangeTimeFunc(), 0.0, [calNextDayChangeTimeFunc](TimedEventItem& eventData) {
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
			GetApp()->_dataResetTimerGuid = GetTimer().StartWithAbsoluteTimeForever(calNextDataResetTimeFunc(), 0.0, [calNextDataResetTimeFunc](TimedEventItem& eventData) {
				GetApp()->OnDataReset();

				time_t zero = Clock::TimeClear_Slow(GetClock().GetTimeStamp(), Clock::E_CTT_DAY);
				GetApp()->_globalVarActor->AddVar("appLastDataResetTimeNoneZero", zero);

				TimedEventLoop::SetOverTime(eventData, calNextDataResetTimeFunc());
			});
		});
        });

	/*
	preTask.clear();
	preTask.emplace_back("load_sid");
	_startPriorityTask.AddTask(preTask, "load_sid", [](const std::string& key) {
	});
	*/

	// 这个必须放在最后
        _startPriorityTaskList.CheckAndExecute();
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

void App::Stop()
{
        StopPreTask();
}

void App::StopPreTask()
{
	GetSteadyTimer().StartWithRelativeTimeForever(10.0, 1.0,[](TimedEventItem& eventData) {
		std::vector<std::string> taskKeyList = GetApp()->_stopPriorityTaskList.GetRunButNotFinishedTask();
		if (taskKeyList.empty())
		{
			GetSteadyTimer().Stop(eventData);
		}
		else
		{
			std::string printStr;
			for (auto& taskKey : taskKeyList)
			{
				printStr += taskKey;
				printStr += " ";
			}
			LOG_WARN("stop task not finish : {}", printStr);
		}
	});

	std::vector<std::string> preTask;
        _stopPriorityTaskList.SetFinalTaskCallback([this]() {
		if (_globalVarActor)
		{
			_globalVarActor->Terminate();
			_globalVarActor->WaitForTerminate();
		}

		PlayerMgr::GetInstance()->Terminate();
		PlayerMgr::GetInstance()->WaitForTerminate();

                TimerProcMgr::GetInstance()->Terminate();
                NetProcMgr::GetInstance()->Terminate();
                RedisProcMgr::GetInstance()->Terminate();

                TimerProcMgr::GetInstance()->WaitForTerminate();
                NetProcMgr::GetInstance()->WaitForTerminate();
                RedisProcMgr::GetInstance()->WaitForTerminate();

		SpecialActorMgr::GetInstance()->Terminate();
		SpecialActorMgr::GetInstance()->WaitForTerminate();

		RegionMgr::GetInstance()->Terminate();
		RegionMgr::GetInstance()->WaitForTerminate();

                GetApp()->SuperType::Stop();
                LOG_WARN("server stop success!!!");
        });

	// 这个必须放在最后
        _stopPriorityTaskList.CheckAndExecute();
}

#include "Player/Player.h"
ACTOR_MAIL_HANDLE(Player, 0x7f, 0)
{
	++GetApp()->_cnt;
	Send2Client(0x7f, 0, nullptr);
	return nullptr;
}

ACTOR_MAIL_HANDLE(Player, 0x7f, 1)
{
	++GetApp()->_cnt;
	Send2Client(0x7f, 1, nullptr);
	return nullptr;
}

// vim: fenc=utf8:expandtab:ts=8
