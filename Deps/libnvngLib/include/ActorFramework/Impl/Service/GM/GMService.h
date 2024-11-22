#pragma once

#include "ActorFramework/ServiceExtra.hpp"

#if defined(GM_SERVICE_SERVER) || defined(GM_SERVICE_CLIENT)

SPECIAL_ACTOR_DEFINE_BEGIN(GMActor, E_MIMT_GM);
public :
        bool Init() override;

#ifdef GM_SERVICE_SERVER
        void Add2DB(uint64_t module, uint64_t guid, const std::string& data);
        void UpdateState(uint64_t module, uint64_t guid, uint64_t state);
        std::string GetData(uint64_t module, uint64_t state = UINT64_MAX);
#endif

SPECIAL_ACTOR_DEFINE_END(GMActor);

struct stGMResult : stActorMailBase
{
        std::pair<bool, std::string> _ret;
};

#ifdef GM_SERVICE_SERVER
struct stSnowflakeGMGuidTag;
typedef Snowflake<stSnowflakeGMGuidTag, uint64_t, 1692871881000> SnowflakeGMGuid;
#endif

DECLARE_SERVICE_BASE_BEGIN(GM, SessionDistributeSID, ServiceSession);

private :
        GMServiceBase() : SuperType("GMService") { }
        ~GMServiceBase() override { }

public :
        bool Init() override;

        template <typename ... Args>
        bool Start(Args ... args)
        {
                if constexpr (E_ServiceType_Client == ServiceType)
                {
                        ThisType::StartLocal(1);
                        ::nl::net::NetMgrBase<typename SessionType::Tag>::GetInstance()->Connect(args..., [](auto&& s) {
                                return std::make_shared<SessionType>(std::move(s));
                        });
                }
                else
                {
                        return SuperType::Start(args...);
                }
                return true;
        }

        FORCE_INLINE GMActorPtr GetServiceActor() { return SuperType::_actorArr[0].lock(); }

public :
        typedef std::function<std::pair<bool, std::string>(const GMActorPtr& gmActor, const std::shared_ptr<MailGMOpt>&)> ModuleOptFuncType;
        void RegistOpt(int64_t module, int64_t opt, const ModuleOptFuncType& cb)
        {
                auto key = std::make_pair(module, opt);
                _moduleOptList.emplace(key, std::move(cb));
        }

        std::map<std::pair<int64_t, int64_t>, ModuleOptFuncType> _moduleOptList;

DECLARE_SERVICE_BASE_END(GM);

#ifdef GM_SERVICE_SERVER
typedef GMServiceBase<E_ServiceType_Server, stLobbyServerInfo> GMService;
#endif

#ifdef GM_SERVICE_CLIENT
typedef GMServiceBase<E_ServiceType_Client, stGMServerInfo> GMService;
#endif

#endif

// vim: fenc=utf8:expandtab:ts=8
