#pragma once

class LoginGateSession : public ActorAgentSession<TcpSession, ActorAgent, LoginGateSession, true, Compress::E_CT_ZLib>
{
public :
	typedef ActorAgentSession<TcpSession, ActorAgent, LoginGateSession, true, Compress::E_CT_ZLib> SuperType;
public :
        LoginGateSession(typename SuperType::SocketType&& s)
                : SuperType(std::move(s))
        {
        }

	void OnConnect() override;
	void OnClose(int32_t reasonType) override;

        static void MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef);

	EXTERN_MSG_HANDLE();
        DECLARE_SHARED_FROM_THIS(LoginGateSession);
};

// vim: fenc=utf8:expandtab:ts=8
