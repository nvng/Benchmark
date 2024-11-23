#pragma once

#if 0
namespace nl::af::impl {

class LobbyDBSession : public ActorAgentSession<TcpSession, ActorAgent, LobbyDBSession, false, Compress::ECompressType::E_CT_Zstd>
{
public :
	typedef ActorAgentSession<TcpSession, ActorAgent, LobbyDBSession, false, Compress::ECompressType::E_CT_Zstd> SuperType;
public :
        LobbyDBSession(typename SuperType::SocketType&& s)
                : SuperType(std::move(s))
        {
        }

        ~LobbyDBSession() override;

public :
	void OnConnect() override;
	void OnClose(int32_t reasonType) override;
        static bool InitOnce();

        static void MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef);
        static void InitCheckFinishTimer();
        static std::string scPriorityTaskKey;

	EXTERN_MSG_HANDLE();
        DECLARE_SHARED_FROM_THIS(LobbyDBSession);
};

}; // end of namespace nl::af::impl;

#endif

// vim: fenc=utf8:expandtab:ts=8
