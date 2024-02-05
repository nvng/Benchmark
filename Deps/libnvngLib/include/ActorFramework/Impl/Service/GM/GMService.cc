#include "GMService.h"

#if defined(GM_SERVICE_SERVER) || defined(GM_SERVICE_CLIENT)

bool GMActor::Init()
{
        if (!SuperType::Init())
                return false;

#ifdef GM_SERVICE_SERVER
        std::string sqlStr = fmt::format("CREATE TABLE IF NOT EXISTS `gm_data` ( \
                                         `module` bigint unsigned NULL, \
                                         `id` bigint unsigned NOT NULL, \
                                         `state` bigint unsigned NULL COMMENT '1 正常状态  2 已停止', \
                                         `data` longtext CHARACTER SET utf8 COLLATE utf8_general_ci NULL, \
                                         INDEX `idx_module_id`(`module`, `id`) USING BTREE, \
                                         INDEX `idx_module_state`(`module`, `state`) USING BTREE \
                                        ) ENGINE = InnoDB CHARACTER SET = utf8 COLLATE = utf8_general_ci ROW_FORMAT = Dynamic;");
        MySqlMgr::GetInstance()->Exec(sqlStr);
#endif
        return true;
}

#ifdef GM_SERVICE_SERVER

void GMActor::Add2DB(uint64_t module, uint64_t guid, const std::string& data)
{
        std::string sqlStr = fmt::format("INSERT INTO gm_data(module, id, state, data) VALUES({}, {}, {}, \'{}\');", module, guid, 1, Base64Encode(data));
        MySqlMgr::GetInstance()->Exec(sqlStr);
}

void GMActor::UpdateState(uint64_t module, uint64_t guid, uint64_t state)
{
        std::string sqlStr = fmt::format("UPDATE gm_data SET state={} WHERE module={} AND id={};", state, module, guid);
        MySqlMgr::GetInstance()->Exec(sqlStr);
}

std::string GMActor::GetData(uint64_t module, uint64_t state/* = UINT64_MAX*/)
{
        std::string sqlStr = fmt::format("SELECT state, id, data FROM gm_data WHERE module={}", module);
        sqlStr += fmt::format("{}", UINT64_MAX == state ? ";" : fmt::format(" AND state={};", state));
        auto result = MySqlMgr::GetInstance()->Exec(sqlStr);
        if (result->rows().empty())
        {
                return "[]";
        }
        else
        {
                std::string itemListStr;
                for (auto row : result->rows())
                        itemListStr += fmt::format("{}\"state\":{},\"guid\":{},\"data\":{}{}"
                                                   , "{", row.at(0).as_uint64(), row.at(1).as_uint64(), Base64Decode(row.at(2).as_string()), "},");
                if (!itemListStr.empty())
                        itemListStr.pop_back();
                return fmt::format("[{}]", itemListStr);
        }
}

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
                
                it = u.params().find("guid");
                if (u.params().end() == it)
                        break;
                auto guid = std::stoll((*it).value);

                auto body = msg->_httpReq->_req.body();

                auto mailGMOpt = std::make_shared<MailGMOpt>();
                mailGMOpt->set_module(module);
                mailGMOpt->set_opt(opt);
                mailGMOpt->set_guid(guid);
                mailGMOpt->set_body(body);

                bool success = false;
                auto optKey = std::make_pair(module, opt);
                auto it_ = GMService::GetInstance()->_moduleOptList.find(optKey);
                if (GMService::GetInstance()->_moduleOptList.end() != it_)
                {
                        std::tie(success, retStr) = it_->second(shared_from_this(), mailGMOpt);
                }
                else
                {
                        if (2 == opt) // 查找。
                        {
                                it = u.params().find("state");
                                if (u.params().end() == it)
                                        retStr = GetData(module);
                                else
                                        retStr = GetData(module, std::stoll((*it).value));
                        }
                        else 
                        {
                                int64_t cnt = 0;
                                std::vector<std::shared_ptr<GMService::SessionType>> successSesList;
                                GMService::GetInstance()->ForeachSession([this, mailGMOpt, &successSesList, &cnt](const auto& ses) {
                                        ++cnt;
                                        auto agent = std::make_shared<GMService::SessionType::ActorAgentType>(0, ses);
                                        agent->BindActor(shared_from_this());
                                        ses->AddAgent(agent);
                                        auto callRet = Call(MailGMOpt, agent, E_MIMT_GM, E_MIGMST_Sync, mailGMOpt);
                                        if (!callRet || E_IET_Success != callRet->error_type())
                                        {
                                                LOG_WARN("GM 操作出错 error_type[{}]，module[{}] opt[{}] body[{}]"
                                                         , callRet ? callRet->error_type() : E_IET_CallTimeOut
                                                         , mailGMOpt->module(), mailGMOpt->opt(), mailGMOpt->body());
                                        }
                                        else
                                        {
                                                successSesList.emplace_back(ses);
                                        }
                                });

                                if (successSesList.size() >= cnt)
                                {
                                        success = true;
                                        retStr = "{\"error_code\":200}";
                                }
                                else
                                {
                                        // TODO : 恢复。
                                        success = false;
                                        retStr = "{\"error_code\":201}";
                                }
                        }
                }

                if (success && 99999999 != module)
                {
                        switch (opt)
                        {
                        case 0 : Add2DB(module, guid, body); break;
                        case 1 : UpdateState(module, guid, 2); break;
                        default : break;
                        }
                }

        } while (0);

        msg->_httpReq->Reply(retStr);
        return nullptr;
}

#endif

template <>
bool GMService::Init()
{
        if (!SuperType::Init())
                return false;

#ifdef GM_SERVICE_SERVER
        SnowflakeGMGuid::Init(GetApp()->GetSID());
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

        RegistOpt(99999999, 0, [](const GMActorPtr& gmActor, const auto& msg) -> std::pair<bool, std::string> {
                return { true, "{ \"time\": " + fmt::format("{}", GetClock().GetTimeStamp()) + " }" };
        });
        RegistOpt(99999999, 1, [](const GMActorPtr& gmActor, const auto& msg) -> std::pair<bool, std::string> {
                return { true, "{ \"guid\": " + fmt::format("{}", SnowflakeGMGuid::Gen()) + " }" };
        });

#endif

        return true;
}

#ifdef GM_SERVICE_CLIENT

SPECIAL_ACTOR_MAIL_HANDLE(GMActor, E_MIGMST_Sync, GMService::SessionType::stServiceMessageWapper)
{
        auto pb = std::dynamic_pointer_cast<MailGMOpt>(msg->_pb);
        do
        {
                if (!pb)
                {
                        pb->set_error_type(E_IET_Fail);
                        break;
                }

                auto key = std::make_pair(pb->module(), pb->opt());
                auto it = GMService::GetInstance()->_moduleOptList.find(key);
                if (GMService::GetInstance()->_moduleOptList.end() != it)
                {
                        auto [ret, str] = it->second(shared_from_this(), pb);
                        pb->set_error_type(ret ? E_IET_Success : E_IET_Fail);
                        break;
                }

                pb->set_error_type(E_IET_Success);
        } while (0);

        auto ses = msg->_agent->GetSession();
        if (ses)
                ses->SendPB(pb, E_MIMT_GM, E_MIGMST_Sync, GMService::SessionType::MsgHeaderType::E_RMT_CallRet, msg->_msgHead._guid, msg->_msgHead._to, msg->_msgHead._from);
        return nullptr;
}

SERVICE_NET_HANDLE(GMService::SessionType, E_MIMT_GM, E_MIGMST_Sync, MailGMOpt)
{
        auto act = GMService::GetInstance()->GetServiceActor();
        if (act)
        {
                auto m = std::make_shared<GMService::SessionType::stServiceMessageWapper>();
                m->_agent = std::make_shared<typename GMService::SessionType::ActorAgentType>(msgHead._from, shared_from_this());
                m->_agent->BindActor(act);
                AddAgent(m->_agent);

                m->_msgHead = msgHead;
                m->_pb = msg;
                act->SendPush(m->_agent, E_MIGMST_Sync, m);
        }
}

#endif

#endif
