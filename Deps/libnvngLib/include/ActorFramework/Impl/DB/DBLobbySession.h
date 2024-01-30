#pragma once

// class DBLobbySession : public ActorAgentSession<TcpSession, ActorAgent, DBLobbySession, true>
class DBLobbySession : public TcpSession<DBLobbySession>
{
public :
	typedef TcpSession<DBLobbySession> SuperType;
public :
        DBLobbySession(typename SuperType::SocketType&& s)
                : SuperType(std::move(s))
        {
        }

	void OnConnect() override;
	void OnClose(int32_t reasonType) override;

        static void MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef);

        DECLARE_SHARED_FROM_THIS(DBLobbySession);
        EXTERN_MSG_HANDLE();
};

// vim: fenc=utf8:expandtab:ts=8
