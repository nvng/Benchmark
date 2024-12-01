#pragma once

// 大厅重新连上后，玩家登录是否需要向游戏管理服请求当前所在游戏信息。
template <typename _Ty>
class LobbyReqAgent : public ActorAgent<_Ty>
{
        typedef ActorAgent<_Ty> SuperType;
public :
        LobbyReqAgent(uint64_t id, const std::shared_ptr<_Ty>& ses)
                : SuperType(id, ses)
        {
                DLOG_INFO("LobbyReqAgent::LobbyReqAgent id:{}", SuperType::GetAgentID());
        }

        ~LobbyReqAgent() override
        {
                DLOG_INFO("LobbyReqAgent::~LobbyReqAgent id:{}", SuperType::GetAgentID());
        }

        bool CallPushInternal(const IActorPtr& from,
                              uint64_t mainType,
                              uint64_t subType,
                              const ActorMailDataPtr& msg,
                              uint16_t guid) override
        {
                assert(false);
                return true;
        }

        void BindActor(const IActorPtr& b) override { SuperType::_bindActor = b; }

public :
        FORCE_INLINE void BindExtra(const IActorPtr& extra) { if (extra) SuperType::_local = extra->GetID();  _extra = extra; }
        FORCE_INLINE IActorPtr GetExtra() { return _extra.lock(); }
private :
        IActorWeakPtr _extra;
};

class GameMgrLobbySession : public ActorAgentSession<TcpSession, LobbyReqAgent, GameMgrLobbySession, true>
{
public :
	typedef ActorAgentSession<TcpSession, LobbyReqAgent, GameMgrLobbySession, true> SuperType;
public :
        GameMgrLobbySession(typename SuperType::SocketType&& s)
                : SuperType(std::move(s))
        {
        }

        ~GameMgrLobbySession() override;
        static bool InitOnce();

public :
	void OnConnect() override;
	void OnClose(int32_t reasonType) override;

        static void MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef);

        FORCE_INLINE void OnRecvCallRet(ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
        {
                auto msgHead = *reinterpret_cast<MsgHeaderType*>(buf);
                auto agent = GetAgent(msgHead._from, msgHead._to);
                if (agent)
                {
                        agent->RemoteCallRet(buf, bufRef);
                }
                else
                {
                        auto msg = std::make_shared<MailAgentNotFoundResult>();
                        msg->set_error_type(E_IET_RegionMgrNotFound);
                        msg->set_type(msgHead._type);
                        SendPB(msg, E_MIMT_Internal, E_MIIST_Result, MsgHeaderType::E_RMT_Send, msgHead._guid, msgHead._to, msgHead._from);
                }
        }

        FORCE_INLINE void OnRecvNotFoundAgent(ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
        {
                auto msgHead = *reinterpret_cast<MsgHeaderType*>(buf);
                switch (msgHead._type)
                {
                case MsgHeaderType::MsgTypeMerge<E_MCMT_QueueCommon, E_MCQCST_ReqQueue>() :
                case MsgHeaderType::MsgTypeMerge<E_MCMT_GameCommon, E_MCGCST_ReqEnterRegion>() :
                        SuperType::SuperType::OnRecv(buf, bufRef);
                        break;
                default :
                        {
                                auto msg = std::make_shared<MailAgentNotFoundResult>();
                                msg->set_error_type(E_IET_RegionMgrNotFound);
                                msg->set_type(msgHead._type);
                                SendPB(msg, E_MIMT_Internal, E_MIIST_Result, MsgHeaderType::E_RMT_Send, msgHead._guid, msgHead._to, msgHead._from);
                        }
                        break;
                }
        }

        void OnRecv(ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef) override
        {
                auto msgHead = *reinterpret_cast<MsgHeaderType*>(buf);
                switch (msgHead._type)
                {
                case MsgHeaderType::MsgTypeMerge<E_MCMT_GameCommon, E_MCGCST_ReqEnterRegion>() :

                case MsgHeaderType::MsgTypeMerge<E_MCMT_QueueCommon, E_MCQCST_ReqQueue>() :
                case MsgHeaderType::MsgTypeMerge<E_MCMT_QueueCommon, E_MCQCST_ExitQueue>() :
                case MsgHeaderType::MsgTypeMerge<E_MCMT_QueueCommon, E_MCQCST_Opt>() :
                case MsgHeaderType::MsgTypeMerge<E_MCMT_QueueCommon, E_MCQCST_ReqQueueList>() :
                        SuperType::SuperType::OnRecv(buf, bufRef);
                        break;
                default:
                        SuperType::OnRecv(buf, bufRef);
                        break;
                }
        }

        static void InitCheckFinishTimer();
        static std::string scPriorityTaskKey;

	EXTERN_MSG_HANDLE();
        DECLARE_SHARED_FROM_THIS(GameMgrLobbySession);
};

// vim: fenc=utf8:expandtab:ts=8
