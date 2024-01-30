#pragma once

class LobbyGMSession : public ActorAgentSession<TcpSession, ActorAgent, LobbyGMSession, false, Compress::ECompressType::E_CT_Zstd>
{
public :
	typedef ActorAgentSession<TcpSession, ActorAgent, LobbyGMSession, false, Compress::ECompressType::E_CT_Zstd> SuperType;
public :
        LobbyGMSession(typename SuperType::SocketType&& s)
                : SuperType(std::move(s))
        {
        }

        ~LobbyGMSession() override;

public :
	void OnConnect() override;
	void OnClose(int32_t reasonType) override;

        static void MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef);

	EXTERN_MSG_HANDLE();
        DECLARE_SHARED_FROM_THIS(LobbyGMSession);
};

// vim: fenc=utf8:expandtab:ts=8
