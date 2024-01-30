#pragma once

#if defined (REDIS_SERVICE_LOCAL) || defined (REDIS_SERVICE_CLIENT) || defined (REDIS_SERVICE_SERVER)

#include "Tools/ServerList.hpp"
struct stRedisTestFalg;

class RedisActor : public ActorImpl<RedisActor, SpecialActorMgr, ActorMail, stRedisTestFalg>
{
        typedef ActorImpl<RedisActor, SpecialActorMgr, ActorMail, stRedisTestFalg> SuperType;
public :
        RedisActor()
                : SuperType(SpecialActorMgr::GetInstance()->GenActorID())
        {
        }
        
        EXTERN_ACTOR_MAIL_HANDLE();
};

// #define REDIS_SERVICE_SERVER
// #define REDIS_SERVICE_CLIENT
// #define REDIS_SERVICE_LOCAL

#include "ActorFramework/ServiceExtra.hpp"

DECLARE_SERVICE_BASE_BEGIN(Redis, SessionDistributeMod, ServiceSession);

        constexpr static int64_t scActorCnt = 1000 * 100;

private :
        RedisServiceBase() : SuperType("RedisService") { }
        ~RedisServiceBase() override { }

public :
        bool Init() override
        {
                if (!SuperType::Init())
                        return false;

                RedisMgrBase<stRedisTestFalg>::CreateInstance();
                RedisMgrBase<stRedisTestFalg>::GetInstance()->Init(ServerCfgMgr::GetInstance()->_redisCfg, "benchmark");

#ifdef REDIS_SERVICE_CLIENT
                for (int64_t i=0; i<scActorCnt; ++i)
                {
                        auto act = std::make_shared<RedisActor>();
                        act->SendPush(nullptr, 0xfff, 0x0, nullptr);
                        act->Start();
                }
#endif
                return true;
        }

DECLARE_SERVICE_BASE_END(Redis);

#ifdef REDIS_SERVICE_CLIENT
typedef RedisServiceBase<E_ServiceType_Client, stLobbyServerInfo> RedisService;
#endif

#endif

// vim: fenc=utf8:expandtab:ts=8
