#include "App.h"

#include "Player/PlayerMgr.h"
#include "Region/RegionMgr.h"

#include "MailSys.h"
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
	DBMgr::CreateInstance();
	RegionMgr::CreateInstance();

	PlayerMgr::CreateInstance();
}

App::~App()
{
	RegionMgr::DestroyInstance();
	DBMgr::DestroyInstance();

	PlayerMgr::DestroyInstance();

	GlobalSetup_CH::DestroyInstance();
}

#include <sys/prctl.h>
#include <malloc.h>
bool App::Init()
{
	LOG_FATAL_IF(!SuperType::Init(), "AppBase init error!!!");

	LOG_FATAL_IF(!GlobalSetup_CH::GetInstance()->Init(), "初始化策划全局配置失败!!!");
	LOG_FATAL_IF(!RegionMgr::GetInstance()->Init(), "RegionMgr init error!!!");
	LOG_FATAL_IF(!PlayerMgr::GetInstance()->Init(), "PlayerMgr init error!!!");
	LOG_FATAL_IF(!DBMgr::GetInstance()->Init(), "DBMgr init error!!!");

#if 1
	int64_t idx = 0;
	ServerListCfgMgr::GetInstance()->Foreach<stDBServerInfo>([&idx](const auto& sInfo) {
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
		malloc_trim(0);
	});



	// {{{ start task
	std::vector<std::string> preTask;
        _startPriorityTaskList->AddFinalTaskCallback([this]() {
		// Note: 多线程
		// TODO: 监听端口

		auto proc = NetProcMgr::GetInstance()->Dist(0);
		LOG_FATAL_IF(nullptr == proc, "proc dist fail!!!");

		auto lobbyInfo = GetServerInfo<stLobbyServerInfo>();
		proc->StartListener("0.0.0.0", lobbyInfo->_gate_port, "", "", []() {
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

	preTask.clear();
	_startPriorityTaskList->AddTask(preTask, LobbyGameMgrSession::_sPriorityTaskKey, [](const std::string& key) {
		auto gameMgrInfo = ServerListCfgMgr::GetInstance()->GetFirst<stGameMgrServerInfo>();
		auto proc = NetProcMgr::GetInstance()->Dist(1);
		proc->Connect(gameMgrInfo->_ip, gameMgrInfo->_lobby_port, false, []() { return CreateSession<LobbyGameMgrSession>(); });
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

template <> constexpr uint64_t GetLogModuleParallelCnt<E_LOG_MT_Player>() { return 8; }
#include "Player/Player.h"
ACTOR_MAIL_HANDLE(Player, 0x7f, 0, MsgClientLogin)
// ACTOR_MAIL_HANDLE(Player, 0x7f, 0)
{
  // for (int64_t i=0; i<10; ++i)
	// RedisCmd("SET a 1", false);
  // PLAYER_LOG_INFO(GetID(), "aaa{}bbb{}ccc{}", 1, 2, 3);
  // LOG_INFO("");
	GetApp()->_cnt += 1;
	// Push();
	// for (int64_t i=0; i<300; ++i)
	{
	  // co_yield;
	  // static char buf[100];
	  // memcpy(buf, msg->nick_name().c_str(), 100);
		// GetApp()->_i = GetApp()->_testList.Get(i);
		// GetApp()->_i = RandInRange(1, 100);
	}


	Send2Client(0x7f, 0, msg);
	for (int64_t i=0; i<0; ++i)
	  Send2Client(0x7f, 1, msg);
	return nullptr;
}

// ACTOR_MAIL_HANDLE(Player, 0x7f, 1, MsgClientLogin)
ACTOR_MAIL_HANDLE(Player, 0x7f, 1)
{
  // for (int64_t i=0; i<10; ++i)
	// RedisCmd("SET a 1", false);
  // PLAYER_LOG_INFO(GetID(), "aaa{}bbb{}ccc{}", 1, 2, 3);
  // LOG_INFO("");
	GetApp()->_cnt += 1;
	// Push();
	// for (int64_t i=0; i<300; ++i)
	{
	  // co_yield;
	  // static char buf[100];
	  // memcpy(buf, msg->nick_name().c_str(), 100);
		// GetApp()->_i = GetApp()->_testList.Get(i);
		// GetApp()->_i = RandInRange(1, 100);
	}
        for (int64_t i=0; i<1; ++i)
                Send2Client(0x7f, 1, nullptr);
	return nullptr;
}

/*
NET_MSG_HANDLE(LobbyGateSessionBase, E_MCMT_ClientCommon, E_MCCCST_Login, MsgClientLogin)
{
        // LOG_INFO("from:{} to:{}", msgHead.from_, msgHead.to_);
        const auto playerGuid = msg->player_guid();
        if (0 == playerGuid)
        {
                LOG_WARN("玩家[{}] 登录时出错，playerGuid 不能为0!!!", playerGuid);
                return;
        }

        LOG_FATAL_IF(!(0!=msgHead.from_ && playerGuid == static_cast<decltype(playerGuid)>(msgHead.to_)),
                     "playerGuid[{}] msgHead.to_[{}]",
                     playerGuid, msgHead.to_);

        auto createPlayerFunc = [this, &msgHead, &msg](int64_t playerGuid) {
		auto tMsg = std::make_shared<MsgClientLogin>();
		tMsg->CopyFrom(*msg);
		tMsg->set_player_guid(playerGuid);
                auto loginInfo = std::make_shared<stLoginInfo>();
                if (GetPlayerMgrBase()->_loginInfoList.Add(playerGuid, loginInfo))
                {
                        loginInfo->_from = msgHead.from_;
                        loginInfo->_ses = shared_from_this();
                        GetPlayerMgrBase()->GetPlayerMgrActor(playerGuid)->SendPush(0, tMsg);
                }
                else
                {
                        DLOG_WARN("玩家[{}] 在登录时，已经有别的端在登录!!!", playerGuid);
                        MsgClientLoginRet sendMsg;
                        sendMsg.set_error_type(E_CET_InLogin);
                        SendPB(&sendMsg, E_MCMT_ClientCommon, E_MCCCST_Login, MsgHeaderType::E_RMT_Send, E_CET_InLogin, msgHead.to_, msgHead.from_);
                }
        };

        createPlayerFunc(playerGuid);
        for (int64_t i=0; i<500 * 1000; ++i)
                createPlayerFunc(playerGuid + i + 1);
}
*/

// vim: fenc=utf8:expandtab:ts=8
