#pragma once

#include "FighterAgent.hpp"

class GameLobbySession : public ActorAgentSession<TcpSession, FighterAgent, GameLobbySession, false>
{
public :
        typedef ActorAgentSession<TcpSession, FighterAgent, GameLobbySession, false> SuperType;
public :
        GameLobbySession(typename SuperType::SocketType&& s)
                : SuperType(std::move(s))
        {
        }
        ~GameLobbySession() override;
        static bool InitOnce();

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

        static void InitCheckFinishTimer();
        static std::string scPriorityTaskKey;

	EXTERN_MSG_HANDLE();
        DECLARE_SHARED_FROM_THIS(GameLobbySession);
};

// vim: fenc=utf8:expandtab:ts=8
