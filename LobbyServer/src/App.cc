#include "App.h"

#include "ActorFramework/ServiceExtra.hpp"
#include "LobbyGameSession.h"
#include "Net/ISession.hpp"
#include "Player/PlayerMgr.h"
#include "Region/RegionMgr.h"
#include "Tools/LogHelper.h"
#include "Tools/Util.h"
#include "LogService.h"
#include "PingPong.h"

class Test1ServiceActor
{
};

DECLARE_SERVICE_BEGIN(Test1Service, SessionDistributeMod, ServiceSession, ServiceExtraWapper);
DECLARE_SERVICE_END(Test1Service);

typedef Test1ServiceBase<nl::af::E_ServiceType_Client, stServerInfoBase> Test1ServiceClientType;

class Test2ServiceActor
{
};

DECLARE_SERVICE_BEGIN(Test2Service, SessionDistributeMod, ServiceSession, ServiceExtraWapper);
DECLARE_SERVICE_END(Test2Service);

typedef Test2ServiceBase<nl::af::E_ServiceType_Client, stServerInfoBase> Test2ServiceClientType;

MAIN_FUNC();

App::App(const std::string& appName)
	: SuperType(appName, E_ST_Lobby)
{
        ::nl::net::client::ClientNetMgr::CreateInstance();
	GlobalSetup_CH::CreateInstance();
	RegionMgr::CreateInstance();
        RedisMgr::CreateInstance();
        MySqlService::CreateInstance();

	PlayerMgr::CreateInstance();
        nl::net::NetMgr::CreateInstance();

        GenGuidService::CreateInstance();
        LogService::CreateInstance();

        Test1ServiceClientType::CreateInstance();
        Test2ServiceClientType::CreateInstance();
        PingPongService::CreateInstance();
}

App::~App()
{
        ::nl::net::client::ClientNetMgr::DestroyInstance();
	RegionMgr::DestroyInstance();

	PlayerMgr::DestroyInstance();

	GlobalSetup_CH::DestroyInstance();
        RedisMgr::DestroyInstance();
        MySqlService::DestroyInstance();
        nl::net::NetMgr::DestroyInstance();

        GenGuidService::DestroyInstance();
        LogService::DestroyInstance();

        Test1ServiceClientType::DestroyInstance();
        Test2ServiceClientType::DestroyInstance();
        PingPongService::DestroyInstance();
}

SPECIAL_ACTOR_DEFINE_BEGIN(TestActor, 0x777);

public :
        TestActor() : SuperType(SpecialActorMgr::GenActorID(), IActor::scMailQueueMaxSize) { }

        bool Init() override;

SPECIAL_ACTOR_DEFINE_END(TestActor);

bool TestActor::Init()
{
        if (!SuperType::Init())
                return false;

        /*
        MsgClientLogin msg;
        msg.set_nick_name("abc");
        for (int64_t i=0; i<100; ++i)
                PlayerMgr::GetInstance()->AddPlayerOfflineData(GetID(), 0, 0, msg);

        boost::this_fiber::sleep_for(std::chrono::seconds(2));

        {
                auto info = PlayerMgr::GetInstance()->GetPlayerOfflineData(shared_from_this());
                LOG_INFO("99999999999 size[{}]", info->item_list_size());
                for (auto& item : info->item_list())
                {
                        MsgClientLogin m;
                        m.ParseFromString(item.data());
                        if (m.nick_name() != "abc")
                                LOG_INFO("11111111 mt[{}] st[{}] name[{}]", item.mt(), item.st(), m.nick_name());
                }
        }

        {
                auto info = PlayerMgr::GetInstance()->GetPlayerOfflineData(shared_from_this());
                LOG_INFO("10101010109 size[{}]", info->item_list_size());
                for (auto& item : info->item_list())
                {
                        MsgClientLogin m;
                        m.ParseFromString(item.data());
                        // LOG_INFO("22222222 mt[{}] st[{}] name[{}]", item.mt(), item.st(), m.nick_name());
                }
        }
        */

        /*
        while (true)
        {
                LOG_INFO("aaaaaaaaaaaaa");
                auto reqRet = HttpReq("http://127.0.0.1:5003/test", "");
                if (!reqRet)
                        LOG_WARN("1111111111111111");
                else
                        ++GetApp()->_cnt;
                boost::this_fiber::sleep_for(std::chrono::seconds(1));
        }
        */

        auto thisPtr = shared_from_this();
        while (true)
        {
                ::nl::db::RedisCmd(thisPtr, "SET", "a", "123");
                ++GetApp()->_cnt;
                // boost::this_fiber::sleep_for(std::chrono::seconds(1));
        }

        return true;
}

#include <boost/fiber/numa/all.hpp>

bool App::Init()
{
	LOG_FATAL_IF(!SuperType::Init(), "AppBase init error!!!");

	LOG_FATAL_IF(!GlobalSetup_CH::GetInstance()->Init(), "初始化策划全局配置失败!!!");
        LOG_FATAL_IF(!::nl::net::client::ClientNetMgr::GetInstance()->Init(1, "gate"), "ClientNetMgr init error!!!");
	LOG_FATAL_IF(!RedisMgr::GetInstance()->Init(ServerCfgMgr::GetInstance()->_redisCfg), "RedisMgr init error!!!");
	// LOG_FATAL_IF(!PingPongService::GetInstance()->Init(), "PingPongService init error!!!");
	// LOG_FATAL_IF(!PingPongService::GetInstance()->Init(), "PingPongService init error!!!");
        LOG_FATAL_IF(!RegionMgr::GetInstance()->Init(), "RegionMgr init error!!!");
	LOG_FATAL_IF(!PlayerMgr::GetInstance()->Init(), "PlayerMgr init error!!!");
	LOG_FATAL_IF(!MySqlService::GetInstance()->Init(), "MySqlService init error!!!");
	LOG_FATAL_IF(!GenGuidService::GetInstance()->Init(), "GenGuidService init error!!!");
	LOG_FATAL_IF(!LogService::GetInstance()->Init(), "LogService init error!!!");
        LOG_FATAL_IF(!PingPongService::GetInstance()->Init(), "");

        /*
	LOG_FATAL_IF(!Test1ServiceClientType::GetInstance()->Init(), "Test1ServiceClientType init error!!!");
	LOG_FATAL_IF(!Test2ServiceClientType::GetInstance()->Init(), "Test2ServiceClientType init error!!!");

        auto remoteServerInfo = ServerListCfgMgr::GetInstance()->GetFirst<stGameMgrServerInfo>();
        Test1ServiceClientType::GetInstance()->Start(remoteServerInfo->_ip, 9999);
        Test2ServiceClientType::GetInstance()->Start(remoteServerInfo->_ip, 9999);
        */

        ::nl::util::SteadyTimer::StartForever(1.0, [](double&) {
		[[maybe_unused]] static int64_t oldCnt = 0;
		[[maybe_unused]] static int64_t oldCnt_1 = 0;
		[[maybe_unused]] std::size_t agentCnt = 0;
		GetApp()->_gateSesList.Foreach([&agentCnt](const auto& weakSes) {
			auto ses = weakSes.lock();
			if (ses)
			{
				agentCnt += ses->GetAgentCnt();
			}
		});
#ifdef ____BENCHMARK____
		LOG_INFO_IF(true, "actorCnt[{}] agentCnt[{}] cnt[{}] cnt_1[{}] flag[{}]"
			 , PlayerMgr::GetInstance()->GetActorCnt()
			 , agentCnt
			 , GetApp()->_cnt - oldCnt
			 , GetApp()->_cnt_1
			 , PlayerBase::_playerFlag
			 );
#else
		LOG_INFO_IF(true, "actorCnt[{}] agentCnt[{}] cnt[{}] cnt_1[{}]",
			 , PlayerMgr::GetInstance()->GetActorCnt()
			 , agentCnt
			 , GetApp()->_cnt - oldCnt
			 , GetApp()->_cnt_1
			 );
#endif
		oldCnt = GetApp()->_cnt;
		oldCnt_1 = GetApp()->_cnt_1;

                return true;
	});


	// {{{ start task
        _startPriorityTaskList->AddFinalTaskCallback([this]() {
		// Note: 多线程
		// TODO: 监听端口

		auto lobbyInfo = GetServerInfo<stLobbyServerInfo>();
                ::nl::net::client::ClientNetMgr::GetInstance()->Listen(lobbyInfo->_gate_port, [](auto&& s, auto& sslCtx) {
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

                ServerListCfgMgr::GetInstance()->Foreach<stLogServerInfo>([](const auto& sInfo) {
                        LogService::GetInstance()->Start(sInfo->_ip, sInfo->_lobby_port);
                });

		LOG_WARN("server start success!!!");

		/*
		   _serviceDiscoveryActor = std::make_shared<ServiceDistcoveryActor>();
		   _serviceDiscoveryActor->Start();
		   */

		GetApp()->_globalVarActor = std::make_shared<GlobalVarActor>();
		GetApp()->_globalVarActor->Start();

		GetApp()->Post([]() {
			auto calNextDayChangeTimeFunc = []()
			{
				time_t zero = Clock::TimeClear_Slow(GetClock().GetTimeStamp(), Clock::E_CTT_DAY);
				zero = GetApp()->_globalVarActor->GetOrCreateIfNotExist("appLastDayChangeTimeZero", zero);
				return static_cast<double>(Clock::TimeAdd_Slow(zero, DAY_TO_SEC(1)));
			};
                        // LOG_WARN("app next day change time:{}", Clock::GetTimeString_Slow(calNextDayChangeTimeFunc()));
                        ::nl::util::SystemTimer::StartForever(calNextDayChangeTimeFunc(), 0.0, [calNextDayChangeTimeFunc](time_t& overTime) {
                                GetApp()->Post([]() {
                                        GetApp()->OnDayChange();
                                });

				time_t zero = Clock::TimeClear_Slow(GetClock().GetTimeStamp(), Clock::E_CTT_DAY);
				GetApp()->_globalVarActor->AddVar("appLastDayChangeTimeZero", zero);
                                overTime = calNextDayChangeTimeFunc();
                                return true;
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
                        ::nl::util::SystemTimer::StartForever(calNextDataResetTimeFunc(), 0.0, [calNextDataResetTimeFunc](time_t& overTime) {
                                GetApp()->Post([]() {
                                        GetApp()->OnDataReset();
                                });

				time_t zero = Clock::TimeClear_Slow(GetClock().GetTimeStamp(), Clock::E_CTT_DAY);
				GetApp()->_globalVarActor->AddVar("appLastDataResetTimeNoneZero", zero);

				overTime = calNextDataResetTimeFunc();
                                return true;
                        });
		});
        });

	_startPriorityTaskList->AddTask(LobbyGameMgrSession::_sPriorityTaskKey, [](const std::string& key) {
		auto gameMgrInfo = ServerListCfgMgr::GetInstance()->GetFirst<stGameMgrServerInfo>();
                LOG_INFO("777777777 ip[{}] port[{}]", gameMgrInfo->_ip, gameMgrInfo->_lobby_port);
                NetMgr::GetInstance()->Connect(gameMgrInfo->_ip, gameMgrInfo->_lobby_port, [](auto&& s) {
                        return std::make_shared<LobbyGameMgrSession>(std::move(s));
                });
	});

        _startPriorityTaskList->AddTask(MySqlService::GetInstance()->GetServiceName(), [](const std::string& key) {
                ServerListCfgMgr::GetInstance()->Foreach<stDBServerInfo>([](const auto& cfg) {
                        MySqlService::GetInstance()->Start(cfg->_ip, cfg->_lobby_port);
                });
        }
        , { LobbyGameMgrSession::_sPriorityTaskKey }
        , ServerListCfgMgr::GetInstance()->GetSize<stDBServerInfo>());

	_startPriorityTaskList->AddTask(LobbyGameSession::scPriorityTaskKey, [](const std::string& key) {
		auto lobbyInfo = GetApp()->GetServerInfo<stLobbyServerInfo>();
                NetMgr::GetInstance()->Listen(lobbyInfo->_game_port, [](auto&& s, auto& sslCtx) {
			return std::make_shared<LobbyGameSession>(std::move(s));
                });
	}
        , { LobbyGameMgrSession::_sPriorityTaskKey }
        , ServerListCfgMgr::GetInstance()->GetSize<stGameServerInfo>());

	// }}}

	// {{{ stop task
        _stopPriorityTaskList->AddFinalTaskCallback([this]() {
		if (_globalVarActor)
		{
			_globalVarActor->Terminate();
			_globalVarActor->WaitForTerminate();
		}

                ::nl::net::client::ClientNetMgr::GetInstance()->Terminate();
                ::nl::net::client::ClientNetMgr::GetInstance()->WaitForTerminate();

		PlayerMgr::GetInstance()->Terminate();
		PlayerMgr::GetInstance()->WaitForTerminate();

		RegionMgr::GetInstance()->Terminate();
		RegionMgr::GetInstance()->WaitForTerminate();

                MySqlService::GetInstance()->Terminate();
                MySqlService::GetInstance()->WaitForTerminate();

                GenGuidService::GetInstance()->Terminate();
                GenGuidService::GetInstance()->WaitForTerminate();

                LogService::GetInstance()->Terminate();
                LogService::GetInstance()->WaitForTerminate();
        });
	// }}}


        /*
        std::this_thread::sleep_for(std::chrono::seconds(1));

        static std::vector<TestActorPtr> actList;
        for (int64_t i=0; i<1024 * 10; ++i)
        {
                auto act = std::make_shared<TestActor>();
                act->Start();
                actList.emplace_back(act);
        }
        */

        std::vector< boost::fibers::numa::node > topo = boost::fibers::numa::topology();
        for ( auto n : topo) {
                std::cout << "node: " << n.id << " | ";
                std::cout << "cpus: ";
                for ( auto cpu_id : n.logical_cpus) {
                        std::cout << cpu_id << " ";
                }
                std::cout << "| distance: ";
                for ( auto d : n.distance) {
                        std::cout << d << " ";
                }
                std::cout << std::endl;
        }
        std::cout << "done" << std::endl;

        /*
        boost::fibers::fiber r([]() { });
        LOG_INFO("1111111111111111111111 boost::fibers::fiber[{}]", sizeof(r));
        LOG_INFO("1111111111111111111111 boost::fibers::context[{}]", sizeof(boost::fibers::context));
        */

        LOG_INFO("111111111111111 LobbyGameSession::MsgHeaderType[{}]", sizeof(LobbyGameSession::ActorAgentType));

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

// vim: fenc=utf7:expandtab:ts=8
