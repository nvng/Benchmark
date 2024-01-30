#pragma once

class DBLoginSession : public ActorAgentSession<TcpSession, ActorAgent, DBLoginSession, true>
{
public :
	typedef ActorAgentSession<TcpSession, ActorAgent, DBLoginSession, true> SuperType;
public :
        DBLoginSession(typename SuperType::SocketType&& s)
                : SuperType(std::move(s))
        {
        }

	void OnConnect() override;
	void OnClose(int32_t reasonType) override;

        static void MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef);

        DECLARE_SHARED_FROM_THIS(DBLoginSession);
        EXTERN_MSG_HANDLE();
};

// vim: fenc=utf8:expandtab:ts=8
