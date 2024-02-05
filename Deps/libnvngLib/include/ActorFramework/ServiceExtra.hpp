#pragma once

#include "ActorFramework/ServiceImpl.hpp"

using namespace nl::net;
using namespace nl::net::server;

namespace nl::af
{

enum EServiceType
{
        E_ServiceType_Local,
        E_ServiceType_Server,
        E_ServiceType_Client,
};

constexpr bool CalServiceTypeIsServer(EServiceType st)
{ return E_ServiceType_Server == st; }

template <typename ServiceType, bool IsServer, typename _Tag = stDefaultTag>
class ServiceSession
        : public ActorAgentSession<TcpSession
                , ActorAgent
                , ServiceSession<ServiceType, IsServer, _Tag>
                , IsServer
                , Compress::ECompressType::E_CT_Zstd
                , _Tag>
{
public :
        typedef ActorAgentSession<TcpSession
                , ActorAgent
                , ServiceSession<ServiceType, IsServer, _Tag>
                , IsServer
                , Compress::ECompressType::E_CT_Zstd
                , _Tag
                > SuperType;
        typedef ServiceSession<ServiceType, IsServer, _Tag> ThisType;

        typedef typename SuperType::MsgHeaderType MsgHeaderType;
        typedef typename SuperType::ActorAgentType ActorAgentType;
        typedef typename SuperType::Tag Tag;

        struct stServiceMessageWapper : public stActorMailBase
        {
                typename SuperType::ActorAgentTypePtr _agent;
                typename SuperType::MsgHeaderType _msgHead;
                std::shared_ptr<google::protobuf::MessageLite> _pb;
        };

public :
        explicit ServiceSession(typename SuperType::SocketType&& s)
                : SuperType(std::move(s))
        {
        }

        void OnConnect() override
        {
                LOG_INFO("{} 连上来了!!!", ServiceType::GetInstance()->GetServiceName());
                SuperType::OnConnect();
        }

        void OnClose(int32_t reasonType) override
        {
                LOG_INFO("{} 断开连接!!! reasonType[{}] id[{}]", ServiceType::GetInstance()->GetServiceName(), reasonType, SuperType::GetID());
                SuperType::OnClose(reasonType);
                ServiceType::GetInstance()->RemoveSession(shared_from_this());
        }

        static void MsgHandleServerInit(const ::nl::net::ISessionPtr& ses
                                        , typename ::nl::net::ISession::BuffTypePtr::element_type* buf
                                        , const ::nl::net::ISession::BuffTypePtr& bufRef)
        {
                SuperType::MsgHandleServerInit(ses, buf, bufRef);
                LOG_INFO("初始化 {} sid[{}] 成功!!!", ServiceType::GetInstance()->GetServiceName(), ses->GetSID());
                ServiceType::GetInstance()->AddSession(std::dynamic_pointer_cast<ServiceSession>(ses));
        }

public :
        EXTERN_MSG_HANDLE();
        DECLARE_SHARED_FROM_THIS(ServiceSession);
};

template <typename ServiceType, typename SessionType, typename ServerInfoType>
class SessionDistributeNull
{
public :
        FORCE_INLINE bool Init() { return true; }
        FORCE_INLINE std::shared_ptr<SessionType> DistSession(uint64_t id) { }
        FORCE_INLINE bool AddSession(const std::shared_ptr<SessionType>& ses) { return false; }
        FORCE_INLINE void RemoveSession(const std::shared_ptr<SessionType>& ses) { }
        FORCE_INLINE void ForeachSession(const auto& cb) { }
};

template <typename ServiceType, typename SessionType, typename ServerInfoType>
class SessionDistributeMod
{
public :
        FORCE_INLINE bool Init() { return true; }
        FORCE_INLINE std::shared_ptr<SessionType> DistSession(uint64_t id)
        {
                {
                        std::lock_guard l(_sesArrMutex);
                        if (!_sesArr.empty())
                                return _sesArr[id % _sesArr.size()].lock();
                }
                return nullptr;
        }

        FORCE_INLINE bool AddSession(const std::shared_ptr<SessionType>& ses)
        {
                if (ses)
                {
                        std::lock_guard l(_sesArrMutex);
                        _sesArr.emplace_back(ses);
                        return true;
                }
                return false;
        }

        FORCE_INLINE void RemoveSession(const std::shared_ptr<SessionType>& ses)
        {
                if (ses)
                {
                        std::lock_guard l(_sesArrMutex);
                        if (!_sesArr.empty())
                                _sesArr.erase(std::remove_if(_sesArr.begin(), _sesArr.end(), [ses](const auto& t) { return ses == t.lock(); }));
                }
        }

        FORCE_INLINE void ForeachSession(const auto& cb)
        {
                std::lock_guard l(_sesArrMutex);
                for (auto& ws : _sesArr)
                {
                        auto ses = ws.lock();
                        if (ses)
                                cb(ses);
                }
        }

private :
        SpinLock _sesArrMutex;
        std::vector<std::weak_ptr<SessionType>> _sesArr;
};

template <typename ServiceType, typename SessionType, typename ServerInfoType>
class SessionDistributeSID
{
public :
        bool Init()
        {
                _sesArr.resize(ServerListCfgMgr::GetInstance()->GetSize<ServerInfoType>());

                if constexpr (!SessionType::IsServer)
                {
                        ::nl::util::SteadyTimer::StartForever(10, 1, []() {
                                return !CheckFinish();
                        });
                }

                return true;
        }

        static bool CheckFinish()
        {
                int64_t cnt = 0;
                for (auto& ws : ServiceType::GetInstance()->_sesDistribute._sesArr)
                {
                        auto s = ws.lock();
                        if (s)
                                ++cnt;
                }

                if (cnt >= ServerListCfgMgr::GetInstance()->GetSize<ServerInfoType>())
                {
                        GetAppBase()->_startPriorityTaskList->Finish(ServiceType::GetInstance()->GetServiceName());
                        return true;
                }
                else
                {
                        LOG_WARN("SessionDistributeSID need all session connected!!! need[{}] cur[{}] taskKey[{}]"
                                 , ServiceType::GetInstance()->_sesDistribute._sesArr.size(), cnt, ServiceType::GetInstance()->GetServiceName());
                        return false;
                }
        }

        FORCE_INLINE std::shared_ptr<SessionType> DistSession(uint64_t id)
        {
                // 不需要 lock。
                return _sesArr[id & (_sesArr.size()-1)].lock();
        }

        FORCE_INLINE bool AddSession(const std::shared_ptr<SessionType>& ses)
        {
                if (!ses)
                        return false;
                auto idx = ServerListCfgMgr::GetInstance()->CalSidIdx<ServerInfoType>(ses->GetSID());
                if (INVALID_SERVER_IDX == idx)
                        return false;

                // lock 只是防止新 ses 被删除。
                std::lock_guard l(_sesArrMutex);
                // LOG_INFO("size[{}]", _sesArr.size());
                _sesArr[idx] = ses;
                CheckFinish();
                return true;
        }

        FORCE_INLINE std::shared_ptr<SessionType> RemoveSession(const std::shared_ptr<SessionType>& ses)
        {
                if (!ses)
                        return nullptr;

                auto idx = ServerListCfgMgr::GetInstance()->CalSidIdx<ServerInfoType>(ses->GetSID());
                if (INVALID_SERVER_IDX == idx)
                        return nullptr;

                // lock 只是防止新 ses 被删除。
                std::lock_guard l(_sesArrMutex);
                auto ret = _sesArr[idx].lock();
                _sesArr[idx].reset();
                return ret;
        }

        FORCE_INLINE void ForeachSession(const auto& cb)
        {
                std::lock_guard l(_sesArrMutex);
                for (auto& ws : _sesArr)
                {
                        auto ses = ws.lock();
                        if (ses)
                                cb(ses);
                }
        }

private :
        SpinLock _sesArrMutex;
        std::vector<std::weak_ptr<SessionType>> _sesArr;
};

template <typename _Ay
        , typename _Iy
        , typename _Sy
        , typename SessionDistributeType
        , EServiceType ServiceType = E_ServiceType_Local>
class ServiceExtra : public ServiceImpl
{
        typedef ServiceImpl SuperType;
        typedef ServiceExtra<_Ay, _Iy, _Sy, SessionDistributeType, ServiceType> ThisType;
        typedef _Iy ImplType;

public :
        typedef _Ay ActorType;
        typedef _Sy SessionType;
        constexpr static bool IsServer = E_ServiceType_Server == ServiceType;

public :
        ServiceExtra(const std::string& flag)
                : SuperType(flag)
                  , _serviceName(flag)
        {
        }

        ~ServiceExtra() override
        {
        }

        bool Init() override
        {
                if (!SuperType::Init())
                        return false;

                return _sesDistribute.Init();
        }

        template <typename ... Args>
        bool Start(Args ... args)
        {
                if constexpr (E_ServiceType_Server == ServiceType)
                {
                        reinterpret_cast<ImplType*>(this)->StartServer(args...);
                }
                else if constexpr (E_ServiceType_Client == ServiceType)
                {
                        ::nl::net::NetMgrBase<typename SessionType::Tag>::GetInstance()->Connect(args..., [](auto&& s) {
                                return std::make_shared<SessionType>(std::move(s));
                        });
                }
                else if constexpr (E_ServiceType_Local == ServiceType)
                {
                        reinterpret_cast<ImplType*>(this)->StartLocal(args...);
                }
                else
                {
                        assert(false);
                        return false;
                }
                return true;
        }

        template <typename ... Args>
        bool StartServer(uint16_t port, Args ... args)
        {
                if (_actorArr.empty())
                {
                        auto serverInfo = GetAppBase()->GetServerInfo<stServerInfoBase>();
                        reinterpret_cast<ImplType*>(this)->StartLocal(serverInfo->_workersCnt * serverInfo->_actorCntPerWorkers, args...);
                }

                ::nl::net::NetMgrBase<typename SessionType::Tag>::GetInstance()->Listen(port, [](auto&& s, const auto& sslCtx) {
                        return std::make_shared<SessionType>(std::move(s));
                });

                return true;
        }

        template <typename ... Args>
        bool StartLocal(int64_t actCnt, Args ... args)
        {
                LOG_FATAL_IF(!CHECK_2N(actCnt), "actCnt 必须设置为 2^N !!!", actCnt);
                for (int64_t i=0; i<actCnt; ++i)
                {
                        auto act = std::make_shared<ActorType>(args...);
                        _actorArr.emplace_back(act);
                        act->Start();
                }

                return true;
        }

public :
        template <typename ... Args> FORCE_INLINE
        auto GetActor(const IActorPtr& act, uint64_t id, Args ... args)
        {
                if constexpr (E_ServiceType_Local == ServiceType)
                {
                        return _actorArr[id & (_actorArr.size() - 1)].lock();
                }
                else if constexpr (E_ServiceType_Client == ServiceType
                                   || E_ServiceType_Server == ServiceType)
                {
                        auto ses = DistSession(act->GetID());
                        if (!ses)
                                return std::shared_ptr<typename SessionType::ActorAgentType>();

                        auto ret = std::make_shared<typename SessionType::ActorAgentType>(id, ses, args...);
                        ret->BindActor(act);
                        ses->AddAgent(ret);
                        return ret;
                }
                else
                {
                        assert(false);
                        return nullptr;
                }
        }

        typedef std::function<void(const std::weak_ptr<ActorType>&)> ForeachAcotrFuncType;
        virtual void ForeachActor(const ForeachAcotrFuncType& cb)
        {
                for (auto& wa : _actorArr)
                        cb(wa);
        }

protected :
        std::vector<std::weak_ptr<ActorType>> _actorArr;

public :
        FORCE_INLINE std::shared_ptr<SessionType> DistSession(uint64_t id)
        { return _sesDistribute.DistSession(id); }
        virtual bool AddSession(const ::nl::net::ISessionPtr& ses)
        { return _sesDistribute.AddSession(std::dynamic_pointer_cast<SessionType>(ses)); }
        FORCE_INLINE void RemoveSession(const ::nl::net::ISessionPtr& ses)
        { _sesDistribute.RemoveSession(std::dynamic_pointer_cast<SessionType>(ses)); }
        FORCE_INLINE void ForeachSession(const auto& cb)
        { _sesDistribute.ForeachSession(cb); }

public :
        SessionDistributeType _sesDistribute;

public :
        FORCE_INLINE std::string_view GetServiceName() const { return _serviceName; }
private :
        const std::string _serviceName;
};

#define DECLARE_SERVICE_BASE_BEGIN(ServiceName, dt, st) \
        template <EServiceType _St, typename ServerInfo> \
        class ServiceName##ServiceBase : public ServiceExtra<ServiceName##Actor \
                               , ServiceName##ServiceBase<_St, ServerInfo> \
                               , st<ServiceName##ServiceBase<_St, ServerInfo>, CalServiceTypeIsServer(_St)> \
                               , dt<ServiceName##ServiceBase<_St, ServerInfo>, st<ServiceName##ServiceBase<_St, ServerInfo>, CalServiceTypeIsServer(_St)>, ServerInfo> \
                               , _St> \
                           , public Singleton<ServiceName##ServiceBase<_St, ServerInfo>> { \
        private : \
                  friend class Singleton<ServiceName##ServiceBase>; \
        public : \
                 typedef ServiceExtra<ServiceName##Actor \
                                   , ServiceName##ServiceBase<_St, ServerInfo> \
                                   , st<ServiceName##ServiceBase<_St, ServerInfo>, CalServiceTypeIsServer(_St)> \
                                   , dt<ServiceName##ServiceBase<_St, ServerInfo>, st<ServiceName##ServiceBase<_St, ServerInfo>, CalServiceTypeIsServer(_St)>, ServerInfo> \
                                   , _St> SuperType; \
                typedef ServiceName##ServiceBase<_St, ServerInfo> ThisType; \
                typedef typename SuperType::SessionType SessionType; \
                constexpr static EServiceType ServiceType = _St; \

#define DECLARE_SERVICE_BASE_END(ServiceName) \
}

#define DEFINE_SERVICE_NET(ServiceName, ServiceFlag, ServerInfo) \
        typedef ServiceName##ServiceBase<E_ServiceType_##ServiceFlag, ServiceInfo> ServiceName##Service

#define DEFINE_SERVICE_LOCAL(ServiceName) \
        typedef ServiceName##ServiceBase<E_ServiceType_Local, stServerInfoBase> ServiceName##Service

#define SERVICE_GET_DEFINE_FUNC(_1, _2, _3, FUNC_NAME, ...)      FUNC_NAME
#define DEFINE_SERVICE(...) SERVICE_GET_DEFINE_FUNC(__VA_ARGS__, DEFINE_SERVICE_NET, DEFINE_SERVICE_LOCAL, ...)(__VA_ARGS__)

#define SERVICE_NET_HANDLE_BASE(sy, mt, st, my) \
        namespace _##sy##_##mt##_##st##_##my { nl::net::stRegistMsgHandleProxy<sy, mt, st, my, 0> _; }; \
        template <> template <> void sy::MsgHandle<mt, st, my>(const sy::MsgHeaderType& msgHead, const std::shared_ptr<my>& msg)

#define SERVICE_NET_HANDLE_EMPTY(sy, mt, st) \
        namespace _##sy##_##mt##_##st { nl::net::stRegistMsgHandleProxy<sy, mt, st, int, 1> _; }; \
        template <> template <> void sy::MsgHandle<mt, st>(const sy::MsgHeaderType& msgHead)

#define SERVICE_NET_HANDLE_BUF(sy, mt, st, buf, bufRef) \
        namespace _##sy##_##mt##_##st { nl::net::stRegistMsgHandleProxy<sy, mt, st, int, 2> _; }; \
        template <> template <> void sy::MsgHandle<mt, st>(const sy::MsgHeaderType& msgHead, ::nl::net::ISession::BuffTypePtr::element_type* buf, std::size_t bufSize, const ::nl::net::ISession::BuffTypePtr& bufRef)

#define SERVICE_NET_HANDLE_MSG_BUF(sy, mt, st, my, buf, bufRef) \
        namespace _##sy##_##mt##_##st { nl::net::stRegistMsgHandleProxy<sy, mt, st, my, 3> _; }; \
        template <> template <> void sy::MsgHandle<mt, st>(const sy::MsgHeaderType& msgHead, const std::shared_ptr<my>& msg, ::nl::net::ISession::BuffTypePtr::element_type* buf, std::size_t bufSize, const ::nl::net::ISession::BuffTypePtr& bufRef)

#define SERVICE_GET_NET_HANDLE_FUNC(_1, _2, _3, _4, _5, _6, FUNC_NAME, ...)      FUNC_NAME
#define SERVICE_NET_HANDLE(...)     SERVICE_GET_NET_HANDLE_FUNC(__VA_ARGS__, SERVICE_NET_HANDLE_MSG_BUF, SERVICE_NET_HANDLE_BUF, SERVICE_NET_HANDLE_BASE, SERVICE_NET_HANDLE_EMPTY, ...)(__VA_ARGS__)

}; // end of namespace nl::af

// vim: fenc=utf8:expandtab:ts=8:noma
