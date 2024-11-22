#pragma once

#if defined(CDKEY_SERVICE_SERVER) || defined(CDKEY_SERVICE_CLIENT) || defined(CDKEY_SERVICE_LOCAL)

#include "ActorFramework/ServiceExtra.hpp"
#include "msg_cdkey.pb.h"

SPECIAL_ACTOR_DEFINE_BEGIN(CDKeyActor, E_MCMT_CDKey);

public :
        bool Init() override;

        std::pair<bool, std::string> AddInfo(const std::shared_ptr<MsgCDKeyInfo>& info);
        std::string PackCDKeyInfo2Json(const MsgCDKeyInfo& msg);
        static std::shared_ptr<MsgCDKeyInfo> UnPackCDKeyInfoFromJson(const rapidjson::Value& data);

        void LoadFromDB();
        void Save2DB();
        void Flush2DB();

        void Terminate() override;

        ::nl::util::SteadyTimer _saveTimer;
        bool _inTimer = false;
        UnorderedMap<std::string, int64_t> _cdkeyListByKey;
        std::atomic_uint64_t _groupGuid = 0;
        UnorderedMap<int64_t, std::shared_ptr<MsgCDKeyInfo>> _cdkeyList;

SPECIAL_ACTOR_DEFINE_END(CDKeyActor);

// #define CDKEY_SERVICE_SERVER
// #define CDKEY_SERVICE_CLIENT
// #define CDKEY_SERVICE_LOCAL

DECLARE_SERVICE_BASE_BEGIN(CDKey, SessionDistributeSID, ServiceSession);

private :
        CDKeyServiceBase()
                : SuperType("CDKeyService")
        {
        }

        ~CDKeyServiceBase() override { }

public :
        bool Init() override;

        template <typename ... Args>
        bool StartLocal(int64_t actCnt, Args ... args)
        {
                auto act = std::make_shared<typename SuperType::ActorType>(std::forward<Args>(args)...);
                SuperType::_actorArr.emplace_back(act);
                act->Start();
                return true;
        }

        bool GMOpt(int64_t opt, const rapidjson::Value& data);

        FORCE_INLINE CDKeyActorPtr GetActor_() const { return SuperType::_actorArr[0].lock(); }

DECLARE_SERVICE_BASE_END(CDKey);

#ifdef CDKEY_SERVICE_SERVER
typedef CDKeyServiceBase<E_ServiceType_Server, stLobbyServerInfo> CDKeyService;
#endif

#ifdef CDKEY_SERVICE_CLIENT
typedef CDKeyServiceBase<E_ServiceType_Client, stGMServerInfo> CDKeyService;
#endif

#ifdef CDKEY_SERVICE_LOCAL
typedef CDKeyServiceBase<E_ServiceType_Local, stServerInfoBase> CDKeyService;
#endif

#endif

// vim: fenc=utf8:expandtab:ts=8
