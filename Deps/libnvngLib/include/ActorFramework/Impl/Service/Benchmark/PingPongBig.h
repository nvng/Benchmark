#pragma once

class PingPongBigActor : public ActorImpl<PingPongBigActor, SpecialActorMgr>
{
        typedef ActorImpl<PingPongBigActor, SpecialActorMgr> SuperType;
public :
        PingPongBigActor()
                : SuperType(SpecialActorMgr::GetInstance()->GenActorID())
        {
        }
        
        EXTERN_ACTOR_MAIL_HANDLE();
};

// #define PING_PONG_BIG_SERVICE_SERVER
// #define PING_PONG_BIG_SERVICE_CLIENT
// #define PING_PONG_BIG_SERVICE_LOCAL

#include "ActorFramework/ServiceExtra.hpp"

struct stPingPongBigTag;
typedef ::nl::net::NetMgrBase<stPingPongBigTag> PingPongBigNetMgr;

template <typename ServiceType, bool IsServer>
class PingPongBigSession : public ServiceSession<ServiceType, IsServer, stPingPongBigTag>
{
        typedef ServiceSession<ServiceType, IsServer, stPingPongBigTag> SuperType;
public :
        typedef typename SuperType::MsgHeaderType MsgHeaderType;
public :
        explicit PingPongBigSession(typename SuperType::SocketType&& s)
                : SuperType(std::move(s))
        {
        }

        void OnRecv(ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef) override
        {
                ++GetApp()->_cnt;
                auto msgHead = *reinterpret_cast<MsgHeaderType*>(buf);
                SuperType::SendBuf(bufRef, buf+sizeof(MsgHeaderType), msgHead._size-sizeof(MsgHeaderType)
                                   , Compress::E_CT_ZLib, msgHead.MainType(), msgHead.SubType()
                                   , MsgHeaderType::E_RMT_Send, 0, 0, 0);
        }
};

DECLARE_SERVICE_BASE_BEGIN(PingPongBig, SessionDistributeMod, PingPongBigSession);

        constexpr static int64_t scSessionCnt = 5;
        constexpr static int64_t scPkgPerSession = 1000 * 100;

private :
        PingPongBigServiceBase() : SuperType("PingPongBigService") { }
        ~PingPongBigServiceBase() override { }

public :
        bool Init() override
        {
                if (!SuperType::Init())
                        return false;

                PingPongBigNetMgr::CreateInstance();
                PingPongBigNetMgr::GetInstance()->Init(4, "pingpong");
#ifdef PING_PONG_BIG_SERVICE_SERVER
                SuperType::Start(60000);
#endif

#ifdef PING_PONG_BIG_SERVICE_CLIENT
                for (int64_t i=0; i<scSessionCnt * 4; ++i)
                {
                        auto remoteServerInfo = ServerListCfgMgr::GetInstance()->GetFirst<stGameMgrServerInfo>();
                        SuperType::Start(remoteServerInfo->_ip, 60000);
                }

                auto act = std::make_shared<PingPongBigActor>();
                act->SendPush(nullptr, 0xfff, 0x0, nullptr);
                act->Start();
#endif
                return true;
        }

DECLARE_SERVICE_BASE_END(PingPongBig);

#ifdef PING_PONG_BIG_SERVICE_SERVER
typedef PingPongBigServiceBase<E_ServiceType_Server, stLobbyServerInfo> PingPongBigService;
#endif

#ifdef PING_PONG_BIG_SERVICE_CLIENT
typedef PingPongBigServiceBase<E_ServiceType_Client, stGameMgrServerInfo> PingPongBigService;
#endif

// vim: fenc=utf8:expandtab:ts=8
