#pragma once

#include "ActorFramework/ActorAgent.hpp"

namespace nl::af::impl {

template <typename _Ty>
class GatePlayerActorAgent : public ActorAgent<_Ty>
{
        typedef _Ty SessionType;
        typedef ActorAgent<_Ty> SuperType;
public :
        GatePlayerActorAgent(uint32_t id, const std::shared_ptr<SessionType>& ses)
                : SuperType(id, ses)
        {
        }

        ~GatePlayerActorAgent() override
        {
        }

        FORCE_INLINE static int64_t GenUniqueID(const std::shared_ptr<SessionType>& ses, uint64_t remoteID)
        { return ses ? ses->GetID() << 32 | remoteID : 0; }

        FORCE_INLINE int64_t GetUniqueID()
        { return GenUniqueID(SuperType::GetSession(), SuperType::GetID()); }

        bool CallPushInternal(const IActorPtr& from,
                              uint64_t mainType,
                              uint64_t subType,
                              const ActorMailDataPtr& msg,
                              uint16_t guid) override
        {
                assert(false);
                return true;
        }
};

class LobbyGateSession : public ActorAgentSession<TcpSession, GatePlayerActorAgent, LobbyGateSession, true, Compress::ECompressType::E_CT_ZLib>
{
public :
        typedef ActorAgentSession<TcpSession, GatePlayerActorAgent, LobbyGateSession, true, Compress::ECompressType::E_CT_ZLib> SuperType;
public :
        LobbyGateSession(typename SuperType::SocketType&& s)
                : SuperType(std::move(s))
        {
                DelAutoRebind();
        }

	void OnConnect() override;
	void OnClose(int32_t reasonType) override;
        void OnRecv(ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef) override;

        static void MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef);
        static std::string scPriorityTaskKey;

        EXTERN_MSG_HANDLE();
        DECLARE_SHARED_FROM_THIS(LobbyGateSession);
};

};

// vim: fenc=utf8:expandtab:ts=8:noma
