#pragma once

namespace nl::af::impl {

class GateLoginSession : public TcpSessionGate<GateLoginSession, MsgActorAgentHeaderType, Compress::ECompressType::E_CT_ZLib>
{
        typedef TcpSessionGate<GateLoginSession, MsgActorAgentHeaderType, Compress::ECompressType::E_CT_ZLib> SuperType;
public :
        GateLoginSession(typename SuperType::SocketType&& s)
                : SuperType(std::move(s))
        {
        }

        ~GateLoginSession() override;

	void OnConnect() override;
	void OnClose(int32_t reasonType) override;
        void OnRecv(typename SuperType::BuffTypePtr::element_type* buf, const typename SuperType::BuffTypePtr& bufRef) override;

        static void MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef);
	static const std::string scPriorityTaskKey;
public :
	std::size_t _hashKey = 0;
        std::atomic_int64_t _sid = 0;

        DECLARE_SHARED_FROM_THIS(GateLoginSession);
        EXTERN_MSG_HANDLE();
};

}; // end of namespace nl::af::impl

// vim: fenc=utf8:expandtab:ts=8:noma
