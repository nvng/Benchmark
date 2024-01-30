#pragma once

#include "Tools/ServerList.hpp"
class MySqlActor : public ActorImpl<MySqlActor, SpecialActorMgr>
{
        typedef ActorImpl<MySqlActor, SpecialActorMgr> SuperType;
public :
        MySqlActor()
                : SuperType(SpecialActorMgr::GetInstance()->GenActorID())
        {
        }
        
        friend class ActorImpl<MySqlActor, SpecialActorMgr>;
        EXTERN_ACTOR_MAIL_HANDLE();
        DECLARE_SHARED_FROM_THIS(MySqlActor);
};

// #define MYSQL_SERVICE_SERVER
// #define MYSQL_SERVICE_CLIENT
// #define MYSQL_SERVICE_LOCAL

#include "ActorFramework/ServiceExtra.hpp"

DECLARE_SERVICE_BASE_BEGIN(MySql, SessionDistributeMod, ServiceSession);

        constexpr static int64_t scSessionCnt = 4;
        constexpr static int64_t scActorCnt = 1000 * 10;

private :
        MySqlServiceBase() : SuperType("MySqlService") { }
        ~MySqlServiceBase() override { }

public :
        bool Init() override
        {
                if (!SuperType::Init())
                        return false;

#ifdef MYSQL_SERVICE_CLIENT
                auto dbInfo = ServerListCfgMgr::GetInstance()->GetFirst<stDBServerInfo>();
                for (int64_t i=0; i<scSessionCnt; ++i)
                        SuperType::Start(dbInfo->_ip, dbInfo->_lobby_port);
                for (int64_t i=0; i<scActorCnt; ++i)
                {
                        auto act = std::make_shared<MySqlActor>();
                        act->SendPush(nullptr, 0xfff, 0x0, nullptr);
                        act->Start();
                }
#endif
                return true;
        }

DECLARE_SERVICE_BASE_END(MySql);

#ifdef MYSQL_SERVICE_CLIENT
typedef MySqlServiceBase<E_ServiceType_Client, stDBServerInfo> MySqlService;
#endif

// vim: fenc=utf8:expandtab:ts=8
