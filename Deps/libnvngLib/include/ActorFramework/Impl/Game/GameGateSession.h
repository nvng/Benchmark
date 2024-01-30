#pragma once

#include "FighterAgent.hpp"

class GameGateSession : public ActorAgentSession<TcpSession, FighterAgent, GameGateSession, true, Compress::ECompressType::E_CT_ZLib>
{
public :
        typedef ActorAgentSession<TcpSession, FighterAgent, GameGateSession, true, Compress::ECompressType::E_CT_ZLib> SuperType;
public :
        GameGateSession(typename SuperType::SocketType&& s)
                : SuperType(std::move(s))
        {
        }

        ~GameGateSession() override;

public :
	void OnConnect() override;
	void OnClose(int32_t reasonType) override;

        static void MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef);
        FORCE_INLINE void OnRecvNotFoundAgent(ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
        {
                auto msgHead = *reinterpret_cast<MsgHeaderType*>(buf);
                auto msg = std::make_shared<MsgResult>();
                msg->set_error_type(E_CET_RegionNotFound);
                SendPB(msg, E_MCMT_ClientCommon, E_MCCCST_Result, MsgHeaderType::E_RMT_Send, msgHead._guid, msgHead._to, msgHead._from);
        }

	EXTERN_MSG_HANDLE();
        DECLARE_SHARED_FROM_THIS(GameGateSession);
};

// vim: fenc=utf8:expandtab:ts=8
