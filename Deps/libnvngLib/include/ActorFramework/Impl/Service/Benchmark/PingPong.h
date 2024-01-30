#pragma once

class PingPongActor : public ActorImpl<PingPongActor, SpecialActorMgr>
{
        typedef ActorImpl<PingPongActor, SpecialActorMgr> SuperType;
public :
        PingPongActor()
                : SuperType(SpecialActorMgr::GetInstance()->GenActorID())
        {
        }
        
        EXTERN_ACTOR_MAIL_HANDLE();
};

// #define PING_PONG_SERVICE_SERVER
// #define PING_PONG_SERVICE_CLIENT
// #define PING_PONG_SERVICE_LOCAL

#include "ActorFramework/ServiceExtra.hpp"

DECLARE_SERVICE_BASE_BEGIN(PingPong, SessionDistributeMod, ServiceSession);

        constexpr static int64_t scSessionCnt = 16;
        constexpr static int64_t scPkgPerSession = 1000 * 1000;

private :
        PingPongServiceBase() : SuperType("PingPongService") { }
        ~PingPongServiceBase() override { }

public :
        bool Init() override
        {
                if (!SuperType::Init())
                        return false;
                return true;

#ifdef PING_PONG_SERVICE_SERVER
                SuperType::Start(60000);
#endif

#ifdef PING_PONG_SERVICE_CLIENT
                for (int64_t i=0; i<scSessionCnt; ++i)
                {
                        auto remoteServerInfo = ServerListCfgMgr::GetInstance()->GetFirst<stGameMgrServerInfo>();
                        SuperType::Start(remoteServerInfo->_ip, 60000);
                }

                auto act = std::make_shared<PingPongActor>();
                act->SendPush(nullptr, 0xfff, 0x0, nullptr);
                act->Start();
#endif
                return true;
        }

DECLARE_SERVICE_BASE_END(PingPong);

#ifdef PING_PONG_SERVICE_SERVER
typedef PingPongServiceBase<E_ServiceType_Server, stLobbyServerInfo> PingPongService;
#endif

#ifdef PING_PONG_SERVICE_CLIENT
typedef PingPongServiceBase<E_ServiceType_Client, stGameMgrServerInfo> PingPongService;
#endif

// vim: fenc=utf8:expandtab:ts=8
