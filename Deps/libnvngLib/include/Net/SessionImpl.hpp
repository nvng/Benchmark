#pragma once

#include "ActorFramework/ActorAgent.hpp"

namespace nl::net
{

enum ESessionSendBufType
{
        E_SSBT_None = 0,
        E_SSBT_GoogleProtobuf = 1,
        E_SSBT_CopyDelete = 2,
        E_SSBT_Copy = 3,
};

template <typename _Hy>
struct stSessionSendBufInfo
{
        _Hy _head;
        ESessionSendBufType _type = E_SSBT_None;
        ISession::BuffTypePtr::element_type* _buf = nullptr;
        std::shared_ptr<void> _ptr;
};

#pragma pack(push,1)
struct MsgTotalHeadType
{
        uint32_t _size = 0;
};

struct MsgHeaderDefault
{
        uint32_t _size = 0;
        uint16_t _type = 0;
        uint64_t _param = 0;

        MsgHeaderDefault() = default;

        MsgHeaderDefault(std::size_t size,
                         Compress::ECompressType ct,
                         std::size_t mt,
                         std::size_t st,
                         uint64_t param = 0)
                : _size(size)
                  , _type(MsgTypeMerge(mt, st))
                  , _param(param)
        {
        }


        FORCE_INLINE void Fill(uint64_t mainType, uint64_t subType, decltype(_size) size, uint64_t param)
        {
                _type = MsgTypeMerge(mainType, subType);
                _size = size;
                _param = param;
        }

        FORCE_INLINE Compress::ECompressType CompressType() const { return Compress::ECompressType::E_CT_ZLib; }
        FORCE_INLINE std::size_t MainType() const { return MsgMainType(_type); }
        FORCE_INLINE std::size_t SubType() const { return MsgSubType(_type); }
        FORCE_INLINE uint64_t Param() const { return _param; }
        DEFINE_MSG_TYPE_MERGE(12);
};
#pragma pack(pop)

// {{{ stRegistMsgHandleProxy

template <typename _Sy, std::size_t _Mt, std::size_t _St, typename _My, std::size_t _Flag = 0>
struct stRegistMsgHandleProxy {
        stRegistMsgHandleProxy() { _Sy::RegistMsgHandle(_Mt, _St, MsgHandle); }
        static std::shared_ptr<_My> ParseMessage(typename ISession::BuffTypePtr::element_type* buf) {
                auto msgHead = *reinterpret_cast<typename _Sy::MsgHeaderType*>(buf);
                auto pb = std::make_shared<_My>();
                Compress::ECompressType ct = msgHead.CompressType();
                if (Compress::E_CT_None != ct) {
                        const auto pbSize = Compress::UnCompressedSize(ct, buf+sizeof(typename _Sy::MsgHeaderType));
                        char* tBuf = new char[pbSize];
                        if (Compress::UnCompress(ct, tBuf, buf+sizeof(typename _Sy::MsgHeaderType), msgHead._size-sizeof(typename _Sy::MsgHeaderType))) {
                                if (!pb->ParseFromArray(tBuf, pbSize)) {
                                        LOG_ERROR("parse msg error!!! mainType[{}] subType[{}]", _Mt, _St);
                                }
                        }
                        delete[] tBuf;
                } else {
                        if (!pb->ParseFromArray(buf+sizeof(typename _Sy::MsgHeaderType), msgHead._size-sizeof(typename _Sy::MsgHeaderType))) {
                                LOG_ERROR("buf parse msg error!!! mainType[{}] subType[{}]", _Mt, _St);
                        }
                }
                return pb;
        }
        static void MsgHandle(const ISessionPtr& ses, ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef) {
                dynamic_pointer_cast<_Sy>(ses)->template MsgHandle<_Mt, _St, _My>(*reinterpret_cast<typename _Sy::MsgHeaderType*>(buf), ParseMessage(buf));
        }
};

#define NET_MSG_HANDLE_BASE(sy, mt, st, my) \
        namespace _##sy##_##mt##_##st##_##my { nl::net::stRegistMsgHandleProxy<sy, mt, st, my, 0> _; }; \
        template <> void sy::MsgHandle<mt, st, my>(const sy::MsgHeaderType& msgHead, const std::shared_ptr<my>& msg)

template <typename _Sy, std::size_t _Mt, std::size_t _St>
struct stRegistMsgHandleProxy<_Sy, _Mt, _St, int, 1> {
        stRegistMsgHandleProxy() { _Sy::RegistMsgHandle(_Mt, _St, MsgHandle); }
        static void MsgHandle(const ISessionPtr& ses, ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef) {
                dynamic_pointer_cast<_Sy>(ses)->template MsgHandle<_Mt, _St>(*reinterpret_cast<typename _Sy::MsgHeaderType*>(buf));
        }
};

#define NET_MSG_HANDLE_EMPTY(sy, mt, st) \
        namespace _##sy##_##mt##_##st { nl::net::stRegistMsgHandleProxy<sy, mt, st, int, 1> _; }; \
        template <> void sy::MsgHandle<mt, st>(const sy::MsgHeaderType& msgHead)

template <typename _Sy, std::size_t _Mt, std::size_t _St>
struct stRegistMsgHandleProxy<_Sy, _Mt, _St, int, 2> {
        stRegistMsgHandleProxy() { _Sy::RegistMsgHandle(_Mt, _St, MsgHandle); }
        static void MsgHandle(const ISessionPtr& ses, ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef) {
                auto msgHead = *reinterpret_cast<typename _Sy::MsgHeaderType*>(buf);
                dynamic_pointer_cast<_Sy>(ses)->template MsgHandle<_Mt, _St>(msgHead
                                                                             , buf + sizeof(typename _Sy::MsgHeaderType) + sizeof(uint32_t)
                                                                             , msgHead._size - sizeof(typename _Sy::MsgHeaderType) - sizeof(uint32_t)
                                                                             , bufRef);
        }
};

#define NET_MSG_HANDLE_BUF(sy, mt, st, buf, bufRef) \
        namespace _##sy##_##mt##_##st { nl::net::stRegistMsgHandleProxy<sy, mt, st, int, 2> _; }; \
        template <> void sy::MsgHandle<mt, st>(const sy::MsgHeaderType& msgHead, ISession::BuffTypePtr::element_type* buf, std::size_t bufSize, const ISession::BuffTypePtr& bufRef)

template <typename _Sy, std::size_t _Mt, std::size_t _St, typename _My>
struct stRegistMsgHandleProxy<_Sy, _Mt, _St, _My, 3> {
        stRegistMsgHandleProxy() { _Sy::RegistMsgHandle(_Mt, _St, MsgHandle); }
        static std::shared_ptr<_My> ParseMessage(typename ISession::BuffTypePtr::element_type* buf, std::size_t offset = sizeof(uint32_t)) {
                auto msgHead = *reinterpret_cast<typename _Sy::MsgHeaderType*>(buf);
                const auto msgSize = *reinterpret_cast<uint32_t*>(buf+sizeof(typename _Sy::MsgHeaderType));
                auto pb = std::make_shared<_My>();
                Compress::ECompressType ct = msgHead.CompressType();
                if (Compress::E_CT_None != ct) {
                        const auto pbSize = Compress::UnCompressedSize(ct, buf+sizeof(typename _Sy::MsgHeaderType)+offset);
                        char* tBuf = new char[pbSize];
                        if (Compress::UnCompress(ct, tBuf, buf+sizeof(typename _Sy::MsgHeaderType)+offset, msgSize)) {
                                if (!pb->ParseFromArray(tBuf, pbSize)) {
                                        LOG_ERROR("parse msg error!!! mainType[{}] subType[{}]", _Mt, _St);
                                }
                        }
                        delete[] tBuf;
                } else {
                        if (!pb->ParseFromArray(buf+sizeof(typename _Sy::MsgHeaderType)+offset, msgSize)) {
                                LOG_ERROR("buf parse msg error!!! mainType[{}] subType[{}]", _Mt, _St);
                        }
                }
                return pb;
        }
        static void MsgHandle(const ISessionPtr& ses, ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef) {
                const auto msgSize = *reinterpret_cast<uint32_t*>(buf+sizeof(typename _Sy::MsgHeaderType));
                auto msgHead = *reinterpret_cast<typename _Sy::MsgHeaderType*>(buf);
                dynamic_pointer_cast<_Sy>(ses)->template MsgHandle<_Mt, _St>(msgHead
                                                                             , ParseMessage(buf)
                                                                             , buf + sizeof(typename _Sy::MsgHeaderType) + sizeof(uint32_t) + msgSize
                                                                             , msgHead._size - sizeof(typename _Sy::MsgHeaderType) - sizeof(uint32_t) - msgSize
                                                                             , bufRef);
        }
};

#define NET_MSG_HANDLE_MSG_BUF(sy, mt, st, my, buf, bufRef) \
        namespace _##sy##_##mt##_##st { nl::net::stRegistMsgHandleProxy<sy, mt, st, my, 3> _; }; \
        template <> void sy::MsgHandle<mt, st>(const sy::MsgHeaderType& msgHead, const std::shared_ptr<my>& msg, ISession::BuffTypePtr::element_type* buf, std::size_t bufSize, const ISession::BuffTypePtr& bufRef)

#define GET_NET_MSG_HANDLE_FUNC(_1, _2, _3, _4, _5, _6, FUNC_NAME, ...)      FUNC_NAME
#define NET_MSG_HANDLE(...)     GET_NET_MSG_HANDLE_FUNC(__VA_ARGS__, NET_MSG_HANDLE_MSG_BUF, NET_MSG_HANDLE_BUF, NET_MSG_HANDLE_BASE, NET_MSG_HANDLE_EMPTY, ...)(__VA_ARGS__)

#define EXTERN_MSG_HANDLE() \
        public : \
        template <std::size_t, std::size_t, typename _Ty> void MsgHandle(const MsgHeaderType&, const std::shared_ptr<_Ty>&); \
        template <std::size_t, std::size_t, typename _Ty> void MsgHandle(const MsgHeaderType&, const std::shared_ptr<_Ty>&, ISession::BuffTypePtr::element_type*, std::size_t, const ISession::BuffTypePtr&); \
        template <std::size_t, std::size_t> void MsgHandle(const MsgHeaderType&); \
        template <std::size_t, std::size_t> void MsgHandle(const MsgHeaderType&, ISession::BuffTypePtr::element_type*, std::size_t, const ISession::BuffTypePtr&)

// }}}

template <typename _Ty
         , bool _Sy
         , typename _My = MsgHeaderDefault
         , Compress::ECompressType _Ct = Compress::ECompressType::E_CT_Zstd
         , typename _Tag = stDefaultTag>
class SessionImpl
        : public ISession
          , public std::enable_shared_from_this<SessionImpl<_Ty, _Sy, _My, _Ct, _Tag>>
{
public:
        typedef ISession SuperType;
        typedef SessionImpl<_Ty, _Sy, _My, _Ct, _Tag> ThisType;
        typedef _Ty ImplType;
        typedef _My MsgHeaderType;
        typedef _Tag Tag;
        constexpr static Compress::ECompressType CompressType = _Ct;
        constexpr static bool IsServer = _Sy;

        SessionImpl(auto& ctx)
                : _sendHeartBeatTimer(ctx)
                  , _checkHeartBeatTimer(ctx)
        {
        }

        ~SessionImpl() override
        {
        }

        bool Init() override
        {
                if (!SuperType::Init())
                        return false;

                static bool ret = _Ty::InitOnce();
                return ret;
        }

        static bool InitOnce()
        {
                /*
                RegisterCloseReasonString(E_SCRT_None, "占位符");
                RegisterCloseReasonString(E_SCRT_RecvMaxSizeLimit, "收到大小超过限制的包");
                RegisterCloseReasonString(E_SCRT_MsgBodyParseError, "消息体解析失败");
                RegisterCloseReasonString(E_SCRT_MsgPBCreateError, "创建PB对象失败");
                RegisterCloseReasonString(E_SCRT_BuffereventEnableError, "bufferevent_enable 错误");
                RegisterCloseReasonString(E_SCRT_BuffereventErrorEvent, "SocketEventCallback 错误");
                RegisterCloseReasonString(E_SCRT_HeartBeatOverTime, "心跳包超时");
                RegisterCloseReasonString(E_SCRT_ByMasterServer, "被主服务器关闭");
                RegisterCloseReasonString(E_SCRT_GateDisconnectByMasterServer, "网关与主服务器断开连接");
                RegisterCloseReasonString(E_SCRT_GateIsAttacked, "网关被攻击");
                RegisterCloseReasonString(E_SCRT_ByLogic, "逻辑需要关闭");
                RegisterCloseReasonString(E_SCRT_KickOut, "玩家被踢出");
                RegisterCloseReasonString(E_SCRT_ReConnect, "玩家重连上来");
                */

                if constexpr (IsServer)
                        RegistMsgHandle(E_MIMT_Internal, E_MIIST_HeartBeat, ImplType::Msg_Handle_ServerHeartBeat);
                else
                        RegistMsgHandle(E_MIMT_Internal, E_MIIST_HeartBeat, ImplType::Msg_Handle_ClientHeartBeat_Ret);

                RegistMsgHandle(E_MIMT_Internal, E_MIIST_ServerInit, _Ty::MsgHandleServerInit);
                return true;
        }

protected :
        virtual void OnMessageMultiCast(ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
        {
        }

        virtual void OnRecvInternal(ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
        {
                auto msgHead = *reinterpret_cast<MsgHeaderType*>(buf);
                switch (msgHead._type)
                {
                case MsgHeaderType::template MsgTypeMerge<E_MIMT_Internal, E_MIIST_HeartBeat>() :
                case MsgHeaderType::template MsgTypeMerge<E_MIMT_Internal, E_MIIST_ServerInit>() :
                        ThisType::OnRecv(buf, bufRef);
                        break;
                case MsgHeaderType::template MsgTypeMerge<E_MIMT_Internal, E_MIIST_MultiCast>() :
                case MsgHeaderType::template MsgTypeMerge<E_MIMT_Internal, E_MIIST_BroadCast>() :
                        OnMessageMultiCast(buf, bufRef);
                        break;
                default :
                        OnRecv(buf, bufRef);
                        break;
                }
        }

        virtual void OnRecv(ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
        {
                auto handleList = GetMsgHandleList();
                auto msgHead = *reinterpret_cast<MsgHeaderType*>(buf);
                auto cb = handleList[msgHead._type];
                if (nullptr != cb)
                {
                        cb(ThisType::shared_from_this(), buf, bufRef);
                }
                else
                {
                        LOG_WARN_M(NET_MODEL_NAME,
                                   "ses[{}] invalid _type[{:#x}] mainType[{:#x}] subType[{:#x}] nullptr == cb",
                                   SuperType::GetID(), msgHead._type, msgHead.MainType(), msgHead.SubType());
                }
        }

public :
        static void MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
        {
                auto msg = stRegistMsgHandleProxy<ThisType, E_MIMT_Internal, E_MIIST_ServerInit, MsgServerInit>::ParseMessage(buf);
                ses->SetSID(msg->sid());
                ses->SetRemoteID(msg->remote_id());
                if (msg->is_crash())
                        ses->SetRemoteCrash();
        }

        void OnConnect() override
        {
                SuperType::OnConnect();

                if constexpr (IsServer)
                {
                        InitCheckHeartBeatTimer();
                }
                else
                {
                        InitSendHeartBeatTimer();
                        InitCheckHeartBeatTimer();
                }

                if constexpr (std::is_same<MsgHeaderType, MsgActorAgentHeaderType>::value)
                {
                        auto sendMsg = std::make_shared<MsgServerInit>();
                        sendMsg->set_sid(GetAppBase()->GetSID());
                        sendMsg->set_remote_id(GetID());
                        sendMsg->set_is_crash(SuperType::IsCrash());
                        reinterpret_cast<ImplType*>(this)->SendPB(sendMsg, E_MIMT_Internal, E_MIIST_ServerInit, MsgHeaderType::E_RMT_None, 0, GetID(), 0);
                }
        }

        void Close(int32_t reasonType) override
        {
                SuperType::Close(reasonType);

                auto t = NetMgrBase<Tag>::GetInstance()->_sesList.Remove(GetID(), this);
                if (t)
                {
                        OnClose(reasonType);
                        if (IsAutoReconnect() && !GetAppBase()->IsStartStop())
                        {
                                NetMgrBase<Tag>::GetInstance()->Connect(_connectEndPoint.address().to_string(),
                                                                         _connectEndPoint.port(),
                                                                         std::move(_createSession));
                        }
                }
        }

        void OnClose(int32_t reasonType) override
        {
                SuperType::OnClose(reasonType);

                _sendHeartBeatTimer.Stop();
                _checkHeartBeatTimer.Stop();
        }

        FORCE_INLINE std::size_t SerializeAndCompressNeedSize(std::size_t s)
        { return s + sizeof(MsgHeaderType) + (Compress::NeedCompress<CompressType>(s) ? Compress::CompressedSize<CompressType>(s) : 0); }

        FORCE_INLINE static std::pair<std::size_t, Compress::ECompressType>
        SerializeAndCompress(const google::protobuf::MessageLite& pb, typename ISession::BuffTypePtr::element_type* buf)
        {
                const auto pbSize = pb.ByteSizeLong();
                if (Compress::NeedCompress<CompressType>(pbSize))
                {
                        auto compressedSize = Compress::CompressedSize<CompressType>(pbSize);
                        if (pb.SerializeToArray(buf+compressedSize, pbSize))
                        {
                                bool r = Compress::Compress<CompressType>(buf, compressedSize, buf+compressedSize, pbSize);
                                if (r)
                                        return { compressedSize, CompressType };
                        }
                }

                pb.SerializeToArray(buf, pbSize);
                return { pbSize, Compress::ECompressType::E_CT_None };
        }

        typedef void(*MsgHandleType)(const ISessionPtr&, ISession::BuffTypePtr::element_type*, const ISession::BuffTypePtr&);
        static void RegistMsgHandle(uint64_t mainType, uint64_t subType, MsgHandleType cb)
        {
                auto msgType = MsgHeaderType::MsgTypeMerge(mainType, subType);
                static auto handleList = GetMsgHandleList();
                assert(nullptr == handleList[msgType]);
                if (0 <= msgType && msgType < MsgHeaderType::scArraySize)
                        handleList[msgType] = cb;
        }

public:
        static FORCE_INLINE MsgHandleType* GetMsgHandleList()
        {
                static MsgHandleType hl[MsgHeaderType::scArraySize];
                return hl;
        }

public :
        boost::asio::ip::tcp::endpoint _connectEndPoint;
        std::function<std::shared_ptr<ImplType>(boost::asio::ip::tcp::socket&&)> _createSession;


public :
        FORCE_INLINE void SendHeartBeat(const std::shared_ptr<google::protobuf::MessageLite>& pb)
        {
                reinterpret_cast<ImplType*>(this)->SendPB(pb, E_MIMT_Internal, E_MIIST_HeartBeat);
        }

        static FORCE_INLINE void Msg_Handle_ServerHeartBeat(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
        {
                auto thisPtr = std::dynamic_pointer_cast<ThisType>(ses);
                thisPtr->SendHeartBeat(nullptr);
                thisPtr->_lastHeartBeatTime = GetClock().GetSteadyTime();
                // LOG_INFO("33333333333333333333333333333 接收心跳包!!! now[{}]", GetClock().GetSteadyTime());
        }

        static FORCE_INLINE void Msg_Handle_ClientHeartBeat_Ret(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
        {
                // LOG_INFO("33333333333333333333333333333 接收心跳包!!! now[{}]", GetClock().GetSteadyTime());
                dynamic_pointer_cast<ThisType>(ses)->_lastHeartBeatTime = GetClock().GetSteadyTime();
        }

protected :
        virtual void InitSendHeartBeatTimer()
        { InitSendHeartBeatTimerInternal(3); }

        void InitSendHeartBeatTimerInternal(double interval)
        {
                auto weakThis = ThisType::weak_from_this();
                _sendHeartBeatTimer.Start(interval, [weakThis, interval]() {
                        auto thisPtr = weakThis.lock();
                        if (thisPtr)
                        {
                                std::reinterpret_pointer_cast<ImplType>(thisPtr)->SendHeartBeat(nullptr);
                                thisPtr->InitSendHeartBeatTimerInternal(interval);
                        }
                });
        }

        virtual void InitCheckHeartBeatTimer()
        { InitCheckHeartBeatTimerInternal(15, 15); }

        void InitCheckHeartBeatTimerInternal(double baseInternal, double internal)
        {
                auto weakThis = ThisType::weak_from_this();
                _checkHeartBeatTimer.Start(internal, [weakThis, baseInternal]() {
                        auto thisPtr = weakThis.lock();
                        if (thisPtr)
                        {
                                const auto now = GetClock().GetSteadyTime();
                                if (thisPtr->_lastHeartBeatTime + baseInternal > now)
                                {
                                        const double diff = baseInternal - (now - thisPtr->_lastHeartBeatTime);
                                        thisPtr->InitCheckHeartBeatTimerInternal(baseInternal, diff);
                                }
                                else
                                {
                                        // LOG_INFO("11111111111111111111111111111 超时断开连接!!! _last[{}] internal[{}] now[{}]", _lastHeartBeatTime, baseInternal, now);
                                        thisPtr->Close(1001);
                                }
                        }
                });
        }

        std::atomic<double> _lastHeartBeatTime = 0;

private :
        ::nl::util::SteadyTimer _sendHeartBeatTimer;
        ::nl::util::SteadyTimer _checkHeartBeatTimer;
};

}; // end of namespace nl::net

// vim: fenc=utf8:expandtab:ts=8:noma
