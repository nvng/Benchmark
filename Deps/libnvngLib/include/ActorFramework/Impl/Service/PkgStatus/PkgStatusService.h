#pragma once

#include "ActorFramework/ServiceExtra.hpp"

#ifdef PKG_STATUS_SERVICE_SERVER

struct stPkgStatusInfo
{
        std::string _version;
        int64_t _status = 0;

        std::string _reqRetStr;
};
typedef std::shared_ptr<stPkgStatusInfo> stPkgStatusInfoPtr;

SPECIAL_ACTOR_DEFINE_BEGIN(PkgStatusActor);

public :
        bool Init() override;
        void InitLoadTimer();

        ::nl::util::SteadyTimer _timer;

SPECIAL_ACTOR_DEFINE_END(PkgStatusActor);

DECLARE_SERVICE_BASE_BEGIN(PkgStatus, SessionDistributeNull, ServiceSession);

private :
        PkgStatusServiceBase() : SuperType("PkgStatusService") { }
        ~PkgStatusServiceBase() override { }

public :
        template <typename ... Args>
        bool StartServer(uint16_t port, Args ... args)
        {
                if (SuperType::_actorArr.empty())
                        ThisType::StartLocal(1, args...);

                auto serverInfo = GetApp()->GetServerInfo<stPkgStatusServerInfo>();
                ::nl::net::NetMgr::GetInstance()->HttpListen(serverInfo->_http_port, [](auto&& req) {
                        boost::urls::url_view u(req->_req.target());
                        auto paramList = u.params();
                        auto it = paramList.find("v");
                        if (paramList.end() == it)
                                return;

                        auto info = ThisType::GetInstance()->_statusList->Get((*it).value);
                        if (!info)
                                return;

                        req->Reply(info->_reqRetStr);
                });

                return true;
        }

public :
        std::shared_ptr<ThreadSafeUnorderedMap<std::string, stPkgStatusInfoPtr>> _statusList;

DECLARE_SERVICE_BASE_END(PkgStatus);

#endif

#ifdef PKG_STATUS_SERVICE_SERVER
typedef PkgStatusServiceBase<E_ServiceType_Server, stLobbyServerInfo> PkgStatusService;
#endif

// vim: fenc=utf8:expandtab:ts=8
