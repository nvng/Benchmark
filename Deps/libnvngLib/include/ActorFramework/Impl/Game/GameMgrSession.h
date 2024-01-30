#pragma once

template <typename _Ty>
class GameMgrAgent : public ActorAgent<_Ty>
{
        typedef _Ty SessionType;
        typedef ActorAgent<_Ty> SuperType;
public :
        GameMgrAgent(uint32_t id, const std::shared_ptr<SessionType>& ses)
                : SuperType(id, ses)
        {
                // DLOG_INFO("GameMgrAgent::GameMgrAgent id:{} local:{}", SuperType::GetID(), SuperType::GetLocalID());
        }

        ~GameMgrAgent() override
        {
                // DLOG_ERROR("GameMgrAgent::~GameMgrAgent id:{} local:{}", SuperType::GetID(), SuperType::GetLocalID());
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

        uint64_t GetType() const override { auto obj = SuperType::GetBindActor(); return obj ? obj->GetType() : 0; }
};

class GameMgrSession : public ActorAgentSession<TcpSession, GameMgrAgent, GameMgrSession, false>
{
public :
        typedef ActorAgentSession<TcpSession, GameMgrAgent, GameMgrSession, false> SuperType;
public :
        GameMgrSession(typename SuperType::SocketType&& s)
                : SuperType(std::move(s))
        {
        }

        ~GameMgrSession() override;

public :
	void OnConnect() override;
	void OnClose(int32_t reasonType) override;

        static void MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef);
        FORCE_INLINE void OnRecvNotFoundAgent(ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
        {
                auto msgHead = *reinterpret_cast<MsgHeaderType*>(buf);
                auto msg = std::make_shared<MailAgentNotFoundResult>();
                msg->set_error_type(E_IET_RegionNotFound);
                msg->set_type(msgHead._type);
                SendPB(msg, E_MIMT_Internal, E_MIIST_Result, MsgHeaderType::E_RMT_Send, msgHead._guid, msgHead._to, msgHead._from);
        }

        void OnRecv(ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef) override
        {
                auto msgHead = *reinterpret_cast<MsgHeaderType*>(buf);
                switch (msgHead._type)
                {
                case MsgHeaderType::MsgTypeMerge<E_MIMT_GameCommon, E_MIGCST_RegionCreate>() :
                case MsgHeaderType::MsgTypeMerge<E_MIMT_GameCommon, E_MIGCST_RegionDestroy>() :
                        SuperType::SuperType::OnRecv(buf, bufRef);
                        break;
                default:
                        SuperType::OnRecv(buf, bufRef);
                        break;
                }
        }

        static std::string scPriorityTaskKey;

	EXTERN_MSG_HANDLE();
        DECLARE_SHARED_FROM_THIS(GameMgrSession);
};

// vim: fenc=utf8:expandtab:ts=8
