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

template <typename ServiceType, bool IsServer>
class PingPongBigSession : public ServiceSession<ServiceType, IsServer>
{
        typedef ServiceSession<ServiceType, IsServer> SuperType;
public :
        typedef typename SuperType::MsgHeaderType MsgHeaderType;
public :
        PingPongBigSession(typename SuperType::SocketType&& s)
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

struct stNetMgrTestFlag_1;
struct stNetMgrTestFlag_2;
struct stNetMgrTestFlag_3;

DECLARE_SERVICE_BASE_BEGIN(PingPongBig, SessionDistributeMod, PingPongBigSession);

        constexpr static int64_t scSessionCnt = 5;
        constexpr static int64_t scPkgPerSession = 100;

private :
        PingPongBigServiceBase() : SuperType("PingPongBigService") { }
        ~PingPongBigServiceBase() override { }

public :
        bool Init() override
        {
                if (!SuperType::Init())
                        return false;

                ::nl::net::NetMgrBase<stNetMgrTestFlag_1>::CreateInstance();
                ::nl::net::NetMgrBase<stNetMgrTestFlag_1>::GetInstance()->Init(1, "t_1");
                ::nl::net::NetMgrBase<stNetMgrTestFlag_2>::CreateInstance();
                ::nl::net::NetMgrBase<stNetMgrTestFlag_2>::GetInstance()->Init(1, "t_2");
                ::nl::net::NetMgrBase<stNetMgrTestFlag_3>::CreateInstance();
                ::nl::net::NetMgrBase<stNetMgrTestFlag_3>::GetInstance()->Init(1, "t_3");
#ifdef PING_PONG_BIG_SERVICE_SERVER
                SuperType::Start(60000);
                /*
                ::nl::net::NetMgrBase<stNetMgrTestFlag_1>::GetInstance()->Listen(61110, [](auto&& s, const auto& sslCtx) {
                        return std::make_shared<typename SuperType::SessionType>(std::move(s));
                });
                ::nl::net::NetMgrBase<stNetMgrTestFlag_2>::GetInstance()->Listen(61120, [](auto&& s, const auto& sslCtx) {
                        return std::make_shared<typename SuperType::SessionType>(std::move(s));
                });
                ::nl::net::NetMgrBase<stNetMgrTestFlag_3>::GetInstance()->Listen(61130, [](auto&& s, const auto& sslCtx) {
                        return std::make_shared<typename SuperType::SessionType>(std::move(s));
                });
                */
#endif

#ifdef PING_PONG_BIG_SERVICE_CLIENT
                for (int64_t i=0; i<scSessionCnt * 4; ++i)
                {
                        auto remoteServerInfo = ServerListCfgMgr::GetInstance()->GetFirst<stGameMgrServerInfo>();
                        SuperType::Start(remoteServerInfo->_ip, 60000);
                }

                /*
                for (int64_t i=0; i<scSessionCnt; ++i)
                {
                        auto remoteServerInfo = ServerListCfgMgr::GetInstance()->GetFirst<stGameMgrServerInfo>();
                        ::nl::net::NetMgrBase<stNetMgrTestFlag_1>::GetInstance()->Connect(remoteServerInfo->_ip, 61110, [](auto&& s) {
                                return std::make_shared<typename SuperType::SessionType>(std::move(s));
                        });
                }

                for (int64_t i=0; i<scSessionCnt; ++i)
                {
                        auto remoteServerInfo = ServerListCfgMgr::GetInstance()->GetFirst<stGameMgrServerInfo>();
                        ::nl::net::NetMgrBase<stNetMgrTestFlag_2>::GetInstance()->Connect(remoteServerInfo->_ip, 61120, [](auto&& s) {
                                return std::make_shared<typename SuperType::SessionType>(std::move(s));
                        });
                }

                for (int64_t i=0; i<scSessionCnt; ++i)
                {
                        auto remoteServerInfo = ServerListCfgMgr::GetInstance()->GetFirst<stGameMgrServerInfo>();
                        ::nl::net::NetMgrBase<stNetMgrTestFlag_3>::GetInstance()->Connect(remoteServerInfo->_ip, 61130, [](auto&& s) {
                                return std::make_shared<typename SuperType::SessionType>(std::move(s));
                        });
                }
                */

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
