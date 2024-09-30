#include "PayService.h"

#ifdef PAY_SERVICE_SERVER

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

SPECIAL_ACTOR_MAIL_HANDLE(PayActor, 0xf, stMailHttpReq)
{
        auto targetList = Tokenizer(msg->_httpReq->_req.target(), "?");
        if (!targetList.empty())
        {
                auto it = PayService::GetInstance()->_callbackFuncTypeList.find(targetList[0]);
                if (PayService::GetInstance()->_callbackFuncTypeList.end() != it)
                        it->second(shared_from_this(), msg);
        }
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
                        >> cfg->_pay
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

                LOG_FATAL_IF(!_payCfgListByPay.Add(cfg->_pay, cfg),
                             "文件[{}] 唯一标识 pay[{}] 重复!!!",
                             fileName, cfg->_pay);
        }

        return true;
}


template <>
bool PayService::Init()
{
        if (!SuperType::Init())
                return false;

#if defined (PAY_SERVICE_SERVER) || defined (PAY_SERVICE_LOCAL)
        SnowflakePayOrderGuid::Init(GetApp()->GetSID());
#endif

#ifndef PAY_SERVICE_CLIENT
        auto payServerInfo = ServerListCfgMgr::GetInstance()->GetFirst<stPayServerInfo>();
        if constexpr (E_ServiceType_Server == ServiceType)
                payServerInfo = GetApp()->GetServerInfo<stPayServerInfo>();

        ::nl::net::NetMgr::GetInstance()->HttpListen(payServerInfo->_web_port, [](auto&& req) {
                boost::system::error_code ec;
                boost::asio::ip::tcp::endpoint remoteEndPoint = req->_socket->remote_endpoint(ec);

                auto it = GlobalSetup_CH::GetInstance()->_payIPLimitList.find(remoteEndPoint.address().to_string());
                if (GlobalSetup_CH::GetInstance()->_payIPLimitList.end() != it)
                {
                        auto m = std::make_shared<stMailHttpReq>();
                        m->_httpReq = std::move(req);
                        auto act = PayService::GetInstance()->GetServiceActor();
                        if (act)
                                act->SendPush(0xf, m);
                }
                else
                {
                        LOG_WARN("支付回调 remote[{}:{}] IP 未在允许列表中!!!"
                                 , remoteEndPoint.address().to_string(), remoteEndPoint.port());
                        req->Reply("{ \"code\":210, \"msg\":\"IP限制\", \"data\":[] }");
                }
        });

        _callbackFuncTypeList.emplace("/pay", [](const PayActorPtr& act, const std::shared_ptr<stMailHttpReq>& msg) {

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
                                        reqShipMsg->set_order_amt(p.value);
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
                                                httpMsg = "参数错误";
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
                        {
                                httpMsg = "参数错误";
                                break;
                        }

                        LOG_INFO("5555555");
                        auto agent = PayService::GetInstance()->GetActor(act, reqShipMsg->player_guid());
                        if (!agent)
                        {
                                code = 202;
                                httpMsg = "玩家不在线";
                                break;
                        }

                        LOG_INFO("6666666");
                        auto reqShipRet = Call(MsgPayReqShip, act, agent, E_MCMT_Pay, E_MCPST_ReqShip, reqShipMsg);
                        if (!reqShipRet)
                        {
                                code = 202;
                                httpMsg = "玩家不在线";
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
                std::string replyStr = fmt::format("{}\"code\":{}, \"msg\":\"{}\", \"data\":[]{}", "{", code, httpMsg, "}");
                msg->_httpReq->Reply(replyStr);
                return nullptr;
        });

        _callbackFuncTypeList.emplace("/gift", [](const PayActorPtr& act, const std::shared_ptr<stMailHttpReq>& msg) {

                LOG_INFO("target[{}]", msg->_httpReq->_req.target());
                LOG_INFO("body[{}]", msg->_httpReq->_req.body());

                int64_t code = 200;
                std::string httpMsg;

                do
                {
                        auto reqMsg = std::make_shared<MsgSDKGift>();
                        boost::urls::url_view u(msg->_httpReq->_req.target());

                        auto it = u.params().find("userid");
                        if (u.params().end() == it)
                        {
                                code = 201;
                                httpMsg = "参数错误";
                                break;
                        }
                        reqMsg->set_userid(std::stoll((*it).value));

                        it = u.params().find("game_id");
                        if (u.params().end() == it)
                        {
                                code = 201;
                                httpMsg = "参数错误";
                                break;
                        }
                        reqMsg->set_game_id(std::stoll((*it).value));

                        it = u.params().find("server_id");
                        if (u.params().end() == it)
                        {
                                code = 201;
                                httpMsg = "参数错误";
                                break;
                        }
                        reqMsg->set_server_id((*it).value);

                        it = u.params().find("cp_role_id");
                        if (u.params().end() == it)
                        {
                                code = 201;
                                httpMsg = "参数错误";
                                break;
                        }
                        reqMsg->set_player_guid(std::stoll((*it).value));

                        it = u.params().find("product_id");
                        if (u.params().end() == it)
                        {
                                code = 201;
                                httpMsg = "参数错误";
                                break;
                        }
                        reqMsg->set_product_id((*it).value);

                        it = u.params().find("timestamp");
                        if (u.params().end() == it)
                        {
                                code = 201;
                                httpMsg = "参数错误";
                                break;
                        }
                        reqMsg->set_timestamp(std::stoll((*it).value));

                        it = u.params().find("sign");
                        if (u.params().end() == it)
                        {
                                code = 201;
                                httpMsg = "参数错误";
                                break;
                        }
                        reqMsg->set_sign((*it).value);

                        LOG_INFO("5555555");
                        auto agent = PayService::GetInstance()->GetActor(act, reqMsg->player_guid());
                        if (!agent)
                        {
                                code = 202;
                                httpMsg = "玩家不在线";
                                break;
                        }

                        LOG_INFO("6666666");
                        auto reqRet = Call(MsgSDKGift, act, agent, E_MCMT_Pay, E_MCPST_Gift, reqMsg);
                        if (!reqRet)
                        {
                                code = 202;
                                httpMsg = "玩家不在线";
                                break;
                        }

                        LOG_INFO("7777777");
                        if (E_CET_Success != reqRet->error_type())
                        {
                                code = reqRet->code();
                                httpMsg = reqRet->msg();
                        }
                } while (0);

                LOG_INFO("8888888");
                std::string replyStr = fmt::format("{}\"code\":{}, \"msg\":\"{}\", \"data\":[]{}", "{", code, httpMsg, "}");
                msg->_httpReq->Reply(replyStr);
                return nullptr;
        });
#endif

        return ReadPayConfig();
}

#if defined (PAY_SERVICE_CLIENT) || defined (PAY_SERVICE_LOCAL)

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

ACTOR_MAIL_HANDLE(Player, E_MCMT_Pay, E_MCPST_ReqOrderGuid, MsgPayOrderGuid)
{
        auto ret = PayService::GetInstance()->ReqOrderGuid(shared_from_this());
        if (ret)
        {
                SetAttr<E_PAT_OrderGuid>(ret->guid());
                ret->set_error_type(E_CET_Success);
                LOG_INFO("1111111111 order guid:{}", ret->guid());

                auto logGuid = LogService::GetInstance()->GenGuid();
                std::string str = fmt::format("{}\"guid\":{},\"cfg_id\":{}{}", "{", ret->guid(), msg->cfg_id(), "}");
                LogService::GetInstance()->Log<E_LSLMT_Content>(GetID(), str, E_LSLST_Pay, E_MCPST_ReqOrderGuid, E_LSOT_PayReqOrderGuid, logGuid);
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
                        pb->set_code(211);
                        pb->set_msg("支付配置未找到!!!");
                        break;
                }

                std::string price = fmt::format("{:.2f}", payCfg->_price / 10000.0);
                LOG_INFO("22222222222 order_amt[{}] orderAmt[{}] payCfg->_price[{}]"
                         , pb->order_amt(), price, payCfg->_price);
                if (pb->order_amt() != price)
                {
                        pb->set_error_type(E_CET_Fail);
                        pb->set_code(212);
                        pb->set_msg("支付金额错误!!!");
                        break;
                }

                auto sendMsg = std::make_shared<MsgPayClientShip>();
                sendMsg->set_cfg_id(pb->cfg_id());
                auto playerChange = sendMsg->mutable_player_change();

                auto logGuid = LogService::GetInstance()->GenGuid();
                if (0 != payCfg->_goodsID)
                {
                        std::vector<std::pair<int64_t, int64_t>> rewardList;
                        rewardList.reserve(4);
                        rewardList.emplace_back(payCfg->_goodsID, 1);
                        uint64_t extraDropID = 0;
                        auto firstBuyTime = 0;
                        auto it = _shopMgr._payTimeList.find(payCfg->_id);
                        if (_shopMgr._payTimeList.end() != it)
                                firstBuyTime = it->second;
                        if (0 == firstBuyTime && 0 != payCfg->_firstBuy)
                                extraDropID = payCfg->_firstBuy;
                        else
                                extraDropID = payCfg->_extraReward;

                        if (0 != extraDropID)
                                rewardList.emplace_back(extraDropID, 1);

                        auto dropRet = BagMgr::GetInstance()->DoDrop(shared_from_this(), *playerChange, rewardList, E_LSOT_PayReqShip, logGuid);
                        if (E_CET_Success != dropRet)
                        {
                                pb->set_error_type(E_CET_Fail);
                                break;
                        }

                        if (0 == firstBuyTime)
                                _shopMgr._payTimeList.emplace(payCfg->_id, GetClock().GetTimeStamp());
                }

                _activityMgr.OnEvent(*playerChange, shared_from_this(), E_AET_Recharge, payCfg->_price, payCfg->_id, E_LSOT_PayReqShip, logGuid);

                pb->set_error_type(E_CET_Success);
                SetAttr<E_PAT_OrderGuid>(0);
                
                _shopMgr.PackPayTimeList(*playerChange);
                sendMsg->set_error_type(E_CET_Success);
                Send2Client(E_MCMT_Pay, E_MCPST_ClientShip, sendMsg);

                std::string str = fmt::format("{}\"orderno\":\"{}\",\"orderno_cp\":\"{}\",\"userid\":{},\"game_id\":{},\"order_amt\":{},\"pay_amt\":{},\"pay_time\":{},\"player_guid\":{},\"cfg_id\":{},\"reward\":{},\"sign\":\"{}\"{}"
                                              , "{", pb->orderno(), pb->orderno_cp(), pb->userid(), pb->game_id(), pb->order_amt(), pb->pay_amt(), pb->pay_time(), pb->player_guid(), pb->cfg_id(), pb->reward(), pb->sign(), "}");
                LogService::GetInstance()->Log<E_LSLMT_Content>(GetID(), str, E_LSLST_Pay, E_MCPST_ReqShip, E_LSOT_PayReqShip, logGuid);
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

ACTOR_MAIL_HANDLE(Player, E_MCMT_Pay, E_MCPST_Gift, PayService::SessionType::stServiceMessageWapper)
{
        auto pb = std::dynamic_pointer_cast<MsgSDKGift>(msg->_pb);
        if (!pb)
                return nullptr;

        do
        {
                auto payCfg = PayService::GetInstance()->_payCfgListByPay.Get(pb->product_id());
                if (!payCfg)
                {
                        pb->set_error_type(E_CET_Fail);
                        break;
                }

                auto mailCfg = MailSys::GetInstance()->_cfgList.Get(1216000001);
                if (!mailCfg)
                {
                        pb->set_error_type(E_CET_Fail);
                        break;
                }

                std::map<int64_t, std::pair<int64_t, int64_t>> rewardList;
                auto errorType = BagMgr::GetInstance()->DoDropInternal({{ payCfg->_goodsID, 1 }}, rewardList, 0);
                if (E_CET_Success != errorType)
                {
                        pb->set_error_type(E_CET_Fail);
                        break;
                }

                std::vector<stMailGoodsInfoPtr> goodsList;
                goodsList.reserve(rewardList.size());
                for (auto& val : rewardList)
                {
                        LOG_INFO("Payyyyyyyyyyyyyyyyyyyyyy type[{}] id[{}] cnt[{}]", val.second.second, val.first, val.second.first);
                        goodsList.emplace_back(std::make_shared<stMailGoodsInfo>(val.second.second, val.first, val.second.first));
                }

                _mailSys.AddMail(mailCfg->_title, mailCfg->_from, mailCfg->_content, goodsList);

                pb->set_error_type(E_CET_Success);
                auto logGuid = LogService::GetInstance()->GenGuid();
                std::string str = fmt::format("{}\"userid\":{},\"game_id\":{},\"server_id\":{},\"cp_role_id\":{},\"product_id\":{},\"timestamp\":{},\"sign\":{}{}"
                                              , "{", pb->userid(), pb->game_id(), pb->server_id(), pb->player_guid(), pb->product_id(), pb->timestamp(), pb->sign(), "}");
                LogService::GetInstance()->Log<E_LSLMT_Content>(GetID(), str, E_LSLST_Pay, E_MCPST_Gift, E_LSOT_PayGift, logGuid);
        } while (0);

        return pb;
}

SERVICE_NET_HANDLE(PayService::SessionType, E_MCMT_Pay, E_MCPST_Gift, MsgSDKGift)
{
        auto p = std::dynamic_pointer_cast<Player>(PlayerMgr::GetInstance()->GetActor(msg->player_guid()));
        if (p)
        {
                auto m = std::make_shared<PayService::SessionType::stServiceMessageWapper>();
                m->_agent = std::make_shared<typename PayService::SessionType::ActorAgentType>(msgHead._from, shared_from_this());
                m->_agent->BindActor(p);
                AddAgent(m->_agent);

                m->_pb = msg;
                p->CallPushInternal(m->_agent, E_MCMT_Pay, E_MCPST_Gift, m, msgHead._guid);
        }
}

#endif
