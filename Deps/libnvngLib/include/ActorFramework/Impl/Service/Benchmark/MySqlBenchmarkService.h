#pragma once

#include "Tools/ServerList.hpp"
class MySqlBenchmarkActor : public ActorImpl<MySqlBenchmarkActor, SpecialActorMgr>
{
        typedef ActorImpl<MySqlBenchmarkActor, SpecialActorMgr> SuperType;
public :
        MySqlBenchmarkActor()
                : SuperType(SpecialActorMgr::GetInstance()->GenActorID())
        {
        }
        
        friend class ActorImpl<MySqlBenchmarkActor, SpecialActorMgr>;
        EXTERN_ACTOR_MAIL_HANDLE();
        DECLARE_SHARED_FROM_THIS(MySqlBenchmarkActor);
};

// #define MYSQL_BENCHMARK_SERVICE_SERVER
// #define MYSQL_BENCHMARK_SERVICE_CLIENT
// #define MYSQL_BENCHMARK_SERVICE_LOCAL

#include "ActorFramework/ServiceExtra.hpp"

DECLARE_SERVICE_BASE_BEGIN(MySqlBenchmark, SessionDistributeMod, ServiceSession);

        constexpr static int64_t scSessionCnt = 4;
        constexpr static int64_t scActorCnt = 1000 * 10;

private :
        MySqlBenchmarkServiceBase() : SuperType("MySqlBenchmarkService") { }
        ~MySqlBenchmarkServiceBase() override { }

public :
        bool Init() override
        {
                if (!SuperType::Init())
                        return false;

#ifdef MYSQL_BENCHMARK_SERVICE_CLIENT
                auto dbInfo = ServerListCfgMgr::GetInstance()->GetFirst<stDBServerInfo>();
                for (int64_t i=0; i<scSessionCnt; ++i)
                        SuperType::Start(dbInfo->_ip, dbInfo->_lobby_port);
                for (int64_t i=0; i<scActorCnt; ++i)
                {
                        auto act = std::make_shared<MySqlBenchmarkActor>();
                        act->SendPush(nullptr, 0xfff, 0x0, nullptr);
                        act->Start();
                }
#endif
                return true;
        }

DECLARE_SERVICE_BASE_END(MySqlBenchmark);

#ifdef MYSQL_BENCHMARK_SERVICE_CLIENT
typedef MySqlBenchmarkServiceBase<E_ServiceType_Client, stDBServerInfo> MySqlBenchmarkService;
#endif

// vim: fenc=utf8:expandtab:ts=8
