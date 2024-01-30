#pragma once

class GMLobbySession : public ActorAgentSession<TcpSession, ActorAgent, GMLobbySession, true>
{
public :
	typedef ActorAgentSession<TcpSession, ActorAgent, GMLobbySession, true> SuperType;
public :
        GMLobbySession(typename SuperType::SocketType&& s)
                : SuperType(std::move(s))
        {
        }

	void OnConnect() override;
	void OnClose(int32_t reasonType) override;

        static void MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef);

	EXTERN_MSG_HANDLE();
        DECLARE_SHARED_FROM_THIS(GMLobbySession);
};

// vim: fenc=utf8:expandtab:ts=8
