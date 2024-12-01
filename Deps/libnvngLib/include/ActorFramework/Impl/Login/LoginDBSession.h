#pragma once

class LoginDBSession : public ActorAgentSession<TcpSession, ActorAgent, LoginDBSession, false, Compress::ECompressType::E_CT_Zstd>
{
public :
	typedef ActorAgentSession<TcpSession, ActorAgent, LoginDBSession, false, Compress::ECompressType::E_CT_Zstd> SuperType;
public :
        LoginDBSession(typename SuperType::SocketType&& s)
                : SuperType(std::move(s))
        {
        }

        ~LoginDBSession() override;

public :
	void OnConnect() override;
	void OnClose(int32_t reasonType) override;
        static bool InitOnce();

        static void MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef);
        static void InitCheckFinishTimer();
        static std::string scPriorityTaskKey;

	EXTERN_MSG_HANDLE();
        DECLARE_SHARED_FROM_THIS(LoginDBSession);
};

// vim: fenc=utf8:expandtab:ts=8
