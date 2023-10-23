#include "PlayerBase.h"

#include "PlayerMgrBase.h"
#include "RegionMgrBase.h"
#include "DBMgr.h"

namespace nl::af::impl {

#ifdef ____BENCHMARK____
std::atomic_int64_t PlayerBase::_playerFlag = 0;
#endif

EClientErrorType ConvertFromInternalErrorType(EInternalErrorType iet)
{
        switch (iet)
        {
        case E_IET_Success : return E_CET_Success; break;
        default: return E_CET_Fail; break;
        }
}

bool PlayerBase::Start(std::size_t stackSize)
{
        Run();
        return true;
}

void PlayerBase::Online()
{
        SetAttr<E_PAT_LastLoginTime>(GetClock().GetTimeStamp());

        CheckDayChange();
        QueueOpt(E_QOT_Offline, 0);

        Save2DB();
}

bool PlayerBase::Flush2DB(bool isDelete)
{
        return DBMgr::GetInstance()->SavePlayer(shared_from_this(), isDelete);

        /*
        PauseCostTime();
        DBPlayerInfo dbInfo;
        Pack2DB(dbInfo);
        auto val = Compress::SerializeAndCompress<Compress::E_CT_Zstd>(dbInfo);
        ResumeCostTime();

        std::string key = fmt::format("p:{}", GetID());
        RedisCmd(key, val);
        return true;
        */
}

void PlayerBase::Offline()
{
        SetAttr<E_PAT_LastLogoutTime>(GetClock().GetTimeStamp());
        Call(MailResult, GetRegion(), E_MCMT_ClientCommon, E_MCCCST_Disconnect, nullptr);
        _clientActor.reset();
        QueueOpt(E_QOT_Offline, 1);
}

void PlayerBase::KickOut(int64_t errorType)
{
        auto sendMsg = std::make_shared<MsgClientKickout>();
        sendMsg->set_error_type(static_cast<EClientErrorType>(errorType));

        Send2Client(E_MCMT_ClientCommon, E_MCCCST_Kickout, sendMsg);
}

void PlayerBase::OnClientReconnect(ERegionType oldRegionType)
{
        auto r = GetRegion();
        if (!r)
                return;

        auto mail = std::make_shared<MailFighterReconnect>();
        mail->set_fighter_guid(GetID());
        mail->set_region_id(r->GetID());
        mail->set_gate_sid(GetClientSID());
        auto ret = Call(MailFighterReconnect, r, E_MCMT_ClientCommon, E_MCCCST_Reconnect, mail);
        if (ret)
        {
                if (E_IET_Success == ret->error_type())
                {
                        auto sendMsg = std::make_shared<MailSwitchRegion>();
                        sendMsg->set_region_type(ret->region_type());
                        sendMsg->set_old_region_type(oldRegionType);
                        sendMsg->set_game_sid(ret->game_sid());
                        sendMsg->set_region_id(r->GetID());
                        Send2Client(E_MCMT_GameCommon, E_MCGCST_SwitchRegion, sendMsg);
                }
                else
                {
                        PLAYER_LOG_WARN(GetID(), "玩家登录时，region 重连出错!!! error[{}]", ret->error_type());
                        if (E_IET_GateNotFound == ret->error_type())
                                Send2Client(E_MCMT_GameCommon, E_MCGCST_GameDisconnect, nullptr);
                        else
                                Back2MainCity(E_RT_None);
                }
        }
        else
        {
                // 可能是 game server 还没与 lobby 连好。
                PLAYER_LOG_WARN(GetID(), "玩家登录时，region 重连超时!!!");
                Send2Client(E_MCMT_GameCommon, E_MCGCST_GameDisconnect, nullptr);
        }
}

void PlayerBase::OnCreateAccount()
{
        auto now = GetClock().GetTimeStamp();
        SetAttr<E_PAT_CreateTime>(now);
        SetAttr<E_PAT_LastLoginTime>(now);
        AddAttr<E_PAT_LV>(1);
}

bool PlayerBase::LoadFromDB()
{
        // 此时还未添加到 PlayerMgr，actor 也未开始运行，无法接收消息。

        return DBMgr::GetInstance()->LoadPlayer(shared_from_this());

        /*
        std::string cmd = fmt::format("GET p:{}", GetID());
        auto loadRet = RedisCmd(cmd, true);
        if (loadRet->IsNil())
        {
                OnCreateAccount();
                Flush2DB(false);
        }
        else
        {
                PauseCostTime();
                DBPlayerInfo dbInfo;
                Compress::UnCompressAndParse<Compress::E_CT_Zstd>(dbInfo, loadRet->GetData()._strView);
                InitFromDB(dbInfo);
                ResumeCostTime();
        }
        AfterInitFromDB();
        return true;
        */
}

void PlayerBase::AfterInitFromDB()
{
        Save2DB();
}

void PlayerBase::InitFromDB(const DBPlayerInfo& dbInfo)
{
        for (int64_t i=0; i<EPlayerAttrType_ARRAYSIZE; ++i)
                _attrArr[i] = 0;
        const int64_t attrCnt = std::min<int64_t>(EPlayerAttrType_ARRAYSIZE, dbInfo.attr_list_size());
        for (int64_t i=0; i<attrCnt; ++i)
                _attrArr[i] = dbInfo.attr_list(i);

        _version = dbInfo.version();
        _nickName = dbInfo.nick_name();
        _iconStr = dbInfo.icon();
}

#define PACK_PLAYER_INFO(msg) \
        msg.set_player_guid(GetID()); \
        msg.set_nick_name(GetNickName()); \
        msg.set_icon(GetIcon()); \
        for (int64_t i=0; i<EPlayerAttrType_ARRAYSIZE; ++i) \
                msg.add_attr_list(_attrArr[i])

void PlayerBase::Pack2Client(MsgPlayerInfo& msg)
{
        PACK_PLAYER_INFO(msg);

        if (_queue)
                _queue->PackQueueInfo(*msg.mutable_queue_info());
        if (_matchInfo)
                msg.mutable_match_info()->CopyFrom(*_matchInfo);
}

void PlayerBase::Pack2DB(DBPlayerInfo& dbInfo)
{
        dbInfo.set_version(++_version);
        PACK_PLAYER_INFO(dbInfo);
}

void PlayerBase::Send2Client(uint64_t mainType, uint64_t subType, const std::shared_ptr<google::protobuf::MessageLite>& msg)
{
        if (_clientActor)
                Send(_clientActor, mainType, subType, msg);
}

void PlayerBase::SetClient(const LobbyGateSession::ActorAgentTypePtr& client)
{
        if (!client)
        {
                PLAYER_LOG_WARN(GetID(), "玩家[{}] 设置 clientAgent 时，client 为 nullptr!!!", GetID());
                return;
        }

        // 在 PlayerBase 中执行。因要获取 _clientActor 中的 sid等信息。
        if (_clientActor)
        {
                KickOut(E_CET_ForeignLogin);
        }

        auto ses = client->GetSession();
        if (ses)
        {
                // 只是将此 reset 还不行，必须调用 ses->RemoveAgent();
                // 因在极端情况下，其它地方还持有。
                // _clientActor.reset();

                client->BindActor(shared_from_this());

                // 必须在添加前再试图删除一次，Offline 等其它地方 reset 后，
                // 可能还是有持有问题。
                ses->RemoveAgent(client->GetID(), client->GetLocalID(), _clientActor.get());
                ses->AddAgent(client);
                _clientActor = client;
        }
        else
        {
                PLAYER_DLOG_WARN(GetID(), "玩家[{}] 设置 clientAgent 时，ses is nullptr!!!", GetID());
                _clientActor = client;
                OnDisconnect(client);
        }
}

int64_t PlayerBase::GetClientSID() const
{
        int64_t sid = -1;
        if (_clientActor)
        {
                auto tmpSes = _clientActor->GetSession();
                if (tmpSes)
                        sid = tmpSes->GetSID();
        }
        return sid;
}

void PlayerBase::InitSavePlayer2DBTimer()
{
        if (FLAG_HAS(_internalFlag, E_PIF_InSaveTimer))
                return;

        FLAG_ADD(_internalFlag, E_PIF_InSaveTimer);
        std::weak_ptr<PlayerBase> weakPlayer = shared_from_this();
        _saveTimer.Start(weakPlayer, 5 * 60, [weakPlayer]() {
                auto p = weakPlayer.lock();
                if (p)
                {
                        FLAG_DEL(p->_internalFlag, E_PIF_InSaveTimer);
                        if (!p->Flush2DB(false))
                                p->InitSavePlayer2DBTimer();
                }
        });
}

bool PlayerBase::CheckDayChange()
{
        auto now = GetClock().GetTimeStamp();
        if (now - GetAttr<E_PAT_LastDayChangeResetTime>() < DAY_TO_SEC(1))
                return false;

        OnDayChange(false);
        return true;
}

void PlayerBase::OnDayChange(bool sync)
{
        auto now = GetClock().GetTimeStamp();
        SetAttr<E_PAT_LastDayChangeResetTime>(Clock::TimeClear_Slow(now, Clock::E_CTT_DAY));

        if (sync)
                Send2Client(E_MCMT_ClientCommon, E_MCCCST_DayChange, nullptr);
        Save2DB();
}

bool PlayerBase::CheckDataReset(int64_t h)
{
        auto now = GetClock().GetTimeStamp();
        auto diff = now - GetAttr<E_PAT_LastNotZeroResetTime>();
        if (diff < DAY_TO_SEC(1) + HOUR_TO_SEC(h))
                return false;

        MsgDataResetNoneZero sendMsg;
        OnDataReset(sendMsg, h);
        return true;
}

void PlayerBase::OnDataReset(MsgDataResetNoneZero& msg, int64_t h)
{
        auto now = GetClock().GetTimeStamp();
        auto zero = Clock::TimeClear_Slow(now, Clock::E_CTT_DAY);
        SetAttr<E_PAT_LastNotZeroResetTime>((now < zero + HOUR_TO_SEC(h)) ? zero - DAY_TO_SEC(1) : zero);

        // PLAYER_DLOG_INFO(GetID(), "_lastNotZeroResetTime[{}] str[{}]", _lastNotZeroResetTime, Clock::GetTimeString_Slow(_lastNotZeroResetTime));
        // PLAYER_DLOG_INFO(GetID(), "playerGuid[{}] on player data reset!!!", GetID());
        Save2DB();
}

void PlayerBase::ReqEnterRegion(const std::shared_ptr<MsgReqEnterRegion>& msg)
{
        ERegionType regionType = msg->region_type();
        msg->set_error_type(E_CET_Success);
        do
        {
                PLAYER_DLOG_INFO(GetID(), "玩家[{}] 请求进入主场景!!!", GetID());
                auto ret = GetRegionMgrBase()->RequestEnterMainCity(shared_from_this());
                if (!ret)
                {
                        PLAYER_LOG_INFO(GetID(), "玩家[{}] 请求进入主场景失败!!!", GetID());
                        break;
                }
                auto sendMsg = std::make_shared<MailSwitchRegion>();
                sendMsg->set_old_region_type(static_cast<ERegionType>(ret->GetType()));
                SetRegion(ret);

                PLAYER_DLOG_INFO(GetID(),
                                 "玩家[{}]请求进入场景type[{}]成功，发送 switch region 消息!!!",
                                 GetID(), regionType);
                sendMsg->set_region_type(regionType);
                Send2Client(E_MCMT_GameCommon, E_MCGCST_SwitchRegion, sendMsg);
                return;
        } while (0);

        PLAYER_DLOG_INFO(GetID(), "玩家[{}]请求进入场景type[{}]失败!!!", GetID(), regionType);
        msg->set_error_type(E_CET_Fail);
        Send2Client(E_MCMT_GameCommon, E_MCGCST_ReqEnterRegion, msg);
        return;
}

void PlayerBase::Back2MainCity(ERegionType oldRegionType)
{
        auto retRegion = GetRegionMgrBase()->RequestEnterMainCity(shared_from_this());
        if (!retRegion)
        {
#ifndef ____BENCHMARK____
                PLAYER_LOG_WARN(GetID(), "将玩家加入到大厅出错 玩家[{}]", GetID());
#endif
                return;
        }

        auto sendMsg = std::make_shared<MailSwitchRegion>();
        sendMsg->set_region_type(E_RT_MainCity);
        sendMsg->set_old_region_type(oldRegionType);
        Send2Client(E_MCMT_GameCommon, E_MCGCST_SwitchRegion, sendMsg);

        SetRegion(retRegion);
        return;
}

void PlayerBase::PackUpdatePlayerAttrExtra(EPlayerAttrType t, MsgUpdatePlayerAttr& msg)
{
        switch (t)
        {
        case E_PAT_Exp : msg.set_val_1(GetAttr<E_PAT_LV>()); break;
        case E_PAT_TiLi : msg.set_val_1(GetAttr<E_PAT_TiLiStartRecoverTime>()); break;
        default :
                break;
        }
}

void PlayerBase::CheckTiLiRecovery()
{
        if (_attrArr[E_PAT_TiLiStartRecoverTime] <= 0)
                return;
        const time_t now = GetClock().GetTimeStamp();
        const auto diff = now - _attrArr[E_PAT_TiLiStartRecoverTime];
        if (diff < 5 * 60)
                return;

        const auto oldTiLi = _attrArr[E_PAT_TiLi];
        _attrArr[E_PAT_TiLi] = std::min<int64_t>(120, _attrArr[E_PAT_TiLi] + diff / (5 * 60));
        if (_attrArr[E_PAT_TiLi] >= 120)
                _attrArr[E_PAT_TiLiStartRecoverTime] = 0;
        else
                _attrArr[E_PAT_TiLiStartRecoverTime] += (_attrArr[E_PAT_TiLi] - oldTiLi) * (5 * 60);
}

template <>
int64_t PlayerBase::GetAttr<E_PAT_TiLi>()
{
        CheckTiLiRecovery();
        return _attrArr[E_PAT_TiLi];
}

template <>
int64_t PlayerBase::AddAttr<E_PAT_TiLi>(int64_t cnt)
{
        CheckTiLiRecovery();
        if (cnt <= 0)
                return _attrArr[E_PAT_TiLi];
        _attrArr[E_PAT_TiLi] = _attrArr[E_PAT_TiLi] + cnt;
        if (_attrArr[E_PAT_TiLi] >= 120)
                _attrArr[E_PAT_TiLiStartRecoverTime] = 0;
        Save2DB();
        return _attrArr[E_PAT_TiLi];
}

template <>
int64_t PlayerBase::AddAttr<E_PAT_TiLi>(MsgPlayerChange& msg, int64_t cnt)
{
        auto ret = AddAttr<E_PAT_TiLi>(cnt);
        PackUpdatePlayerAttr<E_PAT_TiLi>(msg);
        return ret;
}

template <>
bool PlayerBase::DelAttr<E_PAT_TiLi>(int64_t cnt)
{
        CheckTiLiRecovery();
        if (cnt <= 0 || _attrArr[E_PAT_TiLi] < cnt)
                return false;
        _attrArr[E_PAT_TiLi] -= cnt;
        if (_attrArr[E_PAT_TiLi] < 120 && _attrArr[E_PAT_TiLiStartRecoverTime] <= 0)
                _attrArr[E_PAT_TiLiStartRecoverTime] = GetClock().GetTimeStamp();
        Save2DB();
        return true;
}
template <>
bool PlayerBase::DelAttr<E_PAT_TiLi>(MsgPlayerChange& msg, int64_t cnt)
{
        auto ret = DelAttr<E_PAT_TiLi>(cnt);
        PackUpdatePlayerAttr<E_PAT_TiLi>(msg);
        return ret;
}

int64_t PlayerBase::GetMoney(int64_t t)
{
        switch (t)
        {
        case E_PAT_Coins :      return GetAttr<E_PAT_Coins>();           break;
        case E_PAT_TiLi :       return GetAttr<E_PAT_TiLi>();            break;
        case E_PAT_Diamonds :   return GetAttr<E_PAT_Diamonds>();        break;
        case E_PAT_ZhanLinExp : return GetAttr<E_PAT_ZhanLinExp>();      break;
        case E_PAT_MatchScore : return GetAttr<E_PAT_MatchScore>();      break;
        default : return INVALID_MONEY_VAL; break;
        }
}

int64_t PlayerBase::AddMoney(MsgPlayerChange& msg, int64_t t, int64_t cnt, ELogServiceOrigType logType, uint64_t logParam)
{
        int64_t ret = 0;
        switch (t)
        {
        case E_PAT_Coins :      ret = AddAttr<E_PAT_Coins>(msg, cnt);           break;
        case E_PAT_TiLi :       ret = AddAttr<E_PAT_TiLi>(msg, cnt);            break;
        case E_PAT_Diamonds :   ret = AddAttr<E_PAT_Diamonds>(msg, cnt);        break;
        case E_PAT_ZhanLinExp : ret = AddAttr<E_PAT_ZhanLinExp>(msg, cnt);      break;
        case E_PAT_MatchScore : ret = AddAttr<E_PAT_MatchScore>(msg, cnt);      break;
        default : return INVALID_MONEY_VAL; break;
        }
        return ret;
}

bool PlayerBase::CheckMoney(int64_t t, int64_t cnt)
{
        switch (t)
        {
        case E_PAT_Coins :      return GetAttr<E_PAT_Coins>() >= cnt;           break;
        case E_PAT_TiLi :       return GetAttr<E_PAT_TiLi>() >= cnt;            break;
        case E_PAT_Diamonds :   return GetAttr<E_PAT_Diamonds>() >= cnt;        break;
        case E_PAT_ZhanLinExp : return GetAttr<E_PAT_ZhanLinExp>() >= cnt;      break;
        case E_PAT_MatchScore : return GetAttr<E_PAT_MatchScore>() >= cnt;      break;
        default : return false; break;
        }
}

int64_t PlayerBase::DelMoney(MsgPlayerChange& msg, int64_t t, int64_t cnt, ELogServiceOrigType logType, uint64_t logParam)
{
        int64_t ret = 0;
        switch (t)
        {
        case E_PAT_Coins :      ret = DelAttr<E_PAT_Coins>(msg, cnt);           break;
        case E_PAT_TiLi :       ret = DelAttr<E_PAT_TiLi>(msg, cnt);            break;
        case E_PAT_Diamonds :   ret = DelAttr<E_PAT_Diamonds>(msg, cnt);        break;
        case E_PAT_ZhanLinExp : ret = DelAttr<E_PAT_ZhanLinExp>(msg, cnt);      break;
        case E_PAT_MatchScore : ret = DelAttr<E_PAT_MatchScore>(msg, cnt);      break;
        default : return INVALID_MONEY_VAL; break;
        }
        return ret;
}

bool PlayerBase::AddExp(int64_t exp)
{
        if (exp <= 0)
                return false;

        const int64_t oldLV = GetAttr<E_PAT_LV>();
        const auto totalLVExp = AddAttr<E_PAT_Exp>(exp);
        for (int i=0; i<1000; ++i)
        {
                auto needExp = GetPlayerMgrBase()->GetExpByLV(GetAttr<E_PAT_LV>());
                if (totalLVExp >= needExp)
                        AddAttr<E_PAT_LV>(1);
                else
                        break;
        }
        Save2DB();
        return GetAttr<E_PAT_LV>() != oldLV;
}

void PlayerBase::PackUpdateGoods(MsgPlayerChange& msg, int64_t t, int64_t id, int64_t num)
{
        if (0 != id)
        {
                auto info = msg.add_change_goods_list();
                info->set_type(t);
                info->set_id(id);
                info->set_num(num);
        }
}

void PlayerBase::Terminate()
{
        SuperType::Terminate();
}

void PlayerBase::OnDisconnect(const IActorPtr& agent)
{
        if (!agent)
        {
                PLAYER_LOG_WARN(GetID(), "玩家[{}] 本地离线，但 agent is nullptr!!!", GetID());
                return;
        }

        if (agent == _clientActor)
        {
                // 不用作优化，极少情况会走这里。
                auto msg = std::make_shared<stDisconnectInfo>();
                msg->_uniqueID = _uniqueID;
                msg->_to = GetID();
                GetPlayerMgrBase()->GetPlayerMgrActor(GetID())->SendPush(shared_from_this(), 0x2, msg);
        }
        else if (agent == GetRegion())
        {
                // 不删除，game server 重连上来之后，需要发送 reconnect。
                // region 数据以 game mgr server 为准，因此不需要 game server 向 lobby 同步数据。
                // _region.reset();
                Send(shared_from_this(), E_MCMT_GameCommon, E_MCGCST_GameDisconnect, nullptr);
        }
        else if (agent == _queue)
        {
                _queue.reset();
                ExitQueue();
        }
        else
        {
                PLAYER_LOG_WARN(GetID(), "玩家[{}] 本地离线，但未匹配到agent!!!", GetID());
        }
}

ACTOR_MAIL_HANDLE(PlayerBase, E_MIMT_Local, E_MILST_Disconnect)
{
        OnDisconnect(from);
        return nullptr;
}

ACTOR_MAIL_HANDLE(PlayerBase, E_MIMT_Local, E_MILST_Reconnect, MailResult)
{
        if (GetRegion() == from)
        {
                /*
                 * 直接客户端无感重连，若无 region，则直接切回大厅，若有，直接重连上。
                 * 虽 game mgr server 会通知，但需要防止漏网之鱼，
                 * 因 region 的持有情况影响到玩家删除操作。
                 */
                if (E_IET_RemoteCrash == msg->error_type())
                {
                        auto m = std::make_shared<MailRegionDestroy>();
                        m->set_player_guid(GetID());
                        SendPush(from, E_MIMT_GameCommon, E_MIGCST_RegionDestroy, m);
                }
                else
                {
                        SendPush(from, E_MCMT_ClientCommon, E_MCCCST_Reconnect, nullptr);
                }
        }
        else if (_queue == from)
        {
                if (E_IET_RemoteCrash == msg->error_type())
                {
                        _queue.reset();
                        ExitQueue();
                }
                else
                {
                        Send(from, E_MIMT_Internal, E_MIIST_Reconnect, nullptr);
                }
        }
        else
        {
                auto r = GetRegion();
                if (r)
                {
                        auto regionAgent = std::dynamic_pointer_cast<LobbyGameSession::ActorAgentType>(from);
                        if (regionAgent)
                        {
                                auto regionMgrAgent = std::dynamic_pointer_cast<LobbyGameMgrSession::ActorAgentType>(regionAgent->_regionMgr);
                                if (regionMgrAgent)
                                {
                                        PLAYER_LOG_WARN(GetID(),
                                                        "region[{}] 对应的 regionMgr[{}] 重连上来了!!!",
                                                        regionAgent->GetID(), regionMgrAgent->GetID());
                                }
                        }
                }
        }

        return nullptr;
}

ACTOR_MAIL_HANDLE(PlayerBase, E_MIMT_Internal, E_MIIST_Reconnect, MailResult)
{
        return nullptr;
}

ACTOR_MAIL_HANDLE(PlayerBase, E_MIMT_Local, E_MILST_DayChange)
{
        OnDayChange(true);
        return nullptr;
}

ACTOR_MAIL_HANDLE(PlayerBase, E_MIMT_Local, E_MILST_DataResetNoneZero, MailInt)
{
        auto sendMsg = std::make_shared<MsgDataResetNoneZero>();
        OnDataReset(*sendMsg, msg->val());
        Send2Client(E_MCMT_ClientCommon, E_MCCCST_DataResetNoneZero, sendMsg);
        return nullptr;
}

ACTOR_MAIL_HANDLE(PlayerBase, E_MIMT_Local, E_MILST_Terminate)
{
        Offline(); // 有些活动需要 offline 来触发计时，如 在线礼包。
        Flush2DB(true);
        Terminate();
        return nullptr;
}

ACTOR_MAIL_HANDLE(PlayerBase, E_MCMT_ClientCommon, E_MCCCST_Login, stLoginInfo)
{
        // 宕机后，玩家首次登录，如何确定是否在游戏中。

        PLAYER_DLOG_INFO(GetID(), "玩家[{}] 登录上来了!!!", GetID());

        SetClient(msg->_clientAgent);

        auto r = GetRegion();
        if (!r)
        {
                // 必须放到 Online 之前，此处可能初始化 queue。
                SetRegion(GetRegionMgrBase()->GetRegionRelation(shared_from_this()));
                r = GetRegion();
        }

        Online();

        if (!r)
        {
                Back2MainCity(E_RT_None);
        }
        else
        {
                OnClientReconnect(E_RT_MainCity);
        }

        // 必须在此，需要完成 Online 所需所有操作后再删除。
        GetPlayerMgrBase()->_loginInfoList.Remove(GetID(), msg.get());
        return nullptr;
}

void PlayerBase::InitDeletePlayerTimer(uint64_t deleteUniqueID, double internal/* = 30.0*/)
{
        if (FLAG_HAS(_internalFlag, E_PIF_InDeleteTimer))
                return;

        internal = LimitRange(internal, 1.0, 10 * 60.0);
        FLAG_ADD(_internalFlag, E_PIF_InDeleteTimer);
        std::weak_ptr<PlayerBase> weakPlayer = shared_from_this();
        _deleteTimer.Start(weakPlayer, 30, [weakPlayer, deleteUniqueID, internal]() {
                auto p = weakPlayer.lock();
                if (!p)
                        return;
                FLAG_DEL(p->_internalFlag, E_PIF_InDeleteTimer);
                if (deleteUniqueID != p->_uniqueID)
                {
                        if (!FLAG_HAS(p->_internalFlag, E_PIF_Online))
                                p->InitDeletePlayerTimer(p->_uniqueID);
                        return;
                }

                if (p->_queue)
                {
                        switch (p->_queue->_queueType)
                        {
                        case E_QT_CompetitionKnokout :
                                p->InitDeletePlayerTimer(p->_uniqueID, internal * 4);
                                return;
                                break;
                        default :
                                break;
                        }

                        auto mail = std::make_shared<MsgExitQueue>();
                        mail->set_player_guid(p->GetID());
                        p->_queue->PackQueueBaseInfo(*mail->mutable_base_info());
                        auto exitQueueRet = Call(MailResult, p, p->_queue, E_MCMT_QueueCommon, E_MCQCST_ExitQueue, mail);
                        if (!exitQueueRet || E_IET_Success != exitQueueRet->error_type())
                        {
                                PLAYER_LOG_WARN(p->GetID(),
                                                "玩家[{}]离线时，退出队列失败!!! error[{}]",
                                                p->GetID(), exitQueueRet ? exitQueueRet->error_type() : E_IET_CallTimeOut);
                        }
                }

                auto r = p->GetRegion();
                if (r)
                {
                        if (E_RT_MainCity == r->GetType())
                        {
                                // DBServer 断开连接，player 不能删除，直到存储成功。
                                if (!p->Flush2DB(true))
                                {
                                        p->InitDeletePlayerTimer(p->_uniqueID);
                                        return;
                                }

                                auto ret = GetRegionMgrBase()->ReqExitRegion(p, r);
                                PLAYER_LOG_FATAL_IF(p->GetID(), E_IET_Success != ret, "3333333333333333333333333333333333333 id[{}] ret[{}]", p->GetID(), ret);
                                p->_region.reset();
                        }
                        else // 在其它 Region 中，需要等回去主场景后，再进行删除。
                        {
                                p->InitDeletePlayerTimer(p->_uniqueID, internal * 4);
                                return;
                        }
                }
                else
                {
                        PLAYER_DLOG_WARN(p->GetID(), "玩家[{}] 删除时，r is nullptr!!!", p->GetID());
                }

                PLAYER_DLOG_INFO(p->GetID(),
                                 "11111111111111111111 删除玩家!!! id:{} flag[{}]",
                                 p->GetID(), p->_internalFlag);
                auto deleteMsg = std::make_shared<MailUInt>();
                deleteMsg->set_val(deleteUniqueID);
                Call(MailUInt, p, GetPlayerMgrBase()->GetPlayerMgrActor(p->GetID()),
                     scPlayerMgrActorMailMainType, 1, deleteMsg);
        });
}

ACTOR_MAIL_HANDLE(PlayerBase, E_MCMT_ClientCommon, E_MCCCST_Disconnect, stDisconnectInfo)
{
        PLAYER_DLOG_INFO(GetID(), "玩家[{}] 断开连接!!!", GetID());
        Offline();
        InitDeletePlayerTimer(msg->_uniqueID);
        return nullptr;
}

ACTOR_MAIL_HANDLE(PlayerBase, E_MCMT_ClientCommon, E_MCCCST_Kickout, MailInt)
{
        KickOut(msg->val());
        return nullptr;
}

ACTOR_MAIL_HANDLE(PlayerBase, E_MCMT_GameCommon, E_MCGCST_ReqEnterRegion, MsgReqEnterRegion)
{
        ReqEnterRegion(msg);
        return nullptr;
}

ACTOR_MAIL_HANDLE(PlayerBase, E_MCMT_GameCommon, E_MCGCST_GameDisconnect)
{
        Send2Client(E_MCMT_GameCommon, E_MCGCST_GameDisconnect, nullptr);
        return nullptr;
}

std::shared_ptr<MsgReqQueue> PlayerBase::ReqQueue(const std::shared_ptr<MsgReqQueue>& msg)
{
        // PLAYER_DLOG_INFO(GetID(), "玩家[{}] 请求排队，type[{}]!!!", GetID(), msg->region_type());
        do
        {
                auto r = GetRegion();
                if (!r)
                {
                        PLAYER_LOG_WARN(GetID(), "玩家[{}] 请求排队时，region is nullptr!!!", GetID());
                        break;
                }

                if (E_RT_MainCity != r->GetType())
                {
                        PLAYER_LOG_WARN(GetID(), "玩家[{}]请求排队时，不在大厅!!!", GetID());
                        msg->set_error_type(E_CET_AlreadyInRegion);
                        break;
                }

                if (_queue)
                {
                        PLAYER_LOG_WARN(GetID(),
                                         "玩家[{}]请求排队时，已经在一个 queue[{}] 中!!!",
                                         GetID(), _queue->GetID());
                        msg->set_error_type(E_CET_AlreadyInQueue);
                        break;
                }

                EClientErrorType errorType = E_CET_Success;
                auto q = GetRegionMgrBase()->RequestQueue(shared_from_this(), *msg, errorType);
                if (!q)
                {
                        PLAYER_LOG_WARN(GetID(),
                                         "玩家[{}] 请求排队失败!!! regionType[{}] queueType[{}] errorType[{:#x}]",
                                         GetID(), msg->base_info().region_type(), msg->base_info().queue_type(), errorType);
                        msg->set_error_type(errorType);
                        break;
                }

                _queue = q;
                _queue->PackQueueBaseInfo(*msg->mutable_base_info());
                msg->set_error_type(errorType);
        } while (0);

        // PLAYER_DLOG_INFO(GetID(), "玩家[{}] 请求排队 type[{}]，返回 error[{}]!!!", GetID(), msg->region_type(), msg->error_type());
        return msg;
}

EInternalErrorType PlayerBase::QueueOpt(EQueueOptType opt, int64_t param)
{
        auto q = GetQueue();
        if (q && E_QT_Manual == q->_queueType)
        {
                auto mail = std::make_shared<MsgQueueOpt>();
                q->PackQueueBaseInfo(*mail->mutable_base_info());
                mail->set_player_guid(GetID());
                mail->set_opt_type(opt);
                mail->set_param(param);

                auto ret = Call(MailResult, q, E_MCMT_QueueCommon, E_MCQCST_Opt, mail);
                if (!ret || E_IET_Success != ret->error_type())
                        return ret ? ret->error_type() : E_IET_Fail;
        }
        return E_IET_Success;
}

ACTOR_MAIL_HANDLE(PlayerBase, E_MCMT_QueueCommon, E_MCQCST_ReqQueue, MsgReqQueue)
{
        return ReqQueue(msg);
}

std::shared_ptr<MsgExitQueue> PlayerBase::ExitQueueInternal()
{
        PLAYER_DLOG_INFO(GetID(), "玩家[{}] 请求退出排队!!!", GetID());
        auto sendMsg = std::make_shared<MsgExitQueue>();
        do
        {
                if (!_queue)
                {
                        PLAYER_DLOG_WARN(GetID(), "玩家[{}] 请求退出排队时，queue为空!!!", GetID());
                        // sendMsg->set_error_type(E_CET_Success);
                        break;
                }

                sendMsg->set_player_guid(GetID());
                _queue->PackQueueBaseInfo(*sendMsg->mutable_base_info());
                auto ret = Call(MailResult, _queue, E_MCMT_QueueCommon, E_MCQCST_ExitQueue, sendMsg);
                if (!ret || E_IET_Success != ret->error_type())
                {
                        auto errorType = ret ? ret->error_type() : E_IET_CallTimeOut;
                        PLAYER_LOG_WARN(GetID(),
                                         "玩家[{}]请求退出 queue[{}]时，error[{:#x}]!!!",
                                         GetID(), _queue->GetID(), errorType);
                        sendMsg->set_error_type(ConvertFromInternalErrorType(errorType));
                        break;
                }
                _queue.reset();
                sendMsg->set_error_type(E_CET_Success);
        } while (0);

        // PLAYER_DLOG_INFO(GetID(), "玩家[{}] 请求退出排队{}!!!", GetID(), sendMsg->error_type() == E_CET_Success ? "成功" : "失败");
        return sendMsg;
}

void PlayerBase::ExitQueue()
{
        Send2Client(E_MCMT_QueueCommon, E_MCQCST_ExitQueue, ExitQueueInternal());
}

ACTOR_MAIL_HANDLE(PlayerBase, E_MCMT_QueueCommon, E_MCQCST_ExitQueue)
{
        return ExitQueueInternal();
}

ACTOR_MAIL_HANDLE(PlayerBase, E_MCMT_QueueCommon, E_MCQCST_Opt, MsgQueueOpt)
{
        do
        {
                auto q = GetQueue();
                if (!q)
                        break;

                switch (msg->opt_type())
                {
                case E_QOT_Kickout :
                        break;
                default :
                        msg->set_player_guid(GetID());
                        break;
                }

                q->PackQueueBaseInfo(*msg->mutable_base_info());
                auto ret = Call(MailResult, q, E_MCMT_QueueCommon, E_MCQCST_Opt, msg);
                if (!ret || E_IET_Success != ret->error_type())
                {
                        PLAYER_LOG_WARN(GetID(), "玩家[{}] 进行 QueueOpt 时出错!!! error[{}]",
                                        GetID(), ret ? ret->error_type() : E_IET_CallTimeOut);
                        break;
                }

                msg->set_error_type(E_CET_Success);
        } while (0);

        Send2Client(E_MCMT_QueueCommon, E_MCQCST_Opt, msg);
        return nullptr;
}


ACTOR_MAIL_HANDLE(PlayerBase, E_MIMT_QueueCommon, E_MIQCST_ExitQueue)
{
        if (!_queue)
                return nullptr;

        auto sendMsg = std::make_shared<MsgExitQueue>();
        _queue->PackQueueBaseInfo(*sendMsg->mutable_base_info());
        sendMsg->set_error_type(E_CET_Success);

        _queue.reset();
        _matchInfo.reset();
        return sendMsg;
}

ACTOR_MAIL_HANDLE(PlayerBase, E_MIMT_QueueCommon, E_MIQCST_ReqQueue, MailReqQueue)
{
        if (!_queue)
                return nullptr;

        switch (msg->error_type())
        {
        case E_IET_Success :
                {
                        auto ret = GetRegionMgrBase()->DirectEnterRegion(shared_from_this(),
                                                                         _queue->GetSession(),
                                                                         msg->base_info().region_type(),
                                                                         msg->region_id(),
                                                                         msg->game_sid());
                        if (!ret)
                        {
                                PLAYER_LOG_WARN(GetID(),
                                                 "玩家[{}] 排队直接进入 rg[{}] 失败!!! qg[{}]",
                                                 GetID(), msg->region_id(), _queue->GetID());
                                break;
                        }

                        auto region = std::dynamic_pointer_cast<LobbyGameSession::ActorAgentType>(ret);
                        auto gameSes = region->GetSession();
                        if (!gameSes)
                        {
                                // TODO:
                                PLAYER_LOG_WARN(GetID(),
                                                 "玩家[{}] 排队直接进入场景成功，但 game ses 已经释放!!!",
                                                 GetID());
                                break;
                        }

                        _region = region;

                        auto sendMsg = std::make_shared<MailSwitchRegion>();
                        sendMsg->set_region_type(static_cast<ERegionType>(_region->GetType()));
                        sendMsg->set_old_region_type(E_RT_MainCity);
                        sendMsg->set_game_sid(gameSes->GetSID());
                        sendMsg->set_region_id(region->GetID());
                        Send2Client(E_MCMT_GameCommon, E_MCGCST_SwitchRegion, sendMsg);
                }
                break;
                // case E_IET_Dismiss : break;
        default :
                {
                        PLAYER_LOG_WARN(GetID(), "玩家[{}] 请求排队出错，error[{:#x}]", GetID(), msg->error_type());
                        auto sendMsg = std::make_shared<MsgReqQueue>();
                        sendMsg->set_error_type(E_CET_QueueError);
                        Send2Client(E_MCMT_QueueCommon, E_MCQCST_ReqQueue, sendMsg);
                }
                break;
        }

        // PLAYER_LOG_INFO(GetID(), "玩家请求排队后，异步返回时，出错!!! error_type[{}]", msg->error_type());
        switch (_queue->_queueType)
        {
        case E_QT_Manual :
        case E_QT_CompetitionKnokout :
                break;
        default :
                _queue.reset();
                break;
        }
        return nullptr;
}

ACTOR_MAIL_HANDLE(PlayerBase, E_MCMT_QueueCommon, E_MCQCST_SyncQueueInfo, MsgSyncQueueInfo)
{
        if (!_queue || !_queue->QueueEqual(msg->base_info()))
                return nullptr;

        _queue->_playerList.CopyFrom(msg->player_list());
        Send2Client(E_MCMT_QueueCommon, E_MCQCST_SyncQueueInfo, msg);
        return nullptr;
}

ACTOR_MAIL_HANDLE(PlayerBase, E_MIMT_MatchCommon, E_MIMCST_Info, MsgMatchInfo)
{
        _matchInfo = msg;
        Send2Client(E_MCMT_MatchCommon, E_MCMCST_Info, msg);
        return nullptr;
}

ACTOR_MAIL_HANDLE(PlayerBase, E_MCMT_MatchCommon, E_MCMCST_ReqQueueInfo, MsgMatchQueueInfo)
{
        return GetRegionMgrBase()->_competitionQueueInfoList.Get(msg->queue_type());
}

ACTOR_MAIL_HANDLE(PlayerBase, E_MCMT_ClientCommon, E_MCCCST_Reconnect)
{
        auto r = GetRegion();
        if (r)
                OnClientReconnect(static_cast<ERegionType>(r->GetType()));
        else
                Send2Client(E_MCMT_ClientCommon, E_MCCCST_Logout, nullptr);

        return nullptr;
}

ACTOR_MAIL_HANDLE(PlayerBase, E_MIMT_Internal, E_MIIST_Result, MailAgentNotFoundResult)
{
        switch (msg->error_type())
        {
        case E_IET_RegionNotFound :
                {
                        auto r = GetRegion();
                        Back2MainCity(r ? static_cast<ERegionType>(r->GetType()) : E_RT_None);
                }
                break;
        case E_IET_RegionMgrNotFound :
                if (from && _queue == from)
                {
                        _queue.reset();
                        ExitQueue();
                }
                break;
        default :
                break;
        }

        return nullptr;
}

// ================================================================================

ACTOR_MAIL_HANDLE(PlayerBase, E_MCMT_GameCommon, E_MCGCST_ReqExitRegion)
{
        auto r = GetRegion();
        Back2MainCity(r ? static_cast<ERegionType>(r->GetType()) : E_RT_None);
        return nullptr;
}

ACTOR_MAIL_HANDLE(PlayerBase, E_MCMT_GameCommon, E_MCGCST_LoadFinish, MsgLoadFinish)
{
        PLAYER_DLOG_INFO(GetID(), "玩家[{}] 收到 LoadFinish!!!", GetID());
        msg->set_player_guid(GetID());
        Send(GetRegion(), E_MCMT_GameCommon, E_MCGCST_LoadFinish, msg);
        return nullptr;
}

ACTOR_MAIL_HANDLE(PlayerBase, E_MIMT_GameCommon, E_MIGCST_RegionDestroy, MailRegionDestroy)
{
        PLAYER_DLOG_INFO(GetID(), "玩家[{}] 收到场景管理器发送的场景释放消息!!!", GetID());
        auto r = GetRegion();
        if (r && E_RT_MainCity != r->GetType()) // 异常情况下，可能收到重复消息。
        {
                ERegionType oldRegionType = static_cast<ERegionType>(r->GetType());
                SetRegion(nullptr);
                Back2MainCity(0 != msg->region_id() ? oldRegionType : E_RT_None);
        }
        return nullptr;
}

ACTOR_MAIL_HANDLE(PlayerBase, E_MIMT_GameCommon, E_MIGCST_RegionKickout)
{
        // TODO: 发送原因给客户端。

        auto r = GetRegion();
        if (r && E_RT_MainCity != r->GetType())
        {
                auto msg = std::make_shared<MsgClientKickout>();
                msg->set_error_type(E_CET_Success);
                Send2Client(E_MCMT_ClientCommon, E_MCCCST_Kickout, msg);

                ERegionType oldRegionType = static_cast<ERegionType>(r->GetType());
                SetRegion(nullptr);
                Back2MainCity(oldRegionType);
        }
        return nullptr;
}

}; // end of namespace nl::af::impl

// vim: fenc=utf8:expandtab:ts=8
