#pragma once

#ifdef GM_SERVICE_SERVER

#include "ActorFramework/ServiceExtra.hpp"

struct stMailGMData : public stActorMailBase
{
        rapidjson::Value _data;
};

SPECIAL_ACTOR_DEFINE_BEGIN(GMActor, 0xefc);
SPECIAL_ACTOR_DEFINE_END(GMActor);

DECLARE_SERVICE_BASE_BEGIN(GM, SessionDistributeSID, ServiceSession);

private :
        GMServiceBase() : SuperType("GMService") { }
        ~GMServiceBase() override { }

public :
        bool Init() override;

        template <typename ... Args>
        bool StartServer(uint16_t port, Args ... args)
        {
                if (SuperType::_actorArr.empty())
                {
                        auto serverInfo = GetAppBase()->GetServerInfo<stServerInfoBase>();
                        SuperType::StartLocal(1, std::forward<Args>(args)...);
                }

                return true;
        }
        FORCE_INLINE GMActorPtr GetServiceActor() { return SuperType::_actorArr[0].lock(); }

public :
        typedef std::function<std::string(const rapidjson::Value&)> ModuleOptFuncType;
        void RegistOpt(int64_t module, int64_t opt, const ModuleOptFuncType& cb)
        {
                auto key = std::make_pair(module, opt);
                _moduleOptList.emplace(key, std::move(cb));
        }

        std::map<std::pair<int64_t, int64_t>, ModuleOptFuncType> _moduleOptList;

DECLARE_SERVICE_BASE_END(GM);

#endif

#ifdef GM_SERVICE_SERVER
typedef GMServiceBase<E_ServiceType_Server, stLobbyServerInfo> GMService;
#endif

// vim: fenc=utf8:expandtab:ts=8
