#include "PayService.h"

#ifdef CDKEY_SERVICE_SERVER

SERVICE_NET_HANDLE(PayService::SessionType, E_MCMT_Pay, E_MCPST_ReqOrderGuid)
{
        LOG_INFO("111111111111111111111111111");
        auto ret = std::make_shared<MsgPayOrderGuid>();
        ret->set_guid(PayService::SnowflakePayOrderGuid::Gen());
        SendPB(ret,
               E_MCMT_Pay,
               E_MCPST_ReqOrderGuid,
               MsgHeaderType::E_RMT_CallRet,
               msgHead._guid,
               msgHead._to,
               msgHead._from);
}

SPECIAL_ACTOR_MAIL_HANDLE(PayActor, E_MCPST_ReqShip, stMailHttpReq)
{
        LOG_INFO("target[{}]", msg->_httpReq->_req.target());
        LOG_INFO("body[{}]", msg->_httpReq->_req.body());

        int64_t code = 200;
        std::string httpMsg;

        do
        {
                auto reqShipMsg = std::make_shared<MsgPayReqShip>();
                boost::urls::url_view u(msg->_httpReq->_req.target());
                for (auto p : u.params())
                {
                        if (p.key == "orderno")
                                reqShipMsg->set_orderno(p.value);
                        if (p.key == "orderno_cp")
                                reqShipMsg->set_orderno_cp(p.value);
                        if (p.key == "userid")
                                reqShipMsg->set_userid(atoll(p.value.c_str()));
                        if (p.key == "game_id")
                                reqShipMsg->set_game_id(atoll(p.value.c_str()));
                        if (p.key == "order_amt")
                                reqShipMsg->set_order_amt(atoll(p.value.c_str()));
                        if (p.key == "pay_amt")
                                reqShipMsg->set_pay_amt(atoll(p.value.c_str()));
                        if (p.key == "pay_time")
                                reqShipMsg->set_pay_time(atoll(p.value.c_str()));
                        if (p.key == "extra")
                        {
                                auto extraArr = Tokenizer<uint64_t>(p.value, "_");
                                if (2 != extraArr.size())
                                {
                                        code = 202;
                                        break;
                                }

                                reqShipMsg->set_player_guid(extraArr[0]);
                                reqShipMsg->set_cfg_id(extraArr[1]);
                        }
                        if (p.key == "reward")
                                reqShipMsg->set_reward(atoll(p.value.c_str()));
                        if (p.key == "sign")
                                reqShipMsg->set_sign(p.value);
                }

                /*
                auto paramList = u.params();
                auto it = paramList.find("order_amt");
                if (paramList.end() != it)
                        LOG_INFO("2222222 {}", (*it).value);
                */

                if (200 != code)
                        break;

                LOG_INFO("5555555");
                auto thisPtr = shared_from_this();
                auto agent = PayService::GetInstance()->GetActor(thisPtr, reqShipMsg->player_guid());
                if (!agent)
                {
                        code = 202;
                        break;
                }

                LOG_INFO("6666666");
                auto reqShipRet = Call(MsgPayReqShip, agent, E_MCMT_Pay, E_MCPST_ReqShip, reqShipMsg);
                if (!reqShipRet)
                {
                        code = 202;
                        break;
                }

                LOG_INFO("7777777");
                if (E_CET_Success != reqShipRet->error_type())
                {
                        code = reqShipRet->code();
                        httpMsg = reqShipRet->msg();
                }
        } while (0);

        LOG_INFO("8888888");
        std::string replyStr = "{";
        replyStr += fmt::format("\"code\":{}, \"msg\":\"{}\", \"data\":[]", code, httpMsg);
        replyStr += "}";
        msg->_httpReq->Reply(replyStr);
        return nullptr;
}

#endif

template <>
bool PayService::ReadPayConfig()
{
        std::string fileName = ServerCfgMgr::GetInstance()->GetConfigAbsolute("Pay.txt");
        std::stringstream ss;
        if (!ReadFileToSS(ss, fileName))
                return false;

        int64_t idx = 0;
        std::string tmpStr;
        _payCfgList.Clear();
        while (ReadTo(ss, "#"))
        {
                if (++idx <= 3)
                        continue;

                auto cfg = std::make_shared<stPayCfgInfo>();
                ss >> cfg->_id
                        >> tmpStr
                        >> tmpStr
                        >> tmpStr
                        >> cfg->_goodsType
                        >> tmpStr
                        >> cfg->_goodsID
                        >> cfg->_firstBuy
                        >> cfg->_extraReward
                        >> cfg->_price
                        ;

                LOG_FATAL_IF(!CheckConfigEnd(ss, fileName),
                             "文件[{}] 读取错位，id[{}] 没检测到结束符 <end> !!!",
                             fileName, cfg->_id);

                LOG_FATAL_IF(!_payCfgList.Add(cfg->_id, cfg),
                             "文件[{}] 唯一标识 id[{}] 重复!!!",
                             fileName, cfg->_id);
        }

        return true;
}


template <>
bool PayService::Init()
{
        if (!SuperType::Init())
                return false;

#if defined (CDKEY_SERVICE_SERVER) || defined (CDKEY_SERVICE_LOCAL)
        SnowflakePayOrderGuid::Init(GetApp()->GetSID());
#endif

#ifndef CDKEY_SERVICE_CLIENT
        auto payServerInfo = ServerListCfgMgr::GetInstance()->GetFirst<stPayServerInfo>();
        if constexpr (E_ServiceType_Server == ServiceType)
                payServerInfo = GetApp()->GetServerInfo<stPayServerInfo>();

        ::nl::net::NetMgr::GetInstance()->HttpListen(payServerInfo->_web_port, [](auto&& req) {
                auto m = std::make_shared<stMailHttpReq>();
                m->_httpReq = std::move(req);
                auto act = PayService::GetInstance()->GetServiceActor();
                if (act)
                        act->SendPush(E_MCPST_ReqShip, m);
        });
#endif

        return ReadPayConfig();
}

#if defined (CDKEY_SERVICE_CLIENT) || defined (CDKEY_SERVICE_LOCAL)

template <> std::shared_ptr<MsgPayOrderGuid>
PayService::ReqOrderGuid(const PlayerPtr& act)
{
        if constexpr (E_ServiceType_Client == ServiceType)
        {
                auto payActor = GetActor(act, act->GetID());
                if (!payActor)
                        return nullptr;

                LOG_INFO("222222222222222222222222");
                return Call(MsgPayOrderGuid, act, payActor, E_MCMT_Pay, E_MCPST_ReqOrderGuid, nullptr);
        }
        else if constexpr (E_ServiceType_Local == ServiceType)
        {
                auto ret = std::make_shared<MsgPayOrderGuid>();
                ret->set_guid(SnowflakePayOrderGuid::Gen());
                return ret;
        }
        else
        {
                return nullptr;
        }
}

ACTOR_MAIL_HANDLE(Player, E_MCMT_Pay, E_MCPST_ReqOrderGuid)
{
        auto ret = PayService::GetInstance()->ReqOrderGuid(shared_from_this());
        if (ret)
        {
                SetAttr<E_PAT_OrderGuid>(ret->guid());
                ret->set_error_type(E_CET_Success);
                LOG_INFO("1111111111 order guid:{}", ret->guid());
        }
        return ret;
}

ACTOR_MAIL_HANDLE(Player, E_MCMT_Pay, E_MCPST_ReqShip, PayService::SessionType::stServiceMessageWapper)
{
        auto pb = std::dynamic_pointer_cast<MsgPayReqShip>(msg->_pb);
        if (!pb)
                return nullptr;

        LOG_INFO("111111111111111111111111111 cfg:{}", pb->cfg_id());
        do
        {
                if (0 == GetAttr<E_PAT_OrderGuid>())
                {
                        pb->set_error_type(E_CET_Success);
                        break;
                }

                auto payCfg = PayService::GetInstance()->_payCfgList.Get(pb->cfg_id());
                if (!payCfg)
                {
                        pb->set_error_type(E_CET_Fail);
                        break;
                }

                std::vector<std::pair<int64_t, int64_t>> rewardList;
                rewardList.reserve(4);
                rewardList.emplace_back(payCfg->_goodsID, 1);
                uint64_t extraDropID = 0;
                const auto firstBuyTime = GetAttr<E_PAT_FirstBuyTime>();
                if (0 == firstBuyTime && 0 != payCfg->_firstBuy)
                        extraDropID = payCfg->_firstBuy;
                else
                        extraDropID = payCfg->_extraReward;

                auto sendMsg = std::make_shared<MsgPayClientShip>();
                sendMsg->set_cfg_id(pb->cfg_id());
                auto playerChange = sendMsg->mutable_player_change();
                _activityMgr.OnEvent(*playerChange, shared_from_this(), 0 == firstBuyTime ? E_AET_FirstRecharge : E_AET_Recharge, pb->order_amt() * 10000, 0);

                if (0 != extraDropID)
                        rewardList.emplace_back(extraDropID, 1);

                auto dropRet = BagMgr::GetInstance()->DoDrop(shared_from_this(), *playerChange, rewardList);
                if (E_CET_Success != dropRet)
                {
                        pb->set_error_type(E_CET_Fail);
                        break;
                }

                pb->set_error_type(E_CET_Success);
                if (0 == firstBuyTime)
                        SetAttr<E_PAT_FirstBuyTime>(GetClock().GetTimeStamp());
                SetAttr<E_PAT_OrderGuid>(0);
                
                sendMsg->set_error_type(E_CET_Success);
                Send2Client(E_MCMT_Pay, E_MCPST_ClientShip, sendMsg);
        } while (0);

        return pb;
}

SERVICE_NET_HANDLE(PayService::SessionType, E_MCMT_Pay, E_MCPST_ReqShip, MsgPayReqShip)
{
        auto p = std::dynamic_pointer_cast<Player>(PlayerMgr::GetInstance()->GetActor(msg->player_guid()));
        if (p)
        {
                auto m = std::make_shared<PayService::SessionType::stServiceMessageWapper>();
                m->_agent = std::make_shared<typename PayService::SessionType::ActorAgentType>(msgHead._from, shared_from_this());
                m->_agent->BindActor(p);
                AddAgent(m->_agent);

                m->_pb = msg;
                p->CallPushInternal(m->_agent, E_MCMT_Pay, E_MCPST_ReqShip, m, msgHead._guid);
        }
}

#endif
