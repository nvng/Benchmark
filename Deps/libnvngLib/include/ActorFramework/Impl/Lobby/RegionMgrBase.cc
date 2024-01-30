#include "RegionMgrBase.h"

namespace nl::af::impl {

RegionMgrBase::RegionMgrBase()
        : SuperType("RegionMgrBase")
          , _regionRelationList("RegionMgrBase_regionRelationList")
          , _gameSesList("RegionMgrBase_gameSesList")
          , _regionRelationSyncList("RegionMgrBase_regionRelationSyncList")
          , _mainCityActorArrSize(Next2N(std::thread::hardware_concurrency()) - 1)
          , _competitionQueueInfoList("RegionMgrBase_competitionQueueInfoList")
{
        _mainCityActorArr = new MainCityActorPtr[_mainCityActorArrSize+1];
}

RegionMgrBase::~RegionMgrBase()
{
        delete[] _mainCityActorArr;
        _mainCityActorArr = nullptr;
}

bool RegionMgrBase::Init()
{
        for (int64_t i=0; i<_mainCityActorArrSize+1; ++i)
        {
                _mainCityActorArr[i] = std::make_shared<MainCityActor>();
                _mainCityActorArr[i]->Start();
        }

        return true;
}

IActorPtr RegionMgrBase::GetRegionRelation(const PlayerBasePtr& p)
{
        /*
         * 只会在大厅 crash 时，才会出现 player 中应该有 region 而 region 为 nullptr 的情况。
         * player 只会在返回 mainCityActor 时才会被删除，而返回 mainCityActor 只由 game mgr 控制。
         */
        if (!GetApp()->IsCrash())
                return nullptr;

        auto relationMsg = _regionRelationList.Get(p->GetID());
        if (!relationMsg)
                return nullptr;

        auto gameMgrSes = _gameMgrSes.lock();
        if (!gameMgrSes)
        {
                PLAYER_LOG_INFO(p->GetID(),
                                "玩家[{}] 在大厅 crash 之后请求 GameMgrServer 失败!!!",
                                p->GetID());
                return nullptr;
        }

        auto gameMgrAgent = std::make_shared<LobbyGameMgrSession::ActorAgentType>(relationMsg->requestor_id(), gameMgrSes, relationMsg->region_type());
        gameMgrAgent->BindActor(p);
        gameMgrSes->AddAgent(gameMgrAgent);

        _regionRelationList.Remove(p->GetID(), p.get());
        if (relationMsg->has_region_info()) // region
        {
                const auto& regionMsg = relationMsg->region_info();
                auto gameSes = GetGameSession(regionMsg.game_sid());
                if (!gameSes)
                {
                        PLAYER_LOG_INFO(p->GetID(),
                                        "玩家[{}] 在大厅 crash 之后请求 GameServer sid[{}] 失败!!!",
                                        p->GetID(), regionMsg.game_sid());
                }

                auto agent = std::make_shared<LobbyGameSession::ActorAgentType>(regionMsg.id(), gameSes);
                agent->BindActor(p);
                agent->_regionMgr = gameMgrAgent;
                if (gameSes)
                        gameSes->AddAgent(agent);
                else
                        LobbyGameSession::AddAgentOffline(regionMsg.game_sid(), agent);
                return agent;
        }
        else // queue
        {
                // p->_queue = gameMgrAgent;
                return nullptr;
        }
}

void RegionMgrBase::DealRegionRelation(int64_t id, int64_t mt, int64_t st, const ActorMailDataPtr& msg)
{
        auto info = _regionRelationList.Get(id);
        if (!info)
        {
                PLAYER_LOG_WARN(id, "玩家[{}] 收到 RegionMgr 消息，但没找到agent!!! mt[{:#x}] st[{:#x}]", id, mt, st);
                auto p = GetPlayerMgrBase()->GetActor(id);
                if (p)
                        p->SendPush(nullptr, mt, st, msg);
        }
        else
        {
                _regionRelationList.Remove(id, info.get());
        }
}

EInternalErrorType RegionMgrBase::ReqExitRegion(const PlayerBasePtr& p, const IActorPtr& region)
{
        // ++PlayerBase::_playerFlag;
        auto detailRegion = std::dynamic_pointer_cast<MainCityActor>(region);
        if (detailRegion) // 大厅。
        {
                auto ret = Call(MailResult, p, region, E_MCMT_GameCommon, E_MCGCST_ReqExitRegion, nullptr);
                PLAYER_LOG_ERROR_IF(p->GetID(),
                                    !ret,
                                    "玩家[{}] 退出大厅时失败!!!",
                                    p->GetID());
        }
        else
        {
                auto regionAgent = std::dynamic_pointer_cast<LobbyGameSession::ActorAgentType>(region);
                if (!regionAgent)
                {
                        PLAYER_LOG_ERROR(p->GetID(), "玩家[{}] 请求退出 region 时，regionAgent is nullptr", p->GetID());
                        return E_IET_Fail;
                }

                PLAYER_DLOG_INFO(p->GetID(), "玩家[{}] 请求退出 region[{}]", p->GetID(), region->GetID());
                auto m = std::make_shared<MailReqExit>();
                m->set_player_guid(p->GetID());
                auto exitRet = Call(MailResult, p, regionAgent->_regionMgr, E_MCMT_GameCommon, E_MCGCST_ReqExitRegion, m);
                if (!exitRet || E_IET_Success != exitRet->error_type())
                {
#ifndef ____BENCHMARK____
                        PLAYER_LOG_ERROR(p->GetID(),
                                         "玩家[{}] 请求退出新加入的 region[{}]时，error_type[{}]!!!",
                                         p->GetID(), region->GetID(), exitRet ? exitRet->error_type() : E_IET_CallTimeOut);
#endif
                        return exitRet ? exitRet->error_type() : E_IET_CallTimeOut;
                }
        }

        return E_IET_Success;
}

IActorPtr RegionMgrBase::RequestEnterMainCity(const PlayerBasePtr& p)
{
        /*
         * 1. 成功进入新场景。
         * 2. 退出之前场景，成功则将新场景赋值到玩家，若失败则退出新场景。
         */

        auto region = p->GetRegion();
        auto mainCityActor = GetMainCityActor(p->GetID());
        if (region == mainCityActor)
        {
                PLAYER_DLOG_WARN(p->GetID(), "玩家[{}] 请求进入大厅时，已经在大厅!!!", p->GetID());
                return region;
        }

        // ++PlayerBase::_playerFlag;
        auto ret = Call(MailResult, p, mainCityActor, E_MCMT_GameCommon, E_MCGCST_ReqEnterRegion, nullptr);
        if (!ret || E_IET_Success != ret->error_type()) // 主场景load失败!!!
        {
                PLAYER_LOG_ERROR(p->GetID(),
                                 "玩家[{}]进入大厅时，Load失败!!! error[{}]",
                                 p->GetID(), ret ? ret->error_type() : E_IET_CallTimeOut);
                return IActorPtr();
        }

        // region 还是之前场景。
        if (region)
        {
                if (E_IET_Success != ReqExitRegion(p, region))
                {
                        ReqExitRegion(p, mainCityActor);
                        return IActorPtr();
                }
        }

        // PLAYER_LOG_INFO(p->GetID(), "玩家[{}] 请求进入主场景!!! load 成功!!! region[{}]", p->GetID(), mainCityActor->GetID());
        return mainCityActor;
}

IActorPtr RegionMgrBase::RequestEnterRegion(const PlayerBasePtr& p, const MsgReqEnterRegion& msg)
{
        ERegionType regionType = msg.region_type();
        LobbyGameSession::ActorAgentTypePtr regionAgent;
        if (!p || !ERegionType_GameValid(regionType))
        {
                PLAYER_LOG_ERROR(p->GetID(), "玩家请求进入场景时，参数错误!!!");
                return regionAgent;
        }

	auto gameMgrSes = _gameMgrSes.lock();
	if (!gameMgrSes)
        {
                PLAYER_LOG_ERROR(p->GetID(),
                                 "玩家[{}] 请求进入场景 type[{}] 时，game mgr 断开状态!!!",
                                 p->GetID(), regionType);
		return regionAgent;
        }

	int64_t sid = p->GetClientSID();
        if (-1 == sid)
        {
                PLAYER_LOG_ERROR(p->GetID(),
                                 "玩家[{}] 请求进入场景 type[{}] 时，gate 断开状态!!!",
                                 p->GetID(), regionType);
                return regionAgent;
        }

        auto oldRegion = p->GetRegion();

        auto mail = std::make_shared<MailReqEnterRegion>();
        mail->set_region_type(regionType);
        mail->set_region_guid(msg.region_guid());
        mail->set_old_region_guid(oldRegion ? oldRegion->GetID() : -1);

        auto fInfo = mail->add_fighter_list();
        fInfo->set_gate_sid(sid);
        fInfo->set_lobby_sid(GetApp()->GetSID());
        fInfo->set_player_guid(p->GetID());
        fInfo->set_param(msg.param());

        // 因刚开始不知道被分配到的 RequestActor 及 region，因此只做通用关联。
        auto agent = std::make_shared<LobbyGameMgrSession::ActorAgentType>(0, gameMgrSes, regionType);
        agent->BindActor(p);
        gameMgrSes->AddAgent(agent);

        auto reqEnterRegionRet = Call(MailReqEnterRegionRet, p, agent, E_MCMT_GameCommon, E_MCGCST_ReqEnterRegion, mail);
        if (!reqEnterRegionRet || E_IET_Success != reqEnterRegionRet->error_type())
        {
                PLAYER_LOG_ERROR(p->GetID(),
                                 "玩家[{}] 请求进入场景 type[{}] 时，error_type[{}]!!!",
                                 p->GetID(), regionType, reqEnterRegionRet ? reqEnterRegionRet->error_type() : E_IET_CallTimeOut);
                return regionAgent;
        }

        return DirectEnterRegion(p, gameMgrSes, regionType, reqEnterRegionRet->region_guid(), reqEnterRegionRet->game_sid());
}

struct stQueueAgentCache
{
        LobbyGameMgrSession::ActorAgentTypePtr _agent;
};

LobbyGameMgrSession::ActorAgentTypePtr
RegionMgrBase::RequestQueue(const PlayerBasePtr& p,
                            const MsgReqQueue& reqQueueMsg,
                            EClientErrorType& errorType)
{
        const MsgQueueBaseInfo& queueBaseInfo = reqQueueMsg.base_info();
        PLAYER_DLOG_INFO(p->GetID(), "玩家[{}] 请求排队，type[{}]!!!", p->GetID(), queueBaseInfo.region_type());
        if (!p || !ERegionType_GameValid(queueBaseInfo.region_type()))
        {
                PLAYER_LOG_WARN(p->GetID(), "玩家请求排队时，参数错误!!!");
                errorType = E_CET_Fail;
                return nullptr;
        }

	auto gameMgrSes = _gameMgrSes.lock();
	if (!gameMgrSes)
        {
                PLAYER_LOG_WARN(p->GetID(),
                                 "玩家[{}] 请求排队 type[{}] 时，game mgr 断开状态!!!",
                                 p->GetID(), queueBaseInfo.region_type());
                errorType = E_CET_Fail;
		return nullptr;
        }

	int64_t sid = p->GetClientSID();
        if (-1 == sid)
        {
                PLAYER_LOG_WARN(p->GetID(),
                                 "玩家[{}] 请求排队 type[{}] 时，gate 断开状态!!!",
                                 p->GetID(), queueBaseInfo.region_type());
                errorType = E_CET_Fail;
                return nullptr;
        }

        auto mail = std::make_shared<MailReqQueue>();
        mail->mutable_base_info()->CopyFrom(queueBaseInfo);
        mail->set_player_guid(p->GetID());
        mail->set_lobby_sid(GetApp()->GetSID());
        mail->set_gate_sid(sid);
        mail->set_param(reqQueueMsg.param());
        mail->set_param_1(reqQueueMsg.param_1());
        mail->set_param_2(p->GetAttr<E_PAT_MatchGameZone>());

        auto qPlayerInfo = mail->add_player_list();
        qPlayerInfo->set_nick_name(p->GetNickName());
        qPlayerInfo->set_icon(p->GetIcon());
        qPlayerInfo->add_attr_list(0);
        qPlayerInfo->add_attr_list(p->GetID());
        qPlayerInfo->add_attr_list(p->GetAttr<E_PAT_LV>());

        PackReqQueueInfoExtra(p, queueBaseInfo.region_type(), *qPlayerInfo);

        // 因刚开始不知道被分配到的 RequestActor 及 region，因此只做通用关联。
        auto agent = std::make_shared<LobbyGameMgrSession::ActorAgentType>(0, gameMgrSes, queueBaseInfo.region_type());
        agent->BindActor(p);
        gameMgrSes->AddAgent(agent);

        // 因为网络线程与逻辑线程为异步，因此需要以下代码。
        auto cache = std::make_shared<stQueueAgentCache>();
        std::weak_ptr<stQueueAgentCache> weakCache = cache;

        PlayerBaseWeakPtr weakPlayer = p;
        std::weak_ptr<LobbyGameMgrSession> weakGameMgrSes = gameMgrSes;
        agent->_cb = [weakPlayer, weakGameMgrSes, weakCache, param{reqQueueMsg.param()}, param_1{reqQueueMsg.param_1()}](uint64_t type, const ActorMailDataPtr& msg) {
                auto reqQueueRet = std::dynamic_pointer_cast<MailReqQueue>(msg);
                auto gameMgrSes = weakGameMgrSes.lock();
                auto p = weakPlayer.lock();
                auto cache = weakCache.lock();
                if (!p || !gameMgrSes || !cache || !reqQueueRet || E_IET_Success != reqQueueRet->error_type())
                        return;

                auto queueBaseInfo = reqQueueRet->base_info();
                auto qAgent = std::make_shared<LobbyGameMgrSession::ActorAgentType>(queueBaseInfo.queue_guid(), gameMgrSes, queueBaseInfo.region_type());
                qAgent->BindActor(p);
                qAgent->_queueType = queueBaseInfo.queue_type();
                qAgent->_time = queueBaseInfo.time();
                qAgent->_param = param;
                qAgent->_param_1 = param_1;
                gameMgrSes->AddAgent(qAgent);
                cache->_agent = qAgent;
        };

        auto reqQueueRet = Call(MailReqQueue, p, agent, E_MCMT_QueueCommon, E_MCQCST_ReqQueue, mail);
        if (!reqQueueRet || E_IET_Success != reqQueueRet->error_type())
        {
                PLAYER_LOG_WARN(p->GetID(),
                                 "玩家[{}] 请求排队 type[{}] 时，error_type[{}]!!!",
                                 p->GetID(), queueBaseInfo.region_type(), reqQueueRet ? reqQueueRet->error_type() : E_IET_CallTimeOut);
                errorType = E_CET_Fail;
                return nullptr;
        }

        if (cache->_agent)
                errorType = E_CET_Success;
        return cache->_agent;
}

IActorPtr RegionMgrBase::DirectEnterRegion(const PlayerBasePtr& p,
                                           const std::shared_ptr<LobbyGameMgrSession>& gameMgrSes,
                                           ERegionType regionType,
                                           uint64_t regionGuid,
                                           uint64_t gameSesSID)
{
        LobbyGameSession::ActorAgentTypePtr regionAgent;
        auto gameSes = GetGameSession(gameSesSID);
        if (!gameSes)
        {
                PLAYER_LOG_WARN(p->GetID(),
                                 "玩家[{}] 请求进入 region[{}] 时，gameSes[{}] 未找到!!!",
                                 p->GetID(), regionGuid, gameSesSID);
                RequestEnterMainCity(p);
                ReqExitRegion(p, regionAgent);
                return nullptr;
        }

        // 先全部准备好，再退出旧场景。
        regionAgent = std::make_shared<LobbyGameSession::ActorAgentType>(regionGuid, gameSes);
        regionAgent->BindActor(p);
        gameSes->AddAgent(regionAgent);
        auto gameMgrAgent = std::make_shared<LobbyGameMgrSession::ActorAgentType>(regionGuid, gameMgrSes, regionType);
        regionAgent->_regionMgr = gameMgrAgent;
        gameMgrAgent->BindActor(p);
        if (!gameMgrSes->AddAgent(gameMgrAgent))
                return nullptr;

        auto sysMsg = std::make_shared<MailSyncPlayerInfo2Region>();
        sysMsg->set_player_guid(p->GetID());
        sysMsg->set_lv(p->GetAttr<E_PAT_LV>());
        sysMsg->set_nick_name(p->GetNickName());
        sysMsg->set_icon(p->GetIcon());
        PackPlayerInfo2RegionExtra(p, regionType, *sysMsg);
        auto syncRet = Call(MailResult, p, regionAgent, E_MIMT_GameCommon, E_MIGCST_SyncPlayerInfo2Region, sysMsg);
        if (!syncRet || E_IET_Success != syncRet->error_type())
        {
                PLAYER_LOG_WARN(p->GetID(),
                                 "同步玩家[{}]信息到region[{}]时，error_type[{}]!!!",
                                 p->GetID(), regionGuid, syncRet ? syncRet->error_type() : E_IET_CallTimeOut);
                ReqExitRegion(p, regionAgent);
                return nullptr;
        }

        auto oldRegion = p->GetRegion();
        // 退出旧场景必须是最后一步。
        if (oldRegion)
        {
                // TODO: 退出失败后，需要退出新加入region
                auto exitRet = Call(MailResult, p, oldRegion, E_MCMT_GameCommon, E_MCGCST_ReqExitRegion, nullptr);
                if (!exitRet || E_IET_Success != exitRet->error_type())
                {
                        PLAYER_LOG_WARN(p->GetID(),
                                         "玩家[{}] 请求退出 region[{}] 时，error_type[{}]!!!",
                                         p->GetID(), oldRegion->GetID(), exitRet ? exitRet->error_type() : E_IET_CallTimeOut);
                        ReqExitRegion(p, regionAgent);
                        return nullptr;
                }
        }

        return regionAgent;
}

void RegionMgrBase::PackPlayerInfo2RegionExtra(const PlayerBasePtr& p, ERegionType regionType, MailSyncPlayerInfo2Region& msg)
{
}

void RegionMgrBase::Terminate()
{
        for (int64_t i=0; i<_mainCityActorArrSize+1; ++i)
                _mainCityActorArr[i]->Terminate();
        for (int64_t i=0; i<_mainCityActorArrSize+1; ++i)
                _mainCityActorArr[i]->WaitForTerminate();
        for (int64_t i=0; i<_mainCityActorArrSize+1; ++i)
                _mainCityActorArr[i].reset();

        SuperType::Terminate();
}

void RegionMgrBase::WaitForTerminate()
{
        SuperType::WaitForTerminate();
}

// {{{ MainCityActor

MainCityActor::MainCityActor()
        : ActorImpl(GenGuid(), 1 << 16)
{
}

MainCityActor::~MainCityActor()
{
}

ACTOR_MAIL_HANDLE(MainCityActor, E_MCMT_GameCommon, E_MCGCST_LoadFinish)
{
        // PLAYER_DLOG_INFO("主场景收到玩家[{}] LoadFinish 消息!!!", from->GetID());
	auto ret = std::make_shared<MailResult>();
	ret->set_error_type(E_IET_Fail);
	if (!from)
		return ret;

        auto p = _loadPlayerList.Get(from->GetID()).lock();
        if (!p)
                return ret;

	_loadPlayerList.Remove(from->GetID());
	if (!_playerList.Add(from->GetID(), std::dynamic_pointer_cast<PlayerBase>(from)))
		return ret;

        // PLAYER_DLOG_INFO("主场景 load 玩家[{}] 成功!!!", from->GetID());
	ret->set_error_type(E_IET_Fail);
	return ret;
}

ACTOR_MAIL_HANDLE(MainCityActor, E_MCMT_GameCommon, E_MCGCST_ReqEnterRegion)
{
        // --PlayerBase::_playerFlag;

        // PLAYER_DLOG_INFO("主场景收到玩家[{}]请求进入消息!!!", from->GetID());
        auto ret = std::make_shared<MailResult>();
        ret->set_error_type(E_IET_Fail);
        if (!from)
        {
                PLAYER_LOG_ERROR(from->GetID(), "玩家请求进入大厅，但 form 为空!!!");
                return ret;
        }

        auto p = std::dynamic_pointer_cast<PlayerBase>(from);
        if (!_loadPlayerList.Add(from->GetID(), p))
        {
                PLAYER_LOG_ERROR(from->GetID(),
                                 "玩家[{}]请求进入大厅，但load失败!!!",
                                 p->GetID());
                return ret;
        }

        ret->set_error_type(E_IET_Success);
        return ret;
}

ACTOR_MAIL_HANDLE(MainCityActor, E_MCMT_GameCommon, E_MCGCST_ReqExitRegion)
{
        //  --PlayerBase::_playerFlag;
        // PLAYER_DLOG_INFO("主场景收到玩家[{}]请求退出消息!!!", from->GetID());
        auto retMsg = std::make_shared<MailResult>();
        retMsg->set_error_type(E_IET_Success);
	_loadPlayerList.Remove(from->GetID());
        _playerList.Remove(from->GetID());
	return retMsg;
}

ACTOR_MAIL_HANDLE(MainCityActor, E_MCMT_ClientCommon, E_MCCCST_Disconnect)
{
        // PLAYER_DLOG_INFO("主场景收到玩家[{}] 断线消息!!!", from->GetID());
        return std::make_shared<MailResult>();
}

ACTOR_MAIL_HANDLE(MainCityActor, E_MCMT_ClientCommon, E_MCCCST_Reconnect)
{
        auto ret = std::make_shared<MailFighterReconnect>();
        ret->set_error_type(E_IET_Success);
        ret->set_region_type(E_RT_MainCity);
        return ret;
}

// }}} end of MainCityActor

NET_MSG_HANDLE(LobbyGameMgrSession, E_MIMT_Internal, E_MIIST_SyncRelation, MailPlayerRegionRelationInfo)
{
        GetRegionMgrBase()->_regionRelationList.Add(msg->player_guid(), msg);
}

NET_MSG_HANDLE(LobbyGameMgrSession, E_MIMT_GameCommon, E_MIGCST_RegionDestroy, MailRegionDestroy)
{
        GetRegionMgrBase()->DealRegionRelation(msg->player_guid(), E_MIMT_GameCommon, E_MIGCST_RegionDestroy, msg);
}

NET_MSG_HANDLE(LobbyGameMgrSession, E_MIMT_GameCommon, E_MIGCST_RegionKickout, MailRegionKickout)
{
        GetRegionMgrBase()->DealRegionRelation(msg->fighter_guid(), E_MIMT_GameCommon, E_MIGCST_RegionKickout, msg);
}

NET_MSG_HANDLE(LobbyGameMgrSession, E_MCMT_MatchCommon, E_MCMCST_ReqQueueInfo, MsgMatchQueueInfo)
{
        GetRegionMgrBase()->_competitionQueueInfoList.Remove(msg->queue_type(), nullptr);
        GetRegionMgrBase()->_competitionQueueInfoList.Add(msg->queue_type(), msg);
}

}; // end of namespace nl::af::impl

// vim: fenc=utf8:expandtab:ts=8
