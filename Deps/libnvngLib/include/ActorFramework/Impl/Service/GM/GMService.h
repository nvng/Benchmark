#pragma once

#include "ActorFramework/ServiceExtra.hpp"
#include "msg_pay.pb.h"

SPECIAL_ACTOR_DEFINE_BEGIN(CDKeyActor, E_MCMT_CDKey);
SPECIAL_ACTOR_DEFINE_END(CDKeyActor);

// #define CDKEY_SERVICE_SERVER
// #define CDKEY_SERVICE_CLIENT
// #define CDKEY_SERVICE_LOCAL

#if defined (CDKEY_SERVICE_LOCAL) || defined (CDKEY_SERVICE_CLIENT)

#include "Player.h"

#endif

DECLARE_SERVICE_BASE_BEGIN(CDKey, SessionDistributeSID, ServiceSession);

private :
        CDKeyServiceBase() : SuperType("CDKeyService") { }
        ~CDKeyServiceBase() override { }

public :
        bool Init() override;

#if defined (CDKEY_SERVICE_LOCAL) || defined (CDKEY_SERVICE_CLIENT)

        std::shared_ptr<MsgPayOrderGuid> ReqOrderGuid(const PlayerPtr& act);

#else
        
        FORCE_INLINE auto GetServiceActor()
        { return SuperType::_actorArr[_idx++ % SuperType::_actorArr.size()].lock(); }
        int64_t _idx = 0;

#endif

DECLARE_SERVICE_BASE_END(CDKey);

#ifdef CDKEY_SERVICE_SERVER
typedef CDKeyServiceBase<E_ServiceType_Server, stLobbyServerInfo> CDKeyService;
#endif

#ifdef CDKEY_SERVICE_CLIENT
typedef CDKeyServiceBase<E_ServiceType_Client, stCDKeyServerInfo> CDKeyService;
#endif

#ifdef CDKEY_SERVICE_LOCAL
typedef CDKeyServiceBase<E_ServiceType_Local, stServerInfoBase> CDKeyService;
#endif

// vim: fenc=utf8:expandtab:ts=8
