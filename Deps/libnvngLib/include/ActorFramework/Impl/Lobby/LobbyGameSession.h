#pragma once

namespace nl::af::impl {

template <typename _Ty>
class RegionAgent : public ActorAgent<_Ty>
{
        typedef ActorAgent<_Ty> SuperType;
public :
        RegionAgent(uint32_t id, const std::shared_ptr<_Ty>& ses)
                : SuperType(id, ses)
        {
                // DLOG_INFO("RegionAgent::RegionAgent id:{}", SuperType::GetAgentID());
        }

        ~RegionAgent() override
        {
                // DLOG_INFO("RegionAgent::~RegionAgent id:{} localID:{}", SuperType::GetAgentID(), SuperType::GetLocalID());
        }

        uint64_t GetType() const override { return _regionMgr ? _regionMgr->GetType() : E_RT_None; }

        IActorPtr _regionMgr;
};

class LobbyGameSession : public ActorAgentSession<TcpSession, RegionAgent, LobbyGameSession, true>
{
public :
	typedef ActorAgentSession<TcpSession, RegionAgent, LobbyGameSession, true> SuperType;
public :
        LobbyGameSession(typename SuperType::SocketType&& s)
                : SuperType(std::move(s))
        {
        }

        ~LobbyGameSession() override;

        static bool InitOnce();

public :
	void OnConnect() override;
	void OnClose(int32_t reasonType) override;
        static void MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef);
        static void InitCheckFinishTimer();
        static std::string scPriorityTaskKey;
        static std::atomic_int64_t scLoadCnt;

	EXTERN_MSG_HANDLE();
        DECLARE_SHARED_FROM_THIS(LobbyGameSession);
};

typedef std::shared_ptr<LobbyGameSession> LobbyGameSessionPtr;
typedef std::weak_ptr<LobbyGameSession> LobbyGameSessionWeakPtr;

}; // end of namespace nl::af::impl

// vim: fenc=utf8:expandtab:ts=8:noma
