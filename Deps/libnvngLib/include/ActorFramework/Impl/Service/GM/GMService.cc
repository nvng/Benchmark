#include "GMService.h"

#ifdef GM_SERVICE_SERVER

SPECIAL_ACTOR_MAIL_HANDLE(GMActor, 0x0, stMailHttpReq)
{
        std::string retStr;
        do
        {
                boost::urls::url_view u(msg->_httpReq->_req.target());
                auto it = u.params().find("module");
                if (u.params().end() == it)
                        break;
                auto module = std::stoll((*it).value);

                it = u.params().find("opt");
                if (u.params().end() == it)
                        break;
                auto opt = std::stoll((*it).value);

                rapidjson::Document root;
                if (root.Parse(msg->_httpReq->_req.body().data()).HasParseError())
                        break;

                /*
                LOG_INFO("11111111111111 {}", (*it).value);
                LOG_INFO("11111111111111 {}", (*it).key);
                */
                auto key = std::make_pair(module, opt);
                auto itOpt = GMService::GetInstance()->_moduleOptList.find(key);
                if (GMService::GetInstance()->_moduleOptList.end() != itOpt)
                {
                        retStr = itOpt->second(root);
                }

        } while (0);

        msg->_httpReq->Reply(retStr);
        return nullptr;
}

template <>
bool GMService::Init()
{
        if (!SuperType::Init())
                return false;

        RegistOpt(99999999, 0, [](const rapidjson::Value&) {
                return "{ \"time\": " + fmt::format("{}", GetClock().GetTimeStamp()) + " }";
        });

        auto gmServerInfo = ServerListCfgMgr::GetInstance()->GetFirst<stGMServerInfo>();
        if constexpr (E_ServiceType_Server == ServiceType)
                gmServerInfo = GetApp()->GetServerInfo<stGMServerInfo>();

        ::nl::net::NetMgr::GetInstance()->HttpListen(gmServerInfo->_http_gm_port, [](auto&& req) {
                auto m = std::make_shared<stMailHttpReq>();
                m->_httpReq = std::move(req);
                auto act = GMService::GetInstance()->GetServiceActor();
                if (act)
                        act->SendPush(0x0, m);
        });

        return true;
}

#endif
