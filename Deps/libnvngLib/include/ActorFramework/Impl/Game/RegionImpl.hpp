#pragma once

#include "NetMgrImpl.h"
#include "RegionMgr.h"

enum ERegionStateEventCommonType
{
        E_RSECT_None = 0,
        E_RSECT_OnFighterEnter,
        E_RSECT_OnFighterExit,
	E_RSECT_OverTime,
        ERegionStateEventCommonType_ARRAYSIZE,
};

template <typename _Iy, typename _Fy, typename _Cy, typename _Py, typename _FEPy, typename _RSIy>
class RegionImpl : public ActorImpl<RegionImpl<_Iy, _Fy, _Cy, _Py, _FEPy, _RSIy>, RegionMgr>
{
	typedef ActorImpl<RegionImpl<_Iy, _Fy, _Cy, _Py, _FEPy, _RSIy>, RegionMgr> SuperType;
public :
        typedef _Iy ImplType;
        typedef _Fy FighterType;
        typedef std::shared_ptr<FighterType> FighterPtr;
        typedef _Cy CfgType;
        typedef _Py RegionInfoMessageType;
        typedef _FEPy FighterEnterMessageType;
        typedef _RSIy RegionStateInfoMessageType;
        typedef RegionImpl<ImplType, FighterType, CfgType, RegionInfoMessageType, FighterEnterMessageType, RegionStateInfoMessageType> ThisType;

public :
        RegionImpl(const std::shared_ptr<MailRegionCreateInfo>& cInfo,
                   const std::shared_ptr<CfgType>& cfg,
                   const GameMgrSession::ActorAgentTypePtr& agent,
                   int64_t queueSize = 32)
                : SuperType(cInfo->region_guid(), queueSize)
                  , _regionMgr(agent)
                  , _cfg(cfg)
        {
        }

	~RegionImpl() override
        {
        }

        virtual void OnDestroy() { }
        virtual void Reset() { }

	virtual FighterPtr CreateFighter(const GameLobbySession::ActorAgentTypePtr& p,
                                         const GameGateSession::ActorAgentTypePtr& cli,
                                         const MailReqEnterRegion::MsgReqFighterInfo& info) = 0;

        virtual bool LoadFighter(const FighterPtr& f)
        { return f ? _loadFighterList.emplace(f->GetID(), f).second : false; }

        virtual bool RegisterFighter(const FighterPtr& f)
        {
                auto it = _loadFighterList.find(f->GetID());
                if (_loadFighterList.end() == it)
                {
                        it = _fighterList.find(f->GetID());
                        if (_fighterList.end() != it)
                        {
                                // REGION_DLOG_WARN(GetID(), "RegisterFighter 时，玩家已经 RegisterFighter 过了!!!");
                                OnFighterEnter(f);
                        }
                        return false;
                }

                _loadFighterList.erase(it);
                bool ret = _fighterList.emplace(f->GetID(), f).second;
                if (ret)
                        OnFighterEnter(f);
                return ret;
        }

	virtual bool UnRegisterFighter(uint64_t id)
	{
		FighterPtr f = GetFighter(id);

		_loadFighterList.erase(id);
		_fighterList.erase(id);

                // 保证已经被删除。
		if (f)
			OnFighterExit(f);
		return true;
	}

        FORCE_INLINE FighterPtr GetFighter(uint64_t id) const
        {
                auto ret = GetFighterInternal(id);
                if (!ret)
                        ret = GetLoadFighter(id);
                return ret;
        }

	FORCE_INLINE FighterPtr GetFighterInternal(uint64_t id) const
	{
		auto it = _fighterList.find(id);
		return _fighterList.end() != it ? it->second : nullptr;
	}

        FORCE_INLINE FighterPtr GetLoadFighter(uint64_t id) const
        {
		auto it = _loadFighterList.find(id);
                return _loadFighterList.end() != it ? it->second : nullptr;
        }

        FORCE_INLINE bool DirectEnter(const FighterPtr& f)
        {
                bool ret = _fighterList.emplace(f->GetID(), f).second;
                if (ret)
                        OnFighterEnter(f);
                return ret;
        }

	virtual bool CanEnter(const FighterPtr& f) { return true; }
        virtual void OnFighterEnter(const FighterPtr& f)
        {
                f->MarkClientConnect();

                // TODO: 有重复添加可能!!!
                auto p = std::dynamic_pointer_cast<GameLobbySession::ActorAgentType>(f->GetPlayer());
                if (p)
                        _playerMultiCastWapper.Add(p->GetID(), p->GetSession());

                if (!f->IsRobot())
                {
                        auto cli = std::dynamic_pointer_cast<GameGateSession::ActorAgentType>(f->GetClient());
                        if (cli)
                                _clientMultiCastWapper.Add(cli->GetID(), cli->GetSession());
                        else
                                _clientMultiCastWapper.AddRobot(f);
                }

                auto msgRegionInfo = std::make_shared<RegionInfoMessageType>();
                PackRegionInfo(*msgRegionInfo, 1);
                f->Send2Client(E_MCMT_GameCommon, E_MCGCST_RegionInfo, msgRegionInfo);

                auto sendMsg = std::make_shared<FighterEnterMessageType>();
                sendMsg->set_region_type(static_cast<ERegionType>(GetType()));
                sendMsg->set_region_guid(SuperType::GetID());
                f->PackFighterInfo(*(sendMsg->mutable_fighter_info()));
                Send2Client(E_MCMT_GameCommon, E_MCGCST_OnFighterEnter, sendMsg, f->GetID());

                OnEvent(E_RSECT_OnFighterEnter);
        }

	virtual void OnFighterExit(const FighterPtr& f)
        {
                f->MarkClientDisconnect();

                auto sendMsg = std::make_shared<MsgFighterExit>();
                sendMsg->set_region_type(static_cast<ERegionType>(GetType()));
                sendMsg->set_region_guid(SuperType::GetID());
                sendMsg->set_fighter_guid(f->GetID());
                Send2Client(E_MCMT_GameCommon, E_MCGCST_OnFighterExit, sendMsg);

                _playerMultiCastWapper.Remove(f->GetID());
                if (!f->IsRobot())
                        _clientMultiCastWapper.Remove(f->GetID());
                else
                        _clientMultiCastWapper.RemoveRobot(f);

                OnEvent(E_RSECT_OnFighterExit);
        }

        virtual void PackRegionInfo(RegionInfoMessageType& msg, int64_t flag=0)
        {
                msg.set_region_type(static_cast<ERegionType>(GetType()));
                msg.set_region_guid(SuperType::GetID());

                PackCurRegionStateInfo(*(msg.mutable_state_info()));
                ForeachFighter([&msg](const FighterPtr& f) {
                        f->PackFighterInfo(*(msg.add_fighter_list()));
                });
        }

        virtual FighterPtr RandFighter()
        {
                auto it = _fighterList.begin();
                std::advance(it, RandInRange(0, _fighterList.size()));
                return it->second;
        }
        virtual FighterPtr NextFighter(const FighterPtr& f)
        {
                auto it = _fighterList.find(f->GetID());
                return _fighterList.end() == ++it ? _fighterList.begin()->second : it->second;
        }

        void Kickout(const FighterPtr& f, EInternalErrorType errorType)
        {
                auto mail = std::make_shared<MailRegionKickout>();
                mail->set_error_type(errorType);
                mail->set_region_guid(SuperType::GetID());
                mail->set_fighter_guid(f->GetID());
                SuperType::Send(_regionMgr, E_MIMT_GameCommon, E_MIGCST_RegionKickout, mail);
        }

        virtual void OnKickout(const FighterPtr& f) { }
        virtual void OnFighterDisconnect(const FighterPtr& f)
        {
                f->MarkClientDisconnect();
                if (!f->IsRobot())
                        _clientMultiCastWapper.Remove(f->GetID());
                else
                        _clientMultiCastWapper.RemoveRobot(f);
        }

	virtual bool CanExit(const FighterPtr& f) const { return true; }
	virtual void OnReconnect(const IActorPtr& agent) { }
        virtual void OnFighterReconnect(const FighterPtr& f) { f->MarkClientConnect(); }

	FORCE_INLINE void ForeachFighter(const auto& cb)
	{
		for (auto& val : _fighterList)
			cb(val.second);
	}

        FORCE_INLINE int64_t GetFighterCnt() { return _fighterList.size(); }
        FORCE_INLINE int64_t GetAllFighterCnt() { return _fighterList.size() + _loadFighterList.size(); }

	FORCE_INLINE void ForeachAllFighter(const auto& cb)
	{
		ForeachFighter(cb);
		for (auto& val : _loadFighterList)
			cb(val.second);
	}

        virtual void Send2Player(uint64_t mainType, uint64_t subType, const ActorMailDataPtr& msg, uint64_t except=0)
        { _playerMultiCastWapper.MultiCast(SuperType::GetID(), mainType, subType, msg, except); }

        virtual void Send2Client(uint64_t mainType, uint64_t subType, const ActorMailDataPtr& msg, uint64_t except=0)
        { _clientMultiCastWapper.MultiCast(SuperType::GetID(), mainType, subType, msg, except); }

public :
        virtual bool CheckStart() { return false; }
        virtual bool CheckEnd() { return false; }

        virtual std::shared_ptr<MailRegionDestroyInfo> PackRegionDestroyInfo()
        {
                auto ret = std::make_shared<MailRegionDestroyInfo>();
                ret->set_region_guid(SuperType::GetID());
                return ret;
        }

        virtual void Destroy()
        {
                REGION_DLOG_WARN(SuperType::GetID(), "场景 Destroy region[{}]", SuperType::GetID());
                SuperType::Send(_regionMgr, E_MIMT_GameCommon, E_MIGCST_RegionDestroy, PackRegionDestroyInfo());

                std::weak_ptr<ThisType> weakThis = shared_from_this();
                _destroyTimer.Start(weakThis, 60, [weakThis]() {
                        auto thisPtr = weakThis.lock();
                        if (thisPtr)
                        {
                                REGION_LOG_WARN(thisPtr->GetID(),
                                                "region[{}] type[{}] 发送请求 release 消息!!!",
                                                thisPtr->GetID(), thisPtr->GetType());
                                thisPtr->Destroy();
                        }
                });
        }

public :
        FORCE_INLINE int64_t GetStateInterval(int stateType)
        {
                return (stateType < 0 || _cfg->region_state_time_limit_list_size() <= stateType)
                        ? _cfg->region_state_time_limit_list(_cfg->region_state_time_limit_list_size() - 1)
                        : _cfg->region_state_time_limit_list(stateType);
        }

        virtual void UpdateRegionState(int curStateType, double curStateEndTime=0.0, bool sync=true)
        {
                _curRegionStateEndTime = curStateEndTime;
                _curRegionStateType = curStateType;

                if (sync)
                {
                        auto sendMsg = std::make_shared<RegionStateInfoMessageType>();
                        PackCurRegionStateInfo(*(sendMsg.get()));
                        Send2Client(E_MCMT_GameCommon, E_MCGCST_UpdateRegionStateInfo, sendMsg);
                        // REGION_LOG_INFO(GetID(),
                        //                 "region[{}] E_MCGCST_UpdateRegionStateInfo state[{}]",
                        //                 GetID(), _curRegionStateType);
                }
        }

        virtual void PackCurRegionStateInfo(RegionStateInfoMessageType& msg)
        {
                msg.set_region_type(static_cast<ERegionType>(GetType()));
                msg.set_cur_region_state_type(_curRegionStateType);
                msg.set_cur_region_state_end_time(_curRegionStateEndTime * 1000);
        }

        double _curRegionStateEndTime = 0.0;
private :
        int64_t _curRegionStateType = -1;

public :
        virtual void OnEvent(int64_t eventType) = 0;

protected :
	std::unordered_map<uint64_t, FighterPtr> _loadFighterList;
	std::unordered_map<uint64_t, FighterPtr> _fighterList;

public :
        SteadyTimer _destroyTimer;
        GameMgrSession::ActorAgentTypePtr _regionMgr;
        MultiCastWapper<GameLobbySession> _playerMultiCastWapper;
        MultiCastWapperImpl<GameGateSession, FighterType> _clientMultiCastWapper;

public :
	uint64_t GetType() const override { return _cfg->region_type(); }
	FORCE_INLINE uint64_t GetMainType() const { return _cfg->region_type() / 100; }
	FORCE_INLINE uint64_t GetSubType() const  { return _cfg->region_type() % 100; }
protected :
	const std::shared_ptr<CfgType> _cfg;

public :
        FORCE_INLINE int64_t GetEffectiveGuid() const { return _effectiveGuid; }
        FORCE_INLINE void IncEffectiveGuid() { ++_effectiveGuid; }
        FORCE_INLINE void ResetEffectiveGuid() { _effectiveGuid = 0; }
        FORCE_INLINE int64_t GetEffectiveGroupIdx() const { return _effectiveGroupIdx; }
        FORCE_INLINE void IncEffectiveGroupIdx() { ++_effectiveGroupIdx; }
        FORCE_INLINE void ResetEffectiveGroupIdx() { _effectiveGroupIdx = 0; }

private :
        int32_t _effectiveGroupIdx = 0;
        int32_t _effectiveGuid = 0;
public :
        bool _inGroup = false;

protected :
	friend class ActorImpl<RegionImpl<_Iy, _Fy, _Cy, _Py, _FEPy, _RSIy>, RegionMgr>;
        DECLARE_SHARED_FROM_THIS(ThisType);

protected :
        virtual ActorMailDataPtr OnMailHandleLocalDisconnect(const IActorPtr& from)
        {
                return nullptr;
        }

        virtual ActorMailDataPtr OnMailHandleLocalReconnect(const IActorPtr& from, bool remoteCrash)
        {
                auto regionMgrAgent = std::dynamic_pointer_cast<GameMgrSession::ActorAgentType>(from);
                if (regionMgrAgent)
                {
                        if (remoteCrash)
                        {
                                auto m = std::make_shared<MailRegionRelationInfo>();
                                m->set_id(SuperType::GetID());
                                m->set_region_type(static_cast<ERegionType>(GetType()));
                                m->set_requestor_id(from->GetID());
                                m->set_game_sid(GetApp()->GetSID());
                                ForeachAllFighter([m](const auto& f) {
                                        auto player = f->GetPlayer();
                                        if (player)
                                        {
                                                auto fighterInfo = m->add_fighter_list();
                                                fighterInfo->set_id(f->GetID());
                                                fighterInfo->set_lobby_sid(player->GetSID());
                                        }
                                });

                                SuperType::Send(from, E_MIMT_Internal, E_MIIST_SyncRelation, m);
                        }
                }
                return nullptr;
        }

        virtual ActorMailDataPtr OnMailHandleReconnect(const IActorPtr& from)
        {
                return nullptr;
        }

        virtual ActorMailDataPtr OnMailHandleClientDisconnect(const IActorPtr& from)
        {
                if (from)
                {
                        REGION_DLOG_INFO(SuperType::GetID(),
                                         "玩家[{}] 请求断开连接，type[{}] id[{}]",
                                         from->GetID(), GetType(), SuperType::GetID());
                        auto f = GetFighter(from->GetID());
                        if (f)
                                OnFighterDisconnect(f);
                }

                auto ret = std::make_shared<MsgResult>();
                ret->set_error_type(E_CET_Success);
                return ret;
        }

        // game lobby ses 未断开，其它原因导致重连。
        virtual ActorMailDataPtr OnMailHandleClientReconnect(const IActorPtr& from, const std::shared_ptr<MailFighterReconnect>& msg)
        {
                do {
                        auto f = GetFighter(msg->fighter_guid());
                        if (!f || f->GetID() != from->GetID())
                        {
                                msg->set_error_type(E_IET_FighterNotFound);
                                break;
                        }

                        auto client = f->GetClient();
                        if (!client)
                        {
                                msg->set_error_type(E_IET_FighterClientError);
                                break;
                        }

                        // 底层会自动绑定新 ses。
                        // 说明用户从另一个网关连接过来了。
                        auto oldGateSes = client->GetSession();
                        if (!oldGateSes || oldGateSes->GetSID() != msg->gate_sid())
                        {
                                auto gateSes = NetMgrImpl::GetInstance()->GetGateSession(msg->gate_sid());
                                if (gateSes)
                                {
                                        client->SetSession(gateSes);
                                        gateSes->AddAgent(client);
                                }
                                else
                                {
                                        msg->set_error_type(E_IET_GateNotFound);
                                        break;
                                }
                        }

                        REGION_DLOG_INFO(SuperType::GetID(),
                                         "玩家[{}] 请求Reconnect，type[{}] id[{}]",
                                         from->GetID(), GetType(), SuperType::GetID());
                        OnFighterReconnect(f);
                        msg->set_error_type(E_IET_Success);
                } while (0);

                msg->set_region_type(static_cast<ERegionType>(GetType()));
                msg->set_game_sid(GetApp()->GetSID());
                return msg;
        }


        virtual ActorMailDataPtr OnMailHandleRegionDestroy()
        {
                _destroyTimer.Stop();
                OnDestroy();
                return nullptr;
        }

        virtual ActorMailDataPtr OnMailHandleRegionKickout(const std::shared_ptr<MailRegionKickout>& msg)
        {
                auto retMsg = std::make_shared<MailResult>();
                retMsg->set_error_type(E_IET_Success);

                auto f = GetFighter(msg->fighter_guid());
                if (f)
                        OnKickout(f);
                UnRegisterFighter(msg->fighter_guid());

                return retMsg;
        }

        virtual ActorMailDataPtr OnMailHandleReqEnterRegion(const std::shared_ptr<MailReqEnterRegion>& msg)
        {
                auto retMsg = std::make_shared<MailReqEnterRegionRet>();
                retMsg->set_error_type(E_IET_Success);
                retMsg->set_game_sid(GetApp()->GetSID());
                retMsg->set_region_guid(SuperType::GetID());

                for (auto& fInfo : msg->fighter_list())
                {
                        REGION_DLOG_INFO(SuperType::GetID(),
                                         "玩家[{}] 请求进入场景 type[{}] id[{}]",
                                         fInfo.player_guid(), msg->region_type(), msg->region_guid());

                        auto gateSes = NetMgrImpl::GetInstance()->GetGateSession(fInfo.gate_sid());
                        if (!gateSes)
                        {
                                REGION_LOG_WARN(SuperType::GetID(),
                                                "玩家[{}] 请求进入 region[{}] 时，gate ses not found!!!",
                                                fInfo.player_guid(), SuperType::GetID());

                                retMsg->set_error_type(E_IET_GateSesAlreadyRelease);
                                break;
                        }

                        auto lobbySes = NetMgrImpl::GetInstance()->GetLobbySession(fInfo.lobby_sid());
                        if (!lobbySes)
                        {
                                REGION_LOG_WARN(SuperType::GetID(),
                                                "玩家[{}] 请求进入 region[{}] 时，lobby ses not found!!!",
                                                fInfo.player_guid(), SuperType::GetID());

                                retMsg->set_error_type(E_IET_LobbySesAlreadyRelease);
                                break;
                        }

                        auto thisPtr = shared_from_this();
                        auto clientAgent = std::make_shared<GameGateSession::ActorAgentType>(fInfo.player_guid(), gateSes);
                        clientAgent->BindActor(thisPtr);
                        gateSes->AddAgent(clientAgent);

                        auto playerAgent = std::make_shared<GameLobbySession::ActorAgentType>(fInfo.player_guid(), lobbySes);
                        playerAgent->BindActor(thisPtr);
                        lobbySes->AddAgent(playerAgent);

                        auto f = CreateFighter(playerAgent, clientAgent, fInfo);
                        if (!LoadFighter(f))
                        {
                                REGION_LOG_WARN(SuperType::GetID(),
                                                "玩家[{}] 请求进入 region[{}] 时，load fighter error!!!",
                                                fInfo.player_guid(), SuperType::GetID());
                                retMsg->set_error_type(E_IET_RegionLoadFighterFail);
                                break;
                        }
                }

                return retMsg;
        }

        virtual ActorMailDataPtr OnMailHandleLoadFinish(const IActorPtr& from, const std::shared_ptr<MsgLoadFinish>& msg)
        {
                if (!from)
                {
#ifndef ____BENCHMARK____
                        REGION_LOG_WARN(SuperType::GetID(),
                                         "玩家[{}] 发送 LoadFinish 时，region[{}] from is nullptr !!!",
                                         msg->player_guid(), SuperType::GetID());
#endif
                        return nullptr;
                }

                if (msg->region_type() != static_cast<ERegionType>(GetType()))
                {
                        REGION_DLOG_INFO(SuperType::GetID(),
                                         "region[{}] 收到玩家[{}] LoadFinish消息出错!!! regionType[{}] msgRegionType[{}]",
                                         SuperType::GetID(), from->GetID(), GetType(), msg->region_type());
                        return nullptr;
                }

                REGION_DLOG_INFO(SuperType::GetID(),
                                 "region[{}] 收到玩家[{}] player_guid[{}] LoadFinish消息!!!",
                                 SuperType::GetID(), from->GetID(), msg->player_guid());

                auto f = GetFighter(from->GetID());
                if (!f)
                {
                        REGION_LOG_WARN(SuperType::GetID(),
                                         "玩家发送 LoadFinish 时，region[{}] fighter 未找到!!!",
                                         from->GetID(), SuperType::GetID());
                        return nullptr;
                }

                if (!RegisterFighter(f))
                {
                        // REGION_LOG_WARN(GetID(), "玩家发送 LoadFinish 时，region[{}] RegisterFighter 失败!!!", from->GetID(), GetID());
                        return nullptr;
                }

                return nullptr;
        }

        virtual ActorMailDataPtr OnMailHandleReqExit(const IActorPtr& from, const std::shared_ptr<MailReqExit>& msg)
        {
                auto ret = std::make_shared<MailResult>();
                ret->set_error_type(E_IET_Success);
                if (!from)
                {
                        REGION_LOG_WARN(SuperType::GetID(),
                                         "requestorID[{}] 玩家[{}] 请求退出 region[{}] type[{}]时，from is nullptr!!!",
                                         from->GetID(), msg->player_guid(), SuperType::GetID(), GetType());
                        ret->set_error_type(E_IET_FromIsNull);
                        return ret;
                }

                REGION_DLOG_INFO(SuperType::GetID(),
                                 "requestorID[{}] 玩家[{}] 请求退出场景 type[{}] id[{}]",
                                 from->GetID(), msg->player_guid(), GetType(), SuperType::GetID());

                auto f = GetFighter(msg->player_guid());
                if (f)
                {
                        if (!CanExit(f))
                        {
                                REGION_LOG_WARN(SuperType::GetID(),
                                                 "requestorID[{}] 玩家[{}] 请求退出 region[{}] type[{}]时，can not exit!!!",
                                                 f->GetID(), msg->player_guid(), SuperType::GetID(), GetType());
                                ret->set_error_type(E_IET_RegionCannotExit);
                                return ret;
                        }
                        UnRegisterFighter(f->GetID());
                }

                return ret;
        }

        virtual ActorMailDataPtr OnMailHandleSyncPlayerInfo2Region(const IActorPtr& from, const std::shared_ptr<MailSyncPlayerInfo2Region>& msg)
        {
                auto retMsg = std::make_shared<MailResult>();
                retMsg->set_error_type(E_IET_Fail);
                do
                {
                        if (!from)
                        {
                                REGION_LOG_WARN(SuperType::GetID(),
                                                 "大厅同步玩家信息时，region[{}] from is nullptr !!!",
                                                 SuperType::GetID());
                                retMsg->set_error_type(E_IET_FromIsNull);
                                break;
                        }

                        auto f = GetLoadFighter(from->GetID());
                        if (!f)
                        {
                                REGION_LOG_WARN(SuperType::GetID(),
                                                 "大厅同步玩家[{}] 信息时，region[{}] fighter 未找到!!!",
                                                 from->GetID(), SuperType::GetID());
                                retMsg->set_error_type(E_IET_FighterNotFound);
                                break;
                        }

                        if (!f->InitFromLobby(*msg))
                        {
                                REGION_LOG_WARN(SuperType::GetID(),
                                                 "大厅同步玩家[{}]信息时，region[{}] init from lobby error!!!",
                                                 from->GetID(), SuperType::GetID());
                                retMsg->set_error_type(E_IET_FighterInitFromLobbyFail);
                                break;
                        }

                        retMsg->set_error_type(E_IET_Success);
                } while (0);

                return retMsg;
        }
};

#define REGION_GAME_COMMON_HANDLE(rt) \
        ACTOR_MAIL_HANDLE(rt, E_MIMT_Local, E_MILST_Disconnect) { return OnMailHandleLocalDisconnect(from); } \
        ACTOR_MAIL_HANDLE(rt, E_MIMT_Local, E_MILST_Reconnect, MailResult) { return OnMailHandleLocalReconnect(from, E_IET_RemoteCrash == msg->error_type()); } \
        ACTOR_MAIL_HANDLE(rt, E_MIMT_Internal, E_MIIST_Reconnect) { return OnMailHandleReconnect(from); } \
        ACTOR_MAIL_HANDLE(rt, E_MCMT_ClientCommon, E_MCCCST_Disconnect) { return OnMailHandleClientDisconnect(from); } \
        ACTOR_MAIL_HANDLE(rt, E_MCMT_ClientCommon, E_MCCCST_Reconnect, MailFighterReconnect) { return OnMailHandleClientReconnect(from, msg); } \
        ACTOR_MAIL_HANDLE(rt, E_MIMT_GameCommon, E_MIGCST_RegionDestroy) { return OnMailHandleRegionDestroy(); } \
        ACTOR_MAIL_HANDLE(rt, E_MIMT_GameCommon, E_MIGCST_RegionKickout, MailRegionKickout) { return OnMailHandleRegionKickout(msg); } \
        ACTOR_MAIL_HANDLE(rt, E_MCMT_GameCommon, E_MCGCST_ReqEnterRegion, MailReqEnterRegion) { return OnMailHandleReqEnterRegion(msg); } \
        ACTOR_MAIL_HANDLE(rt, E_MCMT_GameCommon, E_MCGCST_LoadFinish, MsgLoadFinish) { return OnMailHandleLoadFinish(from, msg); } \
        ACTOR_MAIL_HANDLE(rt, E_MCMT_GameCommon, E_MCGCST_ReqExitRegion, MailReqExit) { return OnMailHandleReqExit(from, msg); } \
        ACTOR_MAIL_HANDLE(rt, E_MIMT_GameCommon, E_MIGCST_SyncPlayerInfo2Region, MailSyncPlayerInfo2Region) { return OnMailHandleSyncPlayerInfo2Region(from, msg); }

// vim: fenc=utf8:expandtab:ts=8
