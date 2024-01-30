#pragma once

#include "RegionAgent.hpp"

class GameMgrGameSession : public ActorAgentSession<TcpSession, RegionAgent, GameMgrGameSession, true>
{
public :
	typedef ActorAgentSession<TcpSession, RegionAgent, GameMgrGameSession, true> SuperType;
public :
        GameMgrGameSession(typename SuperType::SocketType&& s)
                : SuperType(std::move(s))
        {
        }

        ~GameMgrGameSession() override;
        static bool InitOnce();

public :
	void OnConnect() override;
	void OnClose(int32_t reasonType) override;

        static void MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef);

        static void InitCheckFinishTimer();
        static std::string scPriorityTaskKey;

	EXTERN_MSG_HANDLE();
        DECLARE_SHARED_FROM_THIS(GameMgrGameSession);
};

// vim: fenc=utf8:expandtab:ts=8
