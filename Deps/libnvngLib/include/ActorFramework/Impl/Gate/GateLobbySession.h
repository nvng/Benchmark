#pragma once

#include "TcpSessionGate.hpp"

namespace nl::af::impl {

class GateLobbySession : public TcpSessionGate<GateLobbySession, MsgActorAgentHeaderType, Compress::ECompressType::E_CT_ZLib>
{
        typedef TcpSessionGate<GateLobbySession, MsgActorAgentHeaderType, Compress::ECompressType::E_CT_ZLib> SuperType;
public :
        GateLobbySession(SuperType::SocketType&& s)
                : SuperType(std::move(s))
        {
        }

        ~GateLobbySession() override;

	void OnConnect() override;
	void OnClose(int32_t reasonType) override;
        void OnRecv(typename SuperType::BuffTypePtr::element_type* buf, const typename SuperType::BuffTypePtr& bufRef) override;
	static bool InitOnce();

        static void MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef);
	static const std::string scPriorityTaskKey;

        DECLARE_SHARED_FROM_THIS(GateLobbySession);
	EXTERN_MSG_HANDLE();
};

}; // end of namespace nl::af::impl

// vim: fenc=utf8:expandtab:ts=8
