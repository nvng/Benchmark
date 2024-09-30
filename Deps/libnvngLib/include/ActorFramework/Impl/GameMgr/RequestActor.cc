#include "RequestActor.h"

#include "GameMgrLobbySession.h"

RequestActor::RequestActor(uint64_t id)
        : SuperType(id, 1 << 10)
{
}

RequestActor::~RequestActor()
{
}

ACTOR_MAIL_HANDLE(RequestActor, E_MIMT_Local, E_MILST_Disconnect)
{
        return nullptr;
}

ACTOR_MAIL_HANDLE(RequestActor, E_MIMT_Local, E_MILST_Reconnect, MailResult)
{
        /*
         * game server 重连上来之后，需要每个 regionAgent 向 game server 发送 Reconnect 以确认 region 是否存在。
         *
         * lobby server 重连上来之后，在 lobby server 未当机情况下，以 lobby server 数据为准。
         * 若 lobby server 当机，则会向 game mgr server 请求 region relation 进行同步。
         *
         * queue 暂时处理方式为，若 game mgr server 当机，则全部清空，因无其它地方保存有如组队信息，
         * 因此只以 game mgr server 上的数据为准。
         */

        auto regionAgent = std::dynamic_pointer_cast<GameMgrGameSession::ActorAgentType>(from);
        if (regionAgent)
        {
                if (E_IET_RemoteCrash == msg->error_type())
                        DelRegionFromRegionMgr(std::dynamic_pointer_cast<GameMgrGameSession::ActorAgentType>(from), nullptr);
                else
                        Send(regionAgent, E_MIMT_Internal, E_MIIST_Reconnect, nullptr);
        }

        auto lobbyAgent = std::dynamic_pointer_cast<GameMgrLobbySession::ActorAgentType>(from);
        if (lobbyAgent)
        {
                auto extra = lobbyAgent->GetExtra();
                auto regionAgent = std::dynamic_pointer_cast<GameMgrGameSession::ActorAgentType>(extra);
                if (regionAgent)
                {
                        if (E_IET_RemoteCrash == msg->error_type())
                        {
                                auto m = std::make_shared<MailPlayerRegionRelationInfo>();
                                m->set_player_guid(lobbyAgent->GetID());
                                m->set_requestor_id(GetID());
                                m->set_region_type(static_cast<ERegionType>(regionAgent->GetType()));

                                auto regionMsg = m->mutable_region_info();
                                regionMsg->set_id(regionAgent->GetID());
                                regionMsg->set_game_sid(regionAgent->GetSID());
                                Send(lobbyAgent, E_MIMT_Internal, E_MIIST_SyncRelation, m);
                        }
                }
                else
                {
                        auto queueAgent = std::dynamic_pointer_cast<stQueueInfo>(extra);
                        if (queueAgent)
                        {
                                if (E_IET_RemoteCrash == msg->error_type())
                                {
                                        auto m = std::make_shared<MailPlayerRegionRelationInfo>();
                                        m->set_player_guid(lobbyAgent->GetID());
                                        m->set_requestor_id(GetID());
                                        m->set_region_type(queueAgent->_regionType);

                                        Send(lobbyAgent, E_MIMT_Internal, E_MIIST_SyncRelation, m);
                                }
                        }
                }
        }
        return nullptr;
}

ACTOR_MAIL_HANDLE(RequestActor, E_MIMT_Internal, E_MIIST_Reconnect)
{
        return nullptr;
}

ACTOR_MAIL_HANDLE(RequestActor, E_MIMT_Internal, E_MIIST_Result, MailAgentNotFoundResult)
{
        LOG_INFO("收到 region not found src type[{:#x}]", msg->type());
        switch (msg->error_type())
        {
        case E_IET_RegionNotFound :
                DelRegionFromRegionMgr(std::dynamic_pointer_cast<GameMgrGameSession::ActorAgentType>(from), nullptr);
                break;
        default :
                break;
        }
        return nullptr;
}

ACTOR_MAIL_HANDLE(RequestActor, E_MCMT_GameCommon, E_MCGCST_ReqEnterRegion, stMailReqEnterRegion)
{
        auto reqPB = std::dynamic_pointer_cast<MailReqEnterRegion>(msg->_msg);
        const auto& fInfo = reqPB->fighter_list(0);
        auto reqAgent = msg->_agent;

        auto retMsg = std::make_shared<MailReqEnterRegionRet>();
        retMsg->set_error_type(E_IET_Fail);
        retMsg->set_game_sid(-1);

        GameMgrGameSession::ActorAgentTypePtr regionAgent;
        do
        {
                regionAgent = std::dynamic_pointer_cast<GameMgrGameSession::ActorAgentType>(reqAgent->GetExtra());
                if (!regionAgent)
                {
                        LOG_WARN("玩家[{}] RequestActor regionAgent is nullptr!!!", fInfo.player_guid());
                        retMsg->set_error_type(E_IET_RegionAgentIsNull);
                        break;
                }

                auto gameSes = regionAgent->GetSession();
                if (!gameSes)
                {
                        LOG_WARN("玩家[{}] RequestActor game ses already release!!!", fInfo.player_guid());
                        retMsg->set_error_type(E_IET_GameSesAlreadyRelease);
                        break;
                }

                if (msg->_new)
                {
                        auto ret = Call(MailResult, regionAgent, E_MIMT_GameCommon, E_MIGCST_RegionCreate, regionAgent->_cfg);
                        if (!ret || E_IET_Success != ret->error_type())
                        {
                                LOG_WARN("玩家[{}] RequestActor 请求创建 region[{}] 时，error_type[{}]!!!",
                                         fInfo.player_guid(), regionAgent->GetID(), ret ? ret->error_type() : E_IET_CallTimeOut);
                                retMsg->set_error_type(E_IET_RegionCreateTimeout);
                                break;
                        }
                }

                reqPB->set_region_id(regionAgent->GetID());
                auto reqRet = Call(MailReqEnterRegionRet, regionAgent, E_MCMT_GameCommon, E_MCGCST_ReqEnterRegion, reqPB);
                if (!reqRet || E_IET_Success != reqRet->error_type())
                {
                        LOG_WARN("玩家[{}] RequestActor 请求 enter region[{}] 时，error_type[{}]!!!",
                                 fInfo.player_guid(), regionAgent->GetID(), reqRet ? reqRet->error_type() : E_IET_CallTimeOut);
                        retMsg->set_error_type(E_IET_ReqEnterRegionTimeout);
                        break;
                }

                retMsg->set_error_type(E_IET_Success);
                retMsg->set_region_id(regionAgent->GetID());
                retMsg->set_game_sid(gameSes->GetSID());
                retMsg->set_error_type(E_IET_Success);
                regionAgent->AddFighter(reqAgent);
        } while (0);

        RegionMgr::GetInstance()->_reqList.Remove(fInfo.player_guid());
        if (E_IET_Success == retMsg->error_type())
        {
                if (reqAgent)
                {
                        auto ses = reqAgent->GetSession();
                        if (ses)
                                ses->SendPB(retMsg,
                                            E_MCMT_GameCommon,
                                            E_MCGCST_ReqEnterRegion,
                                            GameMgrLobbySession::MsgHeaderType::E_RMT_CallRet,
                                            msg->_guid,
                                            0,
                                            reqAgent->GetID());
                }
        }
        else
        {
                RegionDestroy(regionAgent, nullptr);
        }

        return nullptr;
}

// 退出场景由 gameMgr 统一管理。
ACTOR_MAIL_HANDLE(RequestActor, E_MCMT_GameCommon, E_MCGCST_ReqExitRegion, MailReqExit)
{
        auto retMsg = std::make_shared<MailResult>();
        retMsg->set_error_type(E_IET_Success);

        do
        {
                auto reqAgent = std::dynamic_pointer_cast<GameMgrLobbySession::ActorAgentType>(from);
                if (!reqAgent)
                {
#ifndef ____BENCHMARK____
                        LOG_WARN("玩家[{}] 请求退出 region 时，reqAgent is nullptr!!!", msg->player_guid());
#endif
                        break;
                }

                auto region = std::dynamic_pointer_cast<GameMgrGameSession::ActorAgentType>(reqAgent->GetExtra());
                if (!region)
                {
                        LOG_WARN("玩家[{}] 请求退出 region 时，region is nullptr!!!", msg->player_guid());
                        break;
                }

                DLOG_INFO("玩家[{}] 请求退出 region[{}]", msg->player_guid(), region->GetID());
                auto ret = Call(MailResult, region, E_MCMT_GameCommon, E_MCGCST_ReqExitRegion, msg);
                if (!ret || E_IET_Success!=ret->error_type())
                {
                        auto errorType = ret ? ret->error_type() : E_IET_CallTimeOut;
                        LOG_WARN("玩家[{}] 请求退出刚创建的 region[{}] type[{}] 时，game server 上退出失败 error[{}]!!!",
                                  msg->player_guid(), region->GetID(), region->GetType(), errorType);
                        retMsg->set_error_type(errorType);
                        break;
                }

                msg->set_region_id(region->GetID());
                region->RemoveFighter(msg->player_guid());
        } while (0);

        return retMsg;
}

void RequestActor::RegionDestroy(const GameMgrGameSession::ActorAgentTypePtr& regionAgent, const std::shared_ptr<MailRegionDestroyInfo>& msg)
{
        if (!regionAgent)
        {
                LOG_ERROR("RegionDestroy regionAgent is nullptr!!! id[{}]", GetID());
                return;
        }

        // 保证 game mgr 上删除的 region，game server 上肯定已删除。
        auto ret = Call(MailResult, regionAgent, E_MIMT_GameCommon, E_MIGCST_RegionDestroy, nullptr);
        if (!ret || E_IET_Success != ret->error_type())
        {
                DLOG_INFO("RequestActor regionAgent release error[{}]!!!",
                          ret ? ret->error_type() : E_IET_CallTimeOut);
                return;
        }

        DelRegionFromRegionMgr(regionAgent, msg);
        return;
}

void RequestActor::DelRegionFromRegionMgr(const GameMgrGameSession::ActorAgentTypePtr& regionAgent,
                                          const std::shared_ptr<MailRegionDestroyInfo>& msg)
{
        if (!regionAgent)
        {
                LOG_ERROR("DelRegionFromRegionMgr regionAgent is nullptr!!! id[{}]", GetID());
                return;
        }

        // 从 regionMgr 中肯定能够删除。
        std::shared_ptr<MailRegionDestroyInfo> destroyMsg = msg;
        if (!msg)
        {
                destroyMsg = std::make_shared<MailRegionDestroyInfo>();
                destroyMsg->set_region_id(regionAgent->GetID());
        }
        auto ret = Call(MailResult, regionAgent->_regionMgr, scRegionMgrActorMailMainType, 0x1, destroyMsg);
        if (!ret || E_IET_Success != ret->error_type())
        {
                DLOG_INFO("RequestActor regionMgrActor release ret[{}] error[{}]!!!",
                          (void*)ret.get(), ret?ret->error_type():E_IET_CallTimeOut);
                return;
        }

        regionAgent->ForeachFighter([this, &regionAgent](const IActorPtr& f) {
                auto m = std::make_shared<MailRegionDestroy>();
                m->set_region_id(regionAgent->GetID());
                m->set_player_guid(f->GetID());
                Send(f, E_MIMT_GameCommon, E_MIGCST_RegionDestroy, m);
        });

        if (0 != regionAgent->_queueGuid)
        {
                auto mail = std::make_shared<MailInt>();
                mail->set_val(regionAgent->_queueGuid);
                Send(RegionMgr::GetInstance()->GetQueueMgrActor(regionAgent->_cfg->region_type(), regionAgent->_queueType),
                     scQueueMgrActorMailMainType, 0xf, mail );
        }
        return;
}

ACTOR_MAIL_HANDLE(RequestActor, E_MIMT_GameCommon, E_MIGCST_RegionDestroy, MailRegionDestroyInfo)
{
        if (from)
        {
                DLOG_INFO("RequestActor 收到场景请求 release 消息!!! from[{}]", from->GetID());
                RegionDestroy(std::dynamic_pointer_cast<GameMgrGameSession::ActorAgentType>(from), msg);
        }

        return nullptr;
}

ACTOR_MAIL_HANDLE(RequestActor, E_MIMT_GameCommon, E_MIGCST_RegionKickout, MailRegionKickout)
{
        auto regionAgent = std::dynamic_pointer_cast<GameMgrGameSession::ActorAgentType>(from);
        if (!regionAgent)
                return nullptr;

        auto ret = Call(MailResult, regionAgent, E_MIMT_GameCommon, E_MIGCST_RegionKickout, msg);
        if (!ret || E_IET_Success != ret->error_type())
        {
                return nullptr;
        }

        ret = Call(MailResult, regionAgent->_regionMgr, scRegionMgrActorMailMainType, 4, msg);
        if (!ret || E_IET_Success != ret->error_type())
        {
                return nullptr;
        }

        auto f = regionAgent->RemoveFighter(msg->fighter_guid());
        if (f)
                Send(f, E_MIMT_GameCommon, E_MIGCST_RegionKickout, msg);

        return nullptr;
}

void RequestActor::ReqQueue(const stQueueInfoPtr& msg)
{
        DLOG_INFO("RequestActor 收到排队成功消息!!!");
        auto sendMsg = std::make_shared<MailReqQueue>();
        sendMsg->set_error_type(E_IET_Fail);
        sendMsg->set_game_sid(-1);

        GameMgrGameSession::ActorAgentTypePtr regionAgent;
        do
        {
                if (!msg)
                {
                        LOG_WARN("ReqQueue id[{}]", GetID());
                        return;
                }

                if (!msg->_regionMgr)
                {
                        LOG_WARN("ReqQueue id[{}]", GetID());
                        return;
                }

                msg->PackBaseInfo(*sendMsg->mutable_base_info());

                auto regionMgrActor = msg->_regionMgr; // RegionMgr::GetInstance()->GetRegionMgrActor(msg->_regionType);
                auto createRet = Call(stMailCreateRegionAgent, regionMgrActor, scRegionMgrActorMailMainType, 0x3, msg);
                if (!createRet)
                {
                        LOG_WARN("排队成功，创建 region agent 失败!!!");
                        break;
                }

                regionAgent = createRet->_region.lock();
                if (!regionAgent)
                {
                        LOG_WARN("排队成功，region agent is nullptr!!!");
                        sendMsg->set_error_type(E_IET_CreateRegionAgentError);
                        break;
                }

                auto gameSes = regionAgent->GetSession();
                if (!gameSes)
                {
                        LOG_WARN("排队成功，RequestActor game ses already release!!!");
                        sendMsg->set_error_type(E_IET_GameSesAlreadyRelease);
                        break;
                }

                auto ret = Call(MailResult, regionAgent, E_MIMT_GameCommon, E_MIGCST_RegionCreate, regionAgent->_cfg);
                if (!ret || E_IET_Success != ret->error_type())
                {
                        LOG_WARN("排队成功，RequestActor 请求创建 region[{}] 时，超时!!!", regionAgent->GetID());
                        sendMsg->set_error_type(E_IET_RegionCreateTimeout);
                        break;
                }

                auto reqEnterRegionMsg = std::make_shared<MailReqEnterRegion>();
                reqEnterRegionMsg->set_region_type(msg->_regionType);
                reqEnterRegionMsg->set_region_id(regionAgent->GetID());

                for (auto& val : msg->_playerList)
                        val.second->Pack2Region(*reqEnterRegionMsg->add_fighter_list());
                for (auto& val : msg->_robotList)
                        reqEnterRegionMsg->add_robot_list()->CopyFrom(*val.second);

                auto reqRet = Call(MailReqEnterRegionRet,
                                   regionAgent,
                                   E_MCMT_GameCommon,
                                   E_MCGCST_ReqEnterRegion,
                                   reqEnterRegionMsg);
                if (!reqRet || E_IET_Success != reqRet->error_type())
                {
                        auto errorType = reqRet ? reqRet->error_type() : E_IET_CallTimeOut;
                        LOG_WARN("排队成功，RequestActor 请求 enter region[{}] 时，error[{}]!!!",
                                 regionAgent->GetID(), errorType);
                        sendMsg->set_error_type(errorType);
                        break;
                }

                sendMsg->set_region_id(regionAgent->GetID());
                sendMsg->set_game_sid(gameSes->GetSID());
                sendMsg->set_error_type(E_IET_Success);
        } while (0);

        if (E_IET_Success == sendMsg->error_type())
        {
                if (E_QT_Manual == msg->_queueType)
                        regionAgent->_queueGuid = msg->GetID();
                auto reqActor = shared_from_this();
                for (auto& val : msg->_playerList)
                {
                        auto p = val.second;
                        auto agent = p->_agent;
                        auto ses = agent->GetSession();
                        if (!ses)
                                continue;

                        auto oldReqActor = agent->GetBindActor();
                        if (!oldReqActor)
                                continue;

                        agent->SendPush(oldReqActor, E_MIMT_QueueCommon, E_MIQCST_ReqQueue, sendMsg); // 必须在重新绑定之前发送。
                        // ses->RemoveAgent(agent->GetID(), agent->GetLocalID(), agent.get());

                        // 需要重新创建 agent 来绑定 region，因一些玩法在游戏过程中，queue 不会被销毁，
                        // 也会操作 queue，因此不能使用相同 agent。
                        auto tmpAgent = std::make_shared<GameMgrLobbySession::ActorAgentType>(agent->GetID(), ses);
                        tmpAgent->BindActor(reqActor);
                        tmpAgent->BindExtra(regionAgent);
                        regionAgent->AddFighter(tmpAgent);
                        if (!ses->AddAgent(tmpAgent))
                        {
                                LOG_WARN("玩家[{}] 添加到 region[{}] 成功，但 AddAgent 失败!!! qInfo[{}] actor[{} {}]",
                                         agent->GetID(), regionAgent->GetID(), msg->GetID(), agent->GetID(), agent->GetLocalID());
                        }

                        // if (regionAgent)
                        // {
                        //         agent->BindActor(reqActor);
                        //         agent->BindExtra(regionAgent);
                        //         regionAgent->AddFighter(agent);
                        //         ses->AddAgent(agent);
                        // }
                }
        }
        else
        {
                if (regionAgent)
                        RegionDestroy(regionAgent, nullptr);

                for (auto& val : msg->_playerList)
                {
                        auto agent = val.second->_agent;
                        if (agent)
                                agent->SendPush(agent->GetBindActor(), E_MIMT_QueueCommon, E_MIQCST_ReqQueue, sendMsg);
                }
        }
}

ACTOR_MAIL_HANDLE(RequestActor, E_MIMT_QueueCommon, E_MIQCST_ReqQueue, stQueueInfo)
{
        ReqQueue(msg);
        return nullptr;
}

ACTOR_MAIL_HANDLE(RequestActor, E_MCMT_MatchCommon, E_MCMCST_ReqRank, MailMatchReqRank)
{
        auto regionAgent = std::dynamic_pointer_cast<GameMgrGameSession::ActorAgentType>(from);
        if (!regionAgent)
                return nullptr;

        auto m = std::make_shared<stRegionMgrMailCustom>();
        m->_msg = msg;
        auto ret = Call(MailMatchReqRank, regionAgent->_regionMgr, scRegionMgrActorMailMainType, 0xe, m);
        if (!ret || E_IET_Success != ret->error_type())
        {
                DLOG_INFO("RequestActor regionMgrActor req match rank ret[{}] error[{}]!!!",
                          (void*)ret.get(), ret?ret->error_type():E_IET_CallTimeOut);
                return nullptr;
        }

        Send(from, E_MCMT_MatchCommon, E_MCMCST_ReqRank, ret);
        return nullptr;
}

// vim: fenc=utf8:expandtab:ts=8
