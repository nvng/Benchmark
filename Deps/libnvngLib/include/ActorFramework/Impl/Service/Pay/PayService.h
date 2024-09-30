#pragma once

#include "ActorFramework/ServiceExtra.hpp"
#include "msg_pay.pb.h"

SPECIAL_ACTOR_DEFINE_BEGIN(PayActor, E_MCMT_Pay);

public :
        PayActor() : SuperType(1 << 10) { }

SPECIAL_ACTOR_DEFINE_END(PayActor);

// #define PAY_SERVICE_SERVER
// #define PAY_SERVICE_CLIENT
// #define PAY_SERVICE_LOCAL

#if defined (PAY_SERVICE_LOCAL) || defined (PAY_SERVICE_CLIENT)

#include "Player.h"

#endif

struct stPayCfgInfo
{
        int64_t _id = 0;
        int64_t _goodsType = 0;
        int64_t _goodsID = 0;
        int64_t _firstBuy = 0;
        int64_t _extraReward = 0;
        int64_t _price = 0;
        std::string _pay;
};
typedef std::shared_ptr<stPayCfgInfo> stPayCfgInfoPtr;

DECLARE_SERVICE_BASE_BEGIN(Pay, SessionDistributeSID, ServiceSession);

struct stSnowflakePayOrderGuidTag;
typedef Snowflake<stSnowflakePayOrderGuidTag, SNOW_FLAKE_START_TIME> SnowflakePayOrderGuid;

private :
        PayServiceBase() : SuperType("PayService") { }
        ~PayServiceBase() override { }

public :
        bool Init() override;
        bool ReadPayConfig();
        UnorderedMap<uint64_t, stPayCfgInfoPtr> _payCfgList;
        UnorderedMap<std::string, stPayCfgInfoPtr> _payCfgListByPay;

#if defined (PAY_SERVICE_LOCAL) || defined (PAY_SERVICE_CLIENT)

        std::shared_ptr<MsgPayOrderGuid> ReqOrderGuid(const PlayerPtr& act);

#else
        
        FORCE_INLINE auto GetServiceActor()
        { return SuperType::_actorArr[_idx++ % SuperType::_actorArr.size()].lock(); }
        int64_t _idx = 0;

        typedef std::function<void(const PayActorPtr&, const std::shared_ptr<stMailHttpReq>&)> PayCallBackFuncType;
        std::unordered_map<std::string, PayCallBackFuncType> _callbackFuncTypeList;

#endif

DECLARE_SERVICE_BASE_END(Pay);

#ifdef PAY_SERVICE_SERVER
typedef PayServiceBase<E_ServiceType_Server, stLobbyServerInfo> PayService;
#endif

#ifdef PAY_SERVICE_CLIENT
typedef PayServiceBase<E_ServiceType_Client, stPayServerInfo> PayService;
#endif

#ifdef PAY_SERVICE_LOCAL
typedef PayServiceBase<E_ServiceType_Local, stServerInfoBase> PayService;
#endif

// vim: fenc=utf8:expandtab:ts=8
