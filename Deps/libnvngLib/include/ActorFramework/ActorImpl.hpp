#pragma once

#include "IActor.h"
#include "IService.h"
#include "DB/RedisMgr.hpp"
#include "Net/ISession.hpp"

namespace nl::af
{

struct stHttpReqReply : public stActorMailBase
{
        std::string _body;
};

#define EMPTY_CALL_RET() \
        do { \
                ActorCallMailPtr ret; \
                while (boost::fibers::channel_op_status::success == _ch.try_pop(ret)) { \
                        /* ++guid 由本 actor 调用，运行到此处之前因远程超时的 ++guid 已经操作，*/ \
                        /* 此时清空 _ch 后，远程 call 返回无法再加入到 _ch。*/ \
                        LOG_WARN("11111111111111111111111111 id[{}] mt[{:#x}] st[{:#x}]", GetID(), ActorMailType::MsgMainType(ret->_type), ActorMailType::MsgSubType(ret->_type)); \
                } \
        } while (0)

template <typename ImplType, typename ServiceType, typename _My=ActorMail, typename _Tag=stDefaultTag>
class ActorImpl : public IActor, public std::enable_shared_from_this<ActorImpl<ImplType, ServiceType, _My, _Tag>>
{
        typedef IActor SuperType;
        typedef ActorImpl<ImplType, ServiceType, _My, _Tag> ThisType;

public :
        typedef _My ActorMailType;
        typedef std::shared_ptr<ActorMailType> ActorMailTypePtr;

public :
        ActorImpl(uint64_t id, std::size_t qSize=1<<4)
                : _id(id)
                  , _ch(1 << 1)
                  , _msgQueue(Next2N(qSize))
        {
                LOG_ERROR_IF(!CHECK_2N(qSize), "qSize[{}] 必须设置为 2^N !!!", qSize);
        }

        ~ActorImpl() override
        {
        }

protected :
        virtual bool Init()
        {
                return true;
        }

public :
        void Terminate() override { FLAG_ADD(_flag, 1<<14); Push(nullptr); }
        FORCE_INLINE bool IsTerminate() const { return FLAG_HAS(_flag, 1<<14); }
        void WaitForTerminate() override
        {
                while (!FLAG_HAS(_flag, 1<<15))
                        boost::this_fiber::sleep_for(std::chrono::milliseconds(1));
        }

        uint64_t GetID() const override { return _id; }
        IService* GetService() override { return ServiceType::GetService(); }

        virtual bool Start(std::size_t stackSize = 32 * 1024)
        {
                auto thisPtr = reinterpret_cast<ImplType*>(this)->shared_from_this();
                if (!ServiceType::GetService()->AddActor(thisPtr))
                {
                        LOG_ERROR("actor 添加失败!!! id[{}]", GetID());
                        return false;
                }

                GetAppBase()->_mainChannel.push([thisPtr]() {
                        boost::fibers::fiber(std::allocator_arg,
                                             // boost::fibers::fixedsize_stack{ stackSize },
                                             boost::fibers::segmented_stack{},
                                             [thisPtr]() {
                                                     if (thisPtr->Init())
                                                     {
                                                             thisPtr->Run();
                                                     }
                                                     else
                                                     {
                                                             LOG_WARN("actor init fail!!! id[{}]", thisPtr->GetID());
                                                             thisPtr->Terminate();
                                                     }
                                                     ServiceType::GetService()->RemoveActor(thisPtr);
                                             }).detach();
                });

                return true;
        }

        void Push(const IActorMailPtr& m) override
        {
                if (boost::fibers::channel_op_status::success != _msgQueue.try_push(m))
                {
                        LOG_ERROR("send 阻塞!!!");
                        if (!IsTerminate())
                                _msgQueue.push(m);
                        else
                                LOG_WARN("actor id:{} 已停止，mail 被丢弃!!!", GetID());
                        LOG_WARN("send 阻塞结束!!!");
                }
        }

        FORCE_INLINE bool SendInternal(const IActorPtr& target,
                                       uint64_t mainType,
                                       uint64_t subType,
                                       const ActorMailDataPtr& msg)
        {
                if (target)
                {
                        target->SendPush(ThisType::shared_from_this(), mainType, subType, msg);
                        return true;
                }
                return false;
        }

        using SuperType::SendPush;
        void SendPush(const IActorPtr& from,
                      uint64_t mainType,
                      uint64_t subType,
                      const ActorMailDataPtr& msg) override
        {
                auto m = std::make_shared<ActorMailType>(from, msg, ActorMailType::MsgTypeMerge(mainType, subType));
                Push(m);
        }

        ActorCallMailPtr AfterCallPush(boost::fibers::buffered_channel<ActorCallMailPtr>& ch,
                                       uint64_t mt,
                                       uint64_t st,
                                       uint16_t& guid) override
        {
                ActorCallMailPtr ret = ch.value_pop();
                if (ActorMailType::MsgTypeMerge(mt, st) == ret->_type)
                {
                        return ret;
                }
                else
                {
                        LOG_WARN("id[{}] 收到 Call 回复消息类型不匹配!!! ret[{:#x}] call[{:#x}]",
                                 GetID(), ret->_type, ActorMailType::MsgTypeMerge(mt, st));
                        return nullptr;
                }
        }

        FORCE_INLINE ActorMailPtr CallInternal(const IActorPtr& target,
                                                 uint64_t mainType,
                                                 uint64_t subType,
                                                 const ActorMailDataPtr& msg)
        {
#ifdef ____BENCHMARK____
                LOG_FATAL_IF(!target, "22222222222222222222222222 id[{}]", GetID());
#endif
                uint16_t guid = GetCallGuid();
                auto thisPtr = ThisType::shared_from_this();
                EMPTY_CALL_RET();
                if (target && target->CallPushInternal(thisPtr, mainType, subType, msg, guid))
                {
                        PauseCostTime();
                        auto ret = target->AfterCallPush(_ch, mainType, subType, guid);
                        ResumeCostTime();
                        SetCallGuid(guid);
                        assert(GetCallGuid() <= 0xFFF);
                        return ret;
                }
                return nullptr;
        }

        template <typename TargetType>
        FORCE_INLINE ActorMailPtr CallInternal(const std::shared_ptr<TargetType>& target,
                                               uint64_t mainType,
                                               uint64_t subType,
                                               const ActorMailDataPtr& msg,
                                               const std::shared_ptr<void>& bufRef,
                                               const char* buf,
                                               std::size_t bufSize)
        {
#ifdef ____BENCHMARK____
                LOG_FATAL_IF(!target, "22222222222222222222222222 id[{}]", GetID());
#endif
                uint16_t guid = GetCallGuid();
                auto thisPtr = ThisType::shared_from_this();
                EMPTY_CALL_RET();
                if (target && target->CallPushInternal(thisPtr, mainType, subType, msg, guid, bufRef, buf, bufSize))
                {
                        PauseCostTime();
                        auto ret = target->AfterCallPush(_ch, mainType, subType, guid);
                        ResumeCostTime();
                        SetCallGuid(guid);
                        assert(GetCallGuid() <= 0xFFF);
                        return ret;
                }
                return nullptr;
        }

        bool CallPushInternal(const IActorPtr& from,
                      uint64_t mainType,
                      uint64_t subType,
                      const ActorMailDataPtr& msg,
                      uint16_t guid) override
        {
                auto m = std::make_shared<ActorCallMail>(from, msg, ActorMailType::MsgTypeMerge(mainType, subType), guid);
                Push(m);
                return true;
        }

        void CallRet(const ActorMailDataPtr& msg,
                     uint16_t guid,
                     uint64_t mainType,
                     uint64_t subType) override
        {
                // 不能改成 TryPush，可能因异步导致掉包。
                auto mail = std::make_shared<ActorCallMail>(nullptr, msg, ActorMailType::MsgTypeMerge(mainType, subType), guid);
                if (boost::fibers::channel_op_status::success != _ch.try_push(mail))
                {
                        LOG_ERROR("call ret 阻塞!!! id[{}] mt[{:#x}] st[{:#x}] guid[{}]",
                                  GetID(), mainType, subType, guid);

                        _ch.push(mail);

                        LOG_WARN("call ret 阻塞结束!!! id[{}] mt[{:#x}] st[{:#x}] guid[{}]",
                                 GetID(), mainType, subType, guid);
                }
        }

        void RemoteCallRet(uint64_t mainType, uint64_t subType, uint16_t guid, const ActorCallMailPtr& mail) override
        {
                // 必须，可避免过多超时包进入 _ch，从而阻塞网络线程。
                if (guid == GetCallGuid())
                {
                        if (boost::fibers::channel_op_status::success != _ch.try_push(mail))
                        {
                                LOG_ERROR("remote call ret 阻塞!!! id[{}] mt[{:#x}] st[{:#x}] guid[{}]",
                                          GetID(), mainType, subType, guid);

                                _ch.push(mail);

                                LOG_WARN("remote call ret 阻塞结束!!! id[{}] mt[{:#x}] st[{:#x}] guid[{}]",
                                         GetID(), mainType, subType, guid);
                        }
                }
                else
                {
                        LOG_WARN("远程回复本地 call ret 失败，id[{}] guid[{}] flag[{}] now[{}] mt[{:#x}] st[{:#x}]",
                                  GetID(), guid, GetCallGuid(), GetClock().GetTimeStamp(), mainType, subType);
                }
        }

        FORCE_INLINE void SetCallGuid(uint16_t t) { _flag = (_flag & 0xF000) | (t & 0xFFF); }
        FORCE_INLINE uint16_t GetCallGuid() const { return _flag & 0xFFF; }

public :
        // 必须使用引用，外部使用 std::string，此处如果复制，导致redis cmd 中 std::string_view 野指针。
        template <typename ... Args>
        std::shared_ptr<db::stReplayMailInfo> RedisCmd(const Args& ... args)
        {
                EMPTY_CALL_RET();
                db::RedisMgrBase<_Tag>::GetInstance()->Exec(ThisType::shared_from_this(), std::forward<const Args&>(args)...);
                PauseCostTime();
                auto ret = _ch.value_pop();
                ResumeCostTime();
                return std::dynamic_pointer_cast<db::stReplayMailInfo>(ret->_msg);
        }

        FORCE_INLINE std::shared_ptr<stHttpReqReply> HttpReq(std::string_view url, std::string_view body, std::string_view contentType = "application/json;charset=UTF-8")
        {
                uint16_t guid = GetCallGuid();
                auto thisPtr = ThisType::shared_from_this();
                EMPTY_CALL_RET();

                std::weak_ptr<ThisType> weakAct = thisPtr;
                ::nl::net::NetMgrBase<_Tag>::GetInstance()->HttpReq(url, body, [weakAct, guid](std::string_view body, const auto& ec) {
                        if (!ec)
                        {
                                auto act = weakAct.lock();
                                if (act)
                                {
                                        auto msg = std::make_shared<stHttpReqReply>();
                                        msg->_body = std::move(body);
                                        act->CallRet(msg, 0, 0, guid);
                                }
                        }
                        else
                        {
                                if (boost::asio::error::timed_out == ec.value())
                                {
                                        auto act = weakAct.lock();
                                        LOG_WARN("http req time out id[{}]", act ? act->GetID() : 0);
                                }
                        }
                }, contentType);

                PauseCostTime();
                auto ret = AfterCallPush(_ch, 0, 0, guid);
                ResumeCostTime();
                return std::dynamic_pointer_cast<stHttpReqReply>(ret->_msg);
        }

protected :
        void Run()
        {
                auto handlerList = GetHandlerList();
                IActorMailPtr mail;
                while (!IsTerminate())
                {
                        IActorMailPtr mail = _msgQueue.value_pop();
                        if (mail)
                        {
#ifdef ____PRINT_ACTOR_MAIL_COST_TIME____
                                _clock = clock();
                                clock_t realClock = _clock;
#endif

                                try
                                {
                                        mail->Run(this, handlerList);
                                }
                                catch (...)
                                {
                                        LOG_FATAL("11111111111111111111111111 type[{}]", mail->Type());
                                }

#ifdef ____PRINT_ACTOR_MAIL_COST_TIME____
                                clock_t end = clock();
                                double diff = (double)(end-_clock)/CLOCKS_PER_SEC;
                                double real = (double)(end-realClock)/CLOCKS_PER_SEC;
                                auto type = mail->Type();
                                LOG_WARN_IF(diff>=0.001 || real>=0.001,
                                            "{}",fmt::sprintf("mail type[%#x] mt[%#x] st[%#x] cost[%lfs] real[%lfs]",
                                                              type,
                                                              ActorMailType::MsgMainType(type),
                                                              ActorMailType::MsgSubType(type),
                                                              diff,
                                                              real));
#endif
                                mail.reset();
                        }
                }
                FLAG_ADD(_flag, 1<<15);
        }
        
public :
        FORCE_INLINE void PauseCostTime()
        {
#ifdef ____PRINT_ACTOR_MAIL_COST_TIME____
                _clock = clock() - _clock;
#endif
        }

        FORCE_INLINE void ResumeCostTime()
        {
#ifdef ____PRINT_ACTOR_MAIL_COST_TIME____
                _clock = clock() - _clock;
#endif
        }

private :
        std::atomic_uint_fast16_t _flag = 0;
        const uint32_t _id = 0;
#ifdef ____PRINT_ACTOR_MAIL_COST_TIME____
        clock_t _clock = 0;
#endif

protected :
        // 去重复，因超时，导致后面的原来的call返回造成不一致问题。
        boost::fibers::buffered_channel<ActorCallMailPtr> _ch;
        boost::fibers::buffered_channel<IActorMailPtr> _msgQueue;

public :
        FORCE_INLINE static void RegisterMailHandler(uint64_t mainType, uint64_t subType, typename ActorMailType::HandleType cb)
        {
                static auto handlerList = GetHandlerList();
                uint64_t msgType = ActorMailType::MsgTypeMerge(mainType, subType);
                assert(nullptr == handlerList[msgType]);
                handlerList[msgType] = cb;
        }

        FORCE_INLINE static typename ActorMailType::HandleType* GetHandlerList()
        {
                static typename ActorMailType::HandleType _handlerList[ActorMailType::scArraySize];
                return _handlerList;
        }
};

template <typename _Ty>
std::shared_ptr<_Ty> ParseMailData(ActorMail* mail, uint64_t mt, uint64_t st)
{
        if (nullptr == mail)
                return nullptr;

        // NOTE: 禁止将 std::dynamic_pointer_cast 改为 std::reinterpret_pointer_cast，
        // 正确远比效率重要，多线程环境中，效率其次，正确第一。
        // std::dynamic_pointer_cast debug 下，1000*1000 0.08s。release 下，1亿次，1.7s。
        switch (mail->Flag())
        {
        case 2 :
                {
                        auto ret = std::make_shared<_Ty>();
                        if (!mail->Parse(*ret))
                                LOG_ERROR("stNetBufferActorMailData parse error!!! mt[{:#x}] st[{:#x}]", mt, st);
                        return ret;
                }
                break;
        default :
                {
#ifdef ____BENCHMARK____
                        LOG_FATAL_IF(!mail->_msg, "eeeeeeeeeeeeeeeeeeeeeeeee msg:{} mt[{:#x}] st[{:#x}]",
                                     fmt::ptr(mail->_msg.get()), mt, st);
#endif
                        auto ret = std::dynamic_pointer_cast<_Ty>(mail->_msg);
#ifdef ____BENCHMARK____
                        LOG_FATAL_IF(!ret, "fffffffffffffffffffffffff msg:{} mt[{:#x}] st[{:#x}]",
                                     fmt::ptr(mail->_msg.get()), mt, st);
#endif
                        return ret;
                }
                break;
        }
        return nullptr;
}

#define EXTERN_ACTOR_MAIL_HANDLE() \
        public : \
        template <std::size_t, std::size_t, typename _Mt> ActorMailDataPtr MailHandle(const IActorPtr&, const std::shared_ptr<_Mt>&); \
        template <std::size_t, std::size_t> ActorMailDataPtr MailHandle(const IActorPtr&)

template <typename _Ay, std::size_t _Mt, std::size_t _St, typename _My>
struct stRegistActorMailHandleProxy {
        stRegistActorMailHandleProxy() { _Ay::RegisterMailHandler(_Mt, _St, MailHandle); }
        static void MailHandle(IActor* act, typename _Ay::ActorMailType* mail) {
                auto from = mail->_from.lock(); /* 必须在此持有，否则回调中若删除，则 Finish 会出问题。*/
                auto msg = ParseMailData<_My>(mail, _Mt, _St);
                auto ret = dynamic_cast<_Ay*>(act)->template MailHandle<_Mt, _St, _My>(from, msg);
                mail->Finish(ret);
        }
};

#define ACTOR_MAIL_HANDLE_BASE(ay, mt, st, my) \
        namespace _##ay##_##mt##_##st##_##my { stRegistActorMailHandleProxy<ay, mt, st, my> _; }; \
        template <> ActorMailDataPtr ay::MailHandle<mt, st, my>(const IActorPtr& from, const std::shared_ptr<my>& msg)

template <typename _Ay, std::size_t _Mt, std::size_t _St>
struct stRegistActorMailHandleProxy<_Ay, _Mt, _St, int> {
        stRegistActorMailHandleProxy() { _Ay::RegisterMailHandler(_Mt, _St, MailHandle); }
        static void MailHandle(IActor* act, typename _Ay::ActorMailType* mail) {
                auto from = mail->_from.lock(); /* 必须在此持有，否则回调中若删除，则 Finish 会出问题。*/
                auto ret = dynamic_cast<_Ay*>(act)->template MailHandle<_Mt, _St>(from);
                mail->Finish(ret);
        }
};

#define ACTOR_MAIL_HANDLE_EMPTY(ay, mt, st) \
        namespace _##ay##_##mt##_##st { stRegistActorMailHandleProxy<ay, mt, st, int> _; }; \
        template <> ActorMailDataPtr ay::MailHandle<mt, st>(const IActorPtr& from)

#define ACTOR_GET_MAIL_HANDLE_FUNC(_1, _2, _3, _4, FUNC_NAME, ...)      FUNC_NAME
#define ACTOR_MAIL_HANDLE(...)     ACTOR_GET_MAIL_HANDLE_FUNC(__VA_ARGS__, ACTOR_MAIL_HANDLE_BASE, ACTOR_MAIL_HANDLE_EMPTY, ...)(__VA_ARGS__)

#define ACTOR_CALL_1(retType, from, to, mt, st, msg)    ParseMailData<retType>(from->CallInternal(to, mt, st, msg).get(), mt, st)
#define ACTOR_CALL_2(retType, to, mt, st, msg)          ParseMailData<retType>(CallInternal(to, mt, st, msg).get(), mt, st)
#define ACTOR_CALL_HANDLE_FUNC(_1, _2, _3, _4, _5, _6, FUNC_NAME, ...)      FUNC_NAME
#define Call(...)     ACTOR_CALL_HANDLE_FUNC(__VA_ARGS__, ACTOR_CALL_1, ACTOR_CALL_2, ...)(__VA_ARGS__)

#define ACTOR_SEND_1(from, to, mt, st, msg)    from->SendInternal(to, mt, st, msg)
#define ACTOR_SEND_2(to, mt, st, msg)          SendInternal(to, mt, st, msg)
#define ACTOR_SEND_HANDLE_FUNC(_1, _2, _3, _4, _5, FUNC_NAME, ...)      FUNC_NAME
#define Send(...)     ACTOR_SEND_HANDLE_FUNC(__VA_ARGS__, ACTOR_SEND_1, ACTOR_SEND_2, ...)(__VA_ARGS__)

}; // end of namespace nl::af

// vim: fenc=utf8:expandtab:ts=8:noma
