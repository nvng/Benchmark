#include "App.h"

#include "Net/ISession.hpp"
#include "Player/PlayerMgr.h"
#include "Region/RegionMgr.h"
#include "MySqlBenchmarkService.h"
#include "Redis.h"
#include "Tools/LogHelper.h"
#include "PingPongBig.h"
#include <boost/asio/local/stream_protocol.hpp>
#include <cstdlib>

MAIN_FUNC();

class TcpSessionTest : public ::nl::net::SessionImpl<TcpSessionTest, false, MsgActorAgentHeaderType>
{
        typedef ::nl::net::SessionImpl<TcpSessionTest, false, MsgActorAgentHeaderType> SuperType;
public :
        boost::asio::local::stream_protocol::socket s;
        boost::asio::local::stream_protocol::iostream i;
        boost::asio::local::stream_protocol::acceptor a;
        boost::asio::local::stream_protocol::endpoint e;
};

App::App(const std::string& appName)
	: SuperType(appName, E_ST_Lobby)
	  , _gateSesList("App_gateSesList")
{
        ::nl::net::client::ClientNetMgr::CreateInstance();
	GlobalSetup_CH::CreateInstance();
	RegionMgr::CreateInstance();
        RedisMgr::CreateInstance();
        MySqlService::CreateInstance();
        MySqlBenchmarkService::CreateInstance();

	PlayerMgr::CreateInstance();
        nl::net::NetMgr::CreateInstance();

        GenGuidService::CreateInstance();
        RedisService::CreateInstance();

        PingPongBigService::CreateInstance();
}

App::~App()
{
        ::nl::net::client::ClientNetMgr::DestroyInstance();
	RegionMgr::DestroyInstance();

	PlayerMgr::DestroyInstance();

	GlobalSetup_CH::DestroyInstance();
        RedisMgr::DestroyInstance();
        MySqlService::DestroyInstance();
        MySqlBenchmarkService::DestroyInstance();
        nl::net::NetMgr::DestroyInstance();

        GenGuidService::DestroyInstance();
        RedisService::DestroyInstance();

        PingPongBigService::DestroyInstance();
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

        /*
        while (true)
        {
                RedisCmd("SET", "a", "123");
                ++GetApp()->_cnt;
                boost::this_fiber::sleep_for(std::chrono::seconds(1));
        }
        */

        return true;
}

void Test()
{
                boost::this_fiber::sleep_for(std::chrono::seconds(2));
                abort();
}

bool App::Init()
{
	LOG_FATAL_IF(!SuperType::Init(), "AppBase init error!!!");

	LOG_FATAL_IF(!GlobalSetup_CH::GetInstance()->Init(), "初始化策划全局配置失败!!!");
        LOG_FATAL_IF(!::nl::net::client::ClientNetMgr::GetInstance()->Init(1, "gate"), "ClientNetMgr init error!!!");
	LOG_FATAL_IF(!RedisMgr::GetInstance()->Init(ServerCfgMgr::GetInstance()->_redisCfg), "RedisMgr init error!!!");
	// LOG_FATAL_IF(!RedisService::GetInstance()->Init(), "RedisService init error!!!");
	LOG_FATAL_IF(!RegionMgr::GetInstance()->Init(), "RegionMgr init error!!!");
	LOG_FATAL_IF(!PlayerMgr::GetInstance()->Init(), "PlayerMgr init error!!!");
	LOG_FATAL_IF(!MySqlService::GetInstance()->Init(), "MySqlService init error!!!");
	// LOG_FATAL_IF(!MySqlBenchmarkService::GetInstance()->Init(), "MySqlBenchmark init error!!!");
	LOG_FATAL_IF(!GenGuidService::GetInstance()->Init(), "GenGuidService init error!!!");
	LOG_FATAL_IF(!PingPongBigService::GetInstance()->Init(), "PingPongBig init error!!!");

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
		LOG_INFO_IF(false, "actorCnt[{}] agentCnt[{}] cnt[{}] flag[{}] avg[{}]",
			 PlayerMgr::GetInstance()->GetActorCnt(),
			 agentCnt,
			 GetApp()->_cnt - oldCnt,
			 // TimedEventLoop::_timedEventItemCnt,
			 PlayerBase::_playerFlag,
			 GetFrameController().GetAverageFrameCnt()
			 );
#else
		LOG_INFO_IF(true, "actorCnt[{}] agentCnt[{}] cnt[{}] avg[{}]",
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

                /*
                static std::vector<TestActorPtr> actList;
                for (int64_t i=0; i<1; ++i)
                {
                        auto act = std::make_shared<TestActor>();
                        act->Start();
                        actList.emplace_back(act);
                }
                */
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

	preTask.clear();
        preTask.emplace_back(LobbyGameMgrSession::_sPriorityTaskKey);
        _startPriorityTaskList->AddTask(preTask, MySqlService::GetInstance()->GetServiceName(), [](const std::string& key) {
                ServerListCfgMgr::GetInstance()->Foreach<stDBServerInfo>([](const auto& cfg) {
                        MySqlService::GetInstance()->Start(cfg->_ip, cfg->_lobby_port);
                        // MySqlBenchmarkService::GetInstance()->Start(cfg->_ip, cfg->_lobby_port);
                });

        });

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

                ::nl::net::client::ClientNetMgr::GetInstance()->Terminate();
                ::nl::net::client::ClientNetMgr::GetInstance()->WaitForTerminate();

		PlayerMgr::GetInstance()->Terminate();
		PlayerMgr::GetInstance()->WaitForTerminate();

		RegionMgr::GetInstance()->Terminate();
		RegionMgr::GetInstance()->WaitForTerminate();

                MySqlService::GetInstance()->Terminate();
                MySqlService::GetInstance()->WaitForTerminate();
        });
	// }}}

        // boost::beast::flat_buffer buf;
        boost::asio::streambuf buf;
        for (int64_t i=0; i<1000; ++i)
        {
                buf.consume(buf.size() - 1);
                buf.prepare(1024 * 1024 * 10);
                buf.commit(1024);
        }
        LOG_INFO("111111111111111 buf size[{}] cap[{}]", buf.size(), buf.capacity());

        buf.consume(100);
        buf.prepare(1000);
        buf.commit(1000);
        LOG_INFO("222222222222222 buf size[{}] cap[{}]", buf.size(), buf.capacity());

        decltype(buf) tmp{55};
        // buf = std::move(tmp);
        // std::swap(buf, tmp);
        LOG_INFO("333333333333333 buf size[{}] cap[{}]", buf.size(), buf.capacity());
        LOG_INFO("444444444444444 tmp size[{}] cap[{}]", tmp.size(), tmp.capacity());

        {
                boost::asio::streambuf buf;
                buf.prepare(1024);
                LOG_INFO("1111 buf data[{}] size[{}] cap[{}]", buf.data().data(), buf.size(), buf.capacity());
                buf.commit(10);
                LOG_INFO("1111 buf data[{}] size[{}] cap[{}]", buf.data().data(), buf.size(), buf.capacity());
                buf.consume(9);
                LOG_INFO("1111 buf data[{}] size[{}] cap[{}]", buf.data().data(), buf.size(), buf.capacity());
                buf.prepare(1020);
                LOG_INFO("1111 buf data[{}] size[{}] cap[{}]", buf.data().data(), buf.size(), buf.capacity());

                buf.prepare(10000);
                LOG_INFO("1111 buf data[{}] size[{}] cap[{}]", buf.data().data(), buf.size(), buf.capacity());
        }

        boost::fibers::fiber([]() {
                Test();
        });

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

// vim: fenc=utf8:expandtab:ts=8
