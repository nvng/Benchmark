#pragma once

#include <stdint.h>

#include "Tools/Util.h"
#include "ActorFramework/IActor.h"

namespace nl::net
{

// {{{ MsgActorAgentHeader

#pragma pack(push,1)
template <uint8_t _Offset>
struct MsgActorAgentHeader
{
        enum ERemoteMailType
        {
                E_RMT_None,
                E_RMT_Send,
                E_RMT_Call,
                E_RMT_CallRet,
        };

        uint32_t _size = 0;
        uint16_t _type = 0;
        uint16_t _flag : 2 = 0;
        uint16_t _ct : 2 = 0;
        uint16_t _guid : 12 = 0;
        uint64_t _from = 0;
        uint64_t _to = 0;

        MsgActorAgentHeader() = default;

        MsgActorAgentHeader(uint64_t size,
                            Compress::ECompressType ct,
                            uint64_t mainType,
                            uint64_t subType)
                : _size(size)
                  , _type(MsgTypeMerge(mainType, subType))
                  , _ct(ct)
        {
        }

        MsgActorAgentHeader(uint64_t size,
                            Compress::ECompressType ct,
                            uint64_t mainType,
                            uint64_t subType,
                            ERemoteMailType flag,
                            uint16_t guid,
                            uint64_t from,
                            uint64_t to)
                : _size(size)
                  , _type(MsgTypeMerge(mainType, subType))
                  , _flag(flag)
                  , _ct(ct)
                  , _guid(guid)
                  , _from(from)
                  , _to(to)
        {
        }

        FORCE_INLINE Compress::ECompressType CompressType() const { return static_cast<Compress::ECompressType>(_ct); }
        FORCE_INLINE uint64_t MainType() const { return MsgMainType(_type); }
        FORCE_INLINE uint64_t SubType() const { return MsgSubType(_type); }

        DEFINE_MSG_TYPE_MERGE(_Offset);

        struct MsgMultiCastHeader
        {
                uint32_t _size = 0;
                uint16_t _type = 0;
                uint64_t _stype : 16 = 0;
                uint64_t _ct : 2 = 0;
                uint64_t _sct : 2 = 0;
                uint64_t _ssize : 60 = 0;
                uint64_t _from = 0;

                MsgMultiCastHeader(uint64_t size,
                                   uint64_t mt,
                                   uint64_t st,
                                   uint64_t smt,
                                   uint64_t sst,
                                   Compress::ECompressType ct,
                                   Compress::ECompressType sct,
                                   uint64_t ssize,
                                   uint64_t from)
                        : _size(size)
                          , _type(MsgTypeMerge(mt, st))
                          , _stype(MsgTypeMerge(smt, sst))
                          , _ct(ct)
                          , _sct(sct)
                          , _ssize(ssize)
                          , _from(from)
                {
                }
        };
};

static_assert(sizeof(MsgActorAgentHeader<8>) == sizeof(MsgActorAgentHeader<8>::MsgMultiCastHeader));
#pragma pack(pop)

typedef MsgActorAgentHeader<12> MsgActorAgentHeaderType;

// }}}

// {{{ ActorNetMail

template <typename _By, typename _Ay>
class ActorNetMail: public _By
{
        typedef _By SuperType;
        typedef _Ay MsgHeaderType;
public :
        ActorNetMail(const IActorPtr& from,
                     const MsgHeaderType& msgHead,
                     ISession::BuffTypePtr::element_type* buf,
                     const ISession::BuffTypePtr& bufRef)
                : SuperType(from, nullptr, msgHead._type)
                  , _buf(buf), _bufRef(bufRef)
        {
        }

        ActorNetMail(const IActorPtr& from,
                     const MsgHeaderType& msgHead,
                     ISession::BuffTypePtr::element_type* buf,
                     const ISession::BuffTypePtr& bufRef,
                     int)
                : SuperType(from, nullptr, msgHead._type, msgHead._guid)
                  , _buf(buf), _bufRef(bufRef)
        {
        }

        ~ActorNetMail() override
        {
        }

        bool Parse(google::protobuf::MessageLite& pb) override
        {
                auto msgHead = *reinterpret_cast<MsgHeaderType*>(_buf);
                if (Compress::E_CT_None != msgHead.CompressType())
                        return Compress::UnCompressAndParseAlloc(msgHead.CompressType(), pb, _buf+sizeof(MsgHeaderType), msgHead._size-sizeof(MsgHeaderType));
                else
                        return pb.ParseFromArray(_buf+sizeof(MsgHeaderType), msgHead._size - sizeof(MsgHeaderType));
        }

        std::tuple<std::shared_ptr<void>, std::string_view, bool>
        ParseExtra(google::protobuf::MessageLite& pb) override
        {
                auto msgHead = *reinterpret_cast<MsgHeaderType*>(_buf);
                constexpr std::size_t offset = sizeof(uint32_t);
                auto msgSize = *reinterpret_cast<uint32_t*>(_buf + sizeof(MsgHeaderType));
                bool ret = 0;
                if (Compress::E_CT_None != msgHead.CompressType())
                        ret = Compress::UnCompressAndParseAlloc(msgHead.CompressType(), pb, _buf+sizeof(MsgHeaderType)+offset, msgSize);
                else
                        ret = pb.ParseFromArray(_buf+sizeof(MsgHeaderType)+offset, msgSize);

                return { _bufRef, { _buf+sizeof(MsgHeaderType)+offset+msgSize, msgHead._size-sizeof(MsgHeaderType)-offset-msgSize }, ret };
        }

        uint64_t Flag() const override { return 2; }

        ISession::BuffTypePtr::element_type* _buf = nullptr;
        const ISession::BuffTypePtr _bufRef;
};

// }}}

// {{{ ActorAgent

template <typename _Ty>
class ActorAgent : public IActor, public std::enable_shared_from_this<ActorAgent<_Ty>>
{
        typedef IActor SuperType;
        typedef _Ty SessionType;
        typedef typename SessionType::MsgHeaderType MsgHeaderType;
        typedef ActorAgent<SessionType> ThisType;
public :
        ActorAgent(uint64_t id, const std::shared_ptr<SessionType>& ses)
                : _ses(ses)
                  , _id(id)
        {
                // 允许后面再调用 SetSession。
                if (ses)
                        _sid = ses->GetSID();
        }

        ~ActorAgent() override
        {
                auto ses = _ses.lock();
                if (ses)
                {
                        ses->RemoveAgent(GetAgentID(), GetLocalID(), this);
                }
                else
                {
                        // 不需要删除，新 ses 连接上来后，只有 agent 还有效的才绑定。
                        /*
                        std::lock_guard l(SessionType::_agentListBySIDMutex);
                        auto it = SessionType::_agentListBySID.find(GetSID());
                        if (SessionType::_agentListBySID.end() != it)
                                it->second.erase((uint64_t)GetAgentID() << 32 | GetLocalID());
                                */
                }
        }

public :
        uint64_t GetID() const override { return _id; }
        FORCE_INLINE uint64_t GetAgentID() const { return _id; }
        FORCE_INLINE uint64_t GetLocalID() const { return _local; }

protected :
        FORCE_INLINE void OnRecv(ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
        {
                auto target = _bindActor.lock();
                if (target)
                {
                        auto msgHead = *reinterpret_cast<MsgHeaderType*>(buf);
                        switch (msgHead._flag)
                        {
                        case MsgHeaderType::E_RMT_Send :
                                {
                                        auto mail = std::make_shared<ActorNetMail<ActorMail, MsgHeaderType>>(ThisType::shared_from_this(), msgHead, buf, bufRef);
                                        target->Push(mail);
                                }
                                break;
                        case MsgHeaderType::E_RMT_Call :
                                {
                                        auto mail = std::make_shared<ActorNetMail<ActorCallMail, MsgHeaderType>>(ThisType::shared_from_this(), msgHead, buf, bufRef, 1);
                                        target->Push(mail);
                                }
                                break;
                        default :
                                break;
                        }
                }
        }

        FORCE_INLINE void Push(const IActorMailPtr& m) override
        {
                auto target = _bindActor.lock();
                if (target)
                        target->Push(m);
        }

        FORCE_INLINE bool SendInternal(uint64_t mainType,
                                       uint64_t subType,
                                       const ActorMailDataPtr& msg)
        {
                auto target = _bindActor.lock();
                if (target)
                {
                        target->SendPush(ThisType::shared_from_this(), mainType, subType, msg);
                        return true;
                }
                else
                {
                        return false;
                }
        }

        // 远程会有一系列连续操作是一个整体，一个超时，也应该接收其它消息。
        // from remote
        FORCE_INLINE void CallInternal(uint16_t guid,
                                       uint64_t mainType,
                                       uint64_t subType,
                                       const ActorMailDataPtr& msg)
        {
                auto target = _bindActor.lock();
                if (target && target->CallPushInternal(ThisType::shared_from_this(), mainType, subType, msg, guid))
                {
                        return;
                }

                if (!target)
                        LOG_ERROR("远程 Call 本地，target is nullptr!!! id[{}] local[{}] mt[{:#x}] st[{:#x}] guid[{}]",
                                  GetID(), GetLocalID(), mainType, subType, guid);
                else
                        LOG_ERROR("远程 Call 本地，CallPush fail!!! id[{}] local[{}] mt[{:#x}] st[{:#x}] guid[{}]",
                                  GetID(), GetLocalID(), mainType, subType, guid);

                CallRet(nullptr, guid, 0, 0);
                return;
        }

public :
        void SendPush(const IActorPtr& from,
                      uint64_t mainType,
                      uint64_t subType,
                      const ActorMailDataPtr& msg) override
        {
                assert(from == _bindActor.lock());
                auto ses = _ses.lock();
                if (ses)
                {
                        ses->SendPB(msg, mainType, subType, MsgHeaderType::E_RMT_Send, 0, GetLocalID(), GetID());
                }
                else
                {
                        DLOG_WARN("ses 已经释放，但还收到发送消息!!!");
                }
        }

        // send to remote
        bool CallPushInternal(const IActorPtr& from,
                      uint64_t mainType,
                      uint64_t subType,
                      const ActorMailDataPtr& msg,
                      uint16_t guid) override
        {
                assert(from == _bindActor.lock());
                auto ses = _ses.lock();
                if (ses)
                {
                        ses->SendPB(msg, mainType, subType, MsgHeaderType::E_RMT_Call, guid, GetLocalID(), GetID());
                        return true;
                }
                else
                {
                        DLOG_WARN("ses 已经释放，但还收到发送消息!!!");
                        return false;
                }
        }

        bool CallPushInternal(const IActorPtr& from,
                              uint64_t mainType,
                              uint64_t subType,
                              const ActorMailDataPtr& msg,
                              uint16_t guid,
                              const std::shared_ptr<void>& bufRef,
                              const char* buf,
                              std::size_t bufSize) override
        {
                assert(from == _bindActor.lock());
                auto ses = _ses.lock();
                if (ses)
                {
                        ses->SendPBExtra(msg, bufRef, buf, bufSize, mainType, subType, MsgHeaderType::E_RMT_Call, guid, GetLocalID(), GetID());
                        return true;
                }
                else
                {
                        DLOG_WARN("ses 已经释放，但还收到发送消息!!!");
                        return false;
                }
        }

        ActorCallMailPtr AfterCallPush(::nl::af::channel_t<ActorCallMailPtr>& ch,
                                       uint64_t mt,
                                       uint64_t st,
                                       uint16_t& guid) override
        {
                ActorCallMailPtr ret;
                while (boost::fibers::channel_op_status::timeout != ch.pop_wait_for(ret, std::chrono::seconds(IActor::scCallRemoteTimeOut))
                       && guid != ret->_guid)
                {
                        LOG_ERROR("远程回复本地不匹配!!! id[{}] local[{}] mt[{:#x}] st[{:#x}] guid[{}] ret->_guid[{}]",
                                  GetID(), GetLocalID(), mt, st, guid, ret->_guid);
                }

                if (ret)
                {
                        if (ActorMail::MsgTypeMerge(mt, st) == ret->_type)
                        {
                                return ret;
                        }
                        else
                        {
                                LOG_WARN("id[{}] 收到 Call 回复消息类型不匹配!!! ret[{:#x}] call[{:#x}]",
                                         GetID(), ret->_type, ActorMail::MsgTypeMerge(mt, st));
                                return nullptr;
                        }
                }
                else // 超时才加。
                {
#ifndef ____BENCHMARK____
                        LOG_WARN("远程回复本地超时!!! id[{}] local[{}] mt[{:#x}] st[{:#x}] guid[{}]",
                                 GetID(), GetLocalID(), mt, st, guid);
#endif
                        ++guid;
                        return ret;
                }
        }

        // local call ret
        void CallRet(const ActorMailDataPtr& msg,
                     uint16_t guid,
                     uint64_t mainType,
                     uint64_t subType) override
        {
                auto ses = _ses.lock();
                if (ses)
                {
                        ses->SendPB(msg, mainType, subType, MsgHeaderType::E_RMT_CallRet, guid, GetLocalID(), GetID());
                }
        }

        using SuperType::RemoteCallRet;
        FORCE_INLINE void RemoteCallRet(ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
        {
                auto act = _bindActor.lock();
                if (act)
                {
                        auto msgHead = *reinterpret_cast<MsgHeaderType*>(buf);
                        auto mail = std::make_shared<ActorNetMail<ActorCallMail, MsgHeaderType>>(ThisType::shared_from_this(), msgHead, buf, bufRef, 1);
                        act->RemoteCallRet(ActorMail::MsgMainType(msgHead._type), ActorMail::MsgSubType(msgHead._type), msgHead._guid, mail);
                }
        }

        FORCE_INLINE void SetSession(const std::shared_ptr<SessionType>& ses)
        {
                auto oldSes = _ses.lock();
                if (oldSes)
                        ses->RemoveAgent(GetAgentID(), GetLocalID(), this);
                _ses = ses;
                if (ses)
                        _sid = ses->GetSID();
        }

        FORCE_INLINE std::shared_ptr<SessionType> GetSession() { return _ses.lock(); }
        FORCE_INLINE int64_t GetSID() const { return _sid; }

        virtual void BindActor(const IActorPtr& b) { if (b) _local = b->GetID(); _bindActor = b; }
        const IActorPtr GetBindActor() const { return _bindActor.lock(); }

protected :
        friend SessionType;
        friend typename SessionType::SuperType;
        IActorWeakPtr _bindActor;
        std::weak_ptr<SessionType> _ses;
protected :
        uint64_t _local = 0;
private :
        const uint64_t _id = 0;
        uint32_t _sid = 0;
};

// }}}

// {{{ ActorAgentSession

template <template<typename, bool, typename, Compress::ECompressType, typename> typename _Ty,
         template <typename> typename _Ay,
         typename _Iy,
         bool _Sy,
         Compress::ECompressType _Ct=Compress::ECompressType::E_CT_Zstd,
         typename _Tag = stDefaultTag,
         uint8_t _Offset=12>
class ActorAgentSession : public _Ty<_Iy, _Sy, MsgActorAgentHeader<_Offset>, _Ct, _Tag>
{
protected :
        typedef _Ty<_Iy, _Sy, MsgActorAgentHeader<_Offset>, _Ct, _Tag> SuperType;
public :
        typedef ActorAgentSession<_Ty, _Ay, _Iy, _Sy, _Ct, _Tag, _Offset> ThisType;
        typedef _Iy ImplType;
        typedef _Ay<ImplType> ActorAgentType;
        typedef std::shared_ptr<ActorAgentType> ActorAgentTypePtr;
        typedef std::weak_ptr<ActorAgentType> ActorAgentTypeWeakPtr;
        typedef typename SuperType::MsgHeaderType MsgHeaderType;
        typedef typename SuperType::Tag Tag;
        constexpr static Compress::ECompressType CompressType = _Ct;

        struct stAgentListInfo
        {
                SpinLock _listMutex;
                boost::concurrent_flat_map<__uint128_t, ActorAgentTypeWeakPtr> _list;
        };
        typedef std::shared_ptr<stAgentListInfo> stAgentListInfoPtr;

        ActorAgentSession(typename SuperType::SocketType&& s
                          , std::size_t scSize = SuperType::cSendChannelSize
                          , std::size_t rcSize = SuperType::cRecvChannelSize)
                : SuperType(std::move(s), scSize, rcSize)
                  , _agentList("ActorAgentSession_agentList")
        {
                SuperType::SetAutoRebind();
        }

        ~ActorAgentSession() override
        {
        }

        void OnClose(int32_t reasonType) override
        {
                SuperType::OnClose(reasonType);

                // 必须在 OnClose 中添加：
                // 若将 info->_listMutex.lock() 放到 OnClose 中，info->_listMutex.unlock() 放到析构函数中，会出现死锁。
                // 只要重连够快，就会导致相同 SID ses 还没析构情况下，新的已经连上来，会在 ServerInit 中死锁。
                // 若有其它问题，请想其它办法解决。

                if (SuperType::IsAutoRebind())
                {
                        stAgentListInfoPtr info;
                        {
                                std::lock_guard l(_agentListBySIDMutex_);
                                auto it = _agentListBySID.find(SuperType::GetSID());
                                if (_agentListBySID.end() != it)
                                {
                                        info = it->second;
                                }
                                else
                                {
                                        info = std::make_shared<stAgentListInfo>();
                                        _agentListBySID.emplace(SuperType::GetSID(), info);
                                }
                        }

                        if (info)
                        {
                                std::lock_guard l(info->_listMutex);
                                _agentList.ForeachClear([this, &info](const ActorAgentTypeWeakPtr& weakAgent) {
                                        auto agent = weakAgent.lock();
                                        if (agent)
                                        {
                                                agent->SendInternal(E_MIMT_Local, E_MILST_Disconnect, nullptr);

                                                auto key = (__uint128_t)agent->GetID() << 64 | agent->GetLocalID();
                                                info->_list.emplace(key, weakAgent);
                                                LOG_INFO("ses[{}] 断开连接，添加到 _agentListBySID agent id[{}] local[{}]",
                                                         SuperType::GetSID(), agent->GetID(), agent->GetLocalID());
                                        }
                                });
                        }
                }
                else
                {
                        _agentList.ForeachClear([](const ActorAgentTypeWeakPtr& weakAgent) {
                                auto agent = weakAgent.lock();
                                if (agent)
                                        agent->SendInternal(E_MIMT_Local, E_MILST_Disconnect, nullptr);
                        });
                }
        }

        static void MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
        {
                SuperType::MsgHandleServerInit(ses, buf, bufRef);

                if (ses->IsAutoRebind())
                {
                        auto thisPtr = std::dynamic_pointer_cast<ImplType>(ses);
                        auto m = std::make_shared<MailResult>();
                        m->set_error_type(ses->IsRemoteCrash() ? E_IET_RemoteCrash : E_IET_None);

                        stAgentListInfoPtr info;
                        {
                                std::lock_guard l(_agentListBySIDMutex_);
                                auto it = _agentListBySID.find(ses->GetSID());
                                if (_agentListBySID.end() != it)
                                {
                                        info = it->second;
                                        // _agentListBySID.erase(it);
                                }
                        }

                        if (info)
                        {
                                std::lock_guard l(info->_listMutex);
                                decltype(info->_list) tmpList = std::move(info->_list);
                                tmpList.visit_all([ses, thisPtr, m](const auto& val) {
                                        auto agent = val.second.lock();
                                        if (agent)
                                        {
                                                agent->SetSession(thisPtr);
                                                thisPtr->AddAgent(agent);
                                                LOG_WARN("ses[{}] 重新连接，自动绑定 agent id[{}] local[{}]",
                                                         ses->GetSID(), agent->GetID(), agent->GetLocalID());

                                                agent->SendInternal(E_MIMT_Local, E_MILST_Reconnect, m);
                                        }
                                });
                        }
                }
        }

        static void AddAgentOffline(int64_t sid, const ActorAgentTypePtr& agent)
        {
                if (!agent)
                        return;
                auto key = (__uint128_t)agent->GetID() << 64 | agent->GetLocalID();

                stAgentListInfoPtr info;
                {
                        std::lock_guard l(_agentListBySIDMutex_);
                        auto it = _agentListBySID.find(sid);
                        if (_agentListBySID.end() != it)
                        {
                                info = it->second;
                        }
                        else
                        {
                                info = std::make_shared<stAgentListInfo>();
                                _agentListBySID.emplace(sid, info);
                        }
                }

                {
                        std::lock_guard l_(info->_listMutex);
                        info->_list.emplace(key, agent);
                }
        }

        void OnMessageMultiCast(ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef) override
        {
                auto msgMultiCastHead = *reinterpret_cast<typename MsgHeaderType::MsgMultiCastHeader*>(buf);
                /*
                LOG_WARN("peer send multi cast, but not override!!! mt[{:#x}] st[{:#x}] from[{}]",
                         MsgHeaderType::MsgMainType(msgMultiCastHead->_stype),
                         MsgHeaderType::MsgSubType(msgMultiCastHead->_stype),
                         msgMultiCastHead->_from);
                         */
                switch (msgMultiCastHead._type)
                {
                case MsgHeaderType::template MsgTypeMerge<E_MIMT_Internal, E_MIIST_MultiCast>() :
                        {
                                const auto bodySize = msgMultiCastHead._size - sizeof(typename MsgHeaderType::MsgMultiCastHeader) - sizeof(uint64_t) - msgMultiCastHead._ssize;
                                const auto except = *reinterpret_cast<uint64_t*>(buf + sizeof(typename MsgHeaderType::MsgMultiCastHeader) + bodySize);

                                MsgMultiCastInfo idListMsg;
                                Compress::UnCompressAndParseAlloc(static_cast<Compress::ECompressType>(msgMultiCastHead._sct),
                                                                  idListMsg,
                                                                  buf + sizeof(typename MsgHeaderType::MsgMultiCastHeader) + bodySize + sizeof(uint64_t),
                                                                  msgMultiCastHead._ssize);

                                new (buf) MsgHeaderType(sizeof(MsgHeaderType)+bodySize
                                                        , static_cast<Compress::ECompressType>(msgMultiCastHead._ct)
                                                        , MsgHeaderType::MsgMainType(msgMultiCastHead._stype)
                                                        , MsgHeaderType::MsgSubType(msgMultiCastHead._stype)
                                                        , MsgHeaderType::E_RMT_Send
                                                        , 0
                                                        , msgMultiCastHead._from
                                                        , 0);
                                auto msgHead = *reinterpret_cast<MsgHeaderType*>(buf);
                                for (auto id : idListMsg.id_list())
                                {
                                        if (id != except)
                                        {
                                                auto agent = GetAgent(msgHead._from, id);
                                                if (agent)
                                                {
                                                        auto mail = std::make_shared<ActorNetMail<ActorMail, MsgHeaderType>>(agent, msgHead, buf, bufRef);
                                                        agent->Push(mail);
                                                }
                                        }
                                }

                        }
                        break;
                case MsgHeaderType::template MsgTypeMerge<E_MIMT_Internal, E_MIIST_BroadCast>() :
                        LOG_WARN("收到广播消息，未处理!!!");

#if 0
                        new (buf) MsgHeaderType(msgMultiCastHead._size
                                                , static_cast<Compress::ECompressType>(msgMultiCastHead._ct)
                                                , MsgHeaderType::MsgMainType(msgMultiCastHead._stype)
                                                , MsgHeaderType::MsgSubType(msgMultiCastHead._stype)
                                                , MsgHeaderType::E_RMT_Send
                                                , 0
                                                , msgMultiCastHead._from
                                                , 0);
                        auto msgHead = *reinterpret_cast<MsgHeaderType*>(buf);
                        _playerList.Foreach([&bufRef, buf, msgHead](const PlayerWeakPtr& wp) {
                                auto p = wp.lock();
                                if (p)
                                {
                                        auto mail = std::make_shared<ActorNetMail<ActorMail, MsgHeaderType>>(nullptr, msgHead, buf, bufRef);
                                        p->Push(mail);
                                }
                        });
#endif
                        break;
                default: break;
                }
        }

        FORCE_INLINE void OnRecvCallRet(ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
        {
                auto msgHead = *reinterpret_cast<MsgHeaderType*>(buf);
                auto agent = GetAgent(msgHead._from, msgHead._to);
                if (agent)
                {
                        agent->RemoteCallRet(buf, bufRef);
                }
                else
                {
                        LOG_WARN("call ret 没找到agent!!! mt[{:#x}] st[{:#x}] to[{}] from[{}]",
                                 MsgHeaderType::MsgMainType(msgHead._type),
                                 MsgHeaderType::MsgSubType(msgHead._type),
                                 msgHead._to, msgHead._from);
                }
        }

        FORCE_INLINE void OnRecvNotFoundAgent(ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
        {
                auto msgHead = *reinterpret_cast<MsgHeaderType*>(buf);
                switch (msgHead._type)
                {
                case MsgHeaderType::template MsgTypeMerge<E_MIMT_Internal, E_MIIST_Result>() :
                        break;
                default :
                        SuperType::OnRecv(buf, bufRef);
                        break;
                }
        }

        void OnRecv(ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef) override
        {
                // 不能交换两个 switch 顺序，因 MultiCast 和 BroadCast 消息头和正常情况不一样，
                // 无法先进行 _flag 判断。
                auto msgHead = *reinterpret_cast<MsgHeaderType*>(buf);
                switch (msgHead._flag)
                {
                case MsgHeaderType::E_RMT_None :
                        SuperType::OnRecv(buf, bufRef);
                        break;
                case MsgHeaderType::E_RMT_CallRet :
                        reinterpret_cast<ImplType*>(this)->OnRecvCallRet(buf, bufRef);
                        break;
                default :
                        {
                                auto agent = GetAgent(msgHead._from, msgHead._to);
                                if (agent)
                                {
                                        agent->OnRecv(buf, bufRef);
                                }
                                else
                                {
                                        /*
                                           LOG_ERROR("收到消息 mainType[{:#x}] subType[{:#x}] 时，没找到 agent from[{}] to[{}]",
                                           mainType, subType, msgHead._from, msgHead._to);
                                           */
                                        reinterpret_cast<ImplType*>(this)->OnRecvNotFoundAgent(buf, bufRef);
                                        return;
                                }
                        }
                        break;
                }
        }

private :
        friend class ActorAgent<ImplType>;

public :
        FORCE_INLINE bool AddAgent(const ActorAgentTypePtr& agent)
        {
                if (agent)
                {
                        auto key = (__uint128_t)agent->GetID() << 64 | agent->GetLocalID();
                        /*
                        LOG_ERROR_IF(0==agent->GetID() || 0==agent->GetLocalID(),
                                     "添加0 id:{} localID:{}",
                                     agent->GetID(), agent->GetLocalID());
                                     */
                        bool ret = _agentList.Add(key, agent);
                        LOG_ERROR_IF(!ret, "添加失败!!!!!!!!! key:{} id:{} local:{}",
                                     key, agent->GetID(), agent->GetLocalID());
                        return ret;
                }
                return false;
        }

        FORCE_INLINE ActorAgentTypePtr GetAgent(uint64_t remote, uint64_t local)
        {
                auto key = (__uint128_t)remote << 64 | local;
                return _agentList.Get(key).lock();
        }
        
        FORCE_INLINE void RemoveAgent(uint64_t remote, uint64_t local, void* ptr)
        {
                auto key = (__uint128_t)remote << 64 | local;
                _agentList.Remove(key, ptr);
        }

        FORCE_INLINE std::size_t GetAgentCnt() { return _agentList.Size(); }

protected :
        ThreadSafeUnorderedMap<__uint128_t, ActorAgentTypeWeakPtr> _agentList;

public :
        static SpinLock _agentListBySIDMutex_;
        static std::unordered_map<int64_t, stAgentListInfoPtr> _agentListBySID;
};

template <template<typename, bool, typename, Compress::ECompressType, typename> typename _Ty,
         template <typename> typename _Ay,
         typename _Iy,
         bool _Sy,
         Compress::ECompressType _Ct,
         typename _Tag,
         uint8_t _Offset>
SpinLock ActorAgentSession<_Ty, _Ay, _Iy, _Sy, _Ct, _Tag, _Offset>::_agentListBySIDMutex_;

template <template<typename, bool, typename, Compress::ECompressType, typename> typename _Ty,
         template <typename> typename _Ay,
         typename _Iy,
         bool _Sy,
         Compress::ECompressType _Ct,
         typename _Tag,
         uint8_t _Offset>
std::unordered_map<int64_t, typename ActorAgentSession<_Ty, _Ay, _Iy, _Sy, _Ct, _Tag, _Offset>::stAgentListInfoPtr> ActorAgentSession<_Ty, _Ay, _Iy, _Sy, _Ct, _Tag, _Offset>::_agentListBySID;

// }}}

}; // end of namespace nl::net

// vim: fenc=utf8:expandtab:ts=8:noma
