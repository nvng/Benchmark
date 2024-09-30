#pragma once

#include "Tools/Util.h"
#include "Tools/LogHelper.h"

namespace nl::af
{

// {{{

class IActor;
typedef std::shared_ptr<IActor> IActorPtr;
typedef std::weak_ptr<IActor> IActorWeakPtr;

typedef std::shared_ptr<google::protobuf::MessageLite> ActorMailDataPtr;

class IActorMail
{
public :
        virtual ~IActorMail() {}

        virtual uint64_t Type() const { return 0; }
        virtual IActorPtr GetFrom() { return nullptr; }
        virtual uint64_t Flag() const { return 0; }
        virtual ActorMailDataPtr GetData() { return nullptr; }
        virtual void Finish(const ActorMailDataPtr& msg) { }
        virtual bool Parse(google::protobuf::MessageLite& pb) { return false; }
        virtual std::tuple<std::shared_ptr<void>, std::string_view, bool> ParseExtra(google::protobuf::MessageLite& pb) { return { nullptr, "", false, }; }
        virtual void Run(IActor* act, void* handlerList) = 0;
};
typedef std::shared_ptr<IActorMail> IActorMailPtr;

class ActorMail : public IActorMail
{
public :
        typedef void(*HandleType)(IActor*, ActorMail*);
public :
        ActorMail(const IActorPtr& from,
                  const ActorMailDataPtr& msg,
                  uint64_t type)
                : _from(from)
                  , _msg(msg)
                  , _type(type)
        {
        }

        uint64_t Type() const override { return _type; }
        IActorPtr GetFrom() override { return _from.lock(); }
        virtual const google::protobuf::MessageLite* GetPB() const { return nullptr; }
        ActorMailDataPtr GetData() override { return _msg; }
        void Run(IActor* act, void* handlerList) override
        {
                auto cb = reinterpret_cast<HandleType*>(handlerList)[_type];
                if (cb)
                {
                        cb(act, const_cast<ActorMail*>(this));
                }
                else
                {
                        LOG_WARN("actor 收到邮件，但没有处理函数!!! type[{:#x}] mainType[{:#x}] subType[{:#x}]",
                                  _type, MsgMainType(_type), MsgSubType(_type));
                }
        }

        const IActorWeakPtr _from;
        const ActorMailDataPtr _msg;
        const uint16_t _type = 0;

        DEFINE_MSG_TYPE_MERGE(12);
};
typedef std::shared_ptr<ActorMail> ActorMailPtr;

class CallbackMail : public IActorMail
{
public :
        typedef std::function<void()> CallbackType;
        CallbackMail(const CallbackType& cb) : _cb(std::move(cb)) { }
        uint64_t Flag() const override { return 3; }
        void Run(IActor* act, void* handlerList) override { _cb(); }
        CallbackType _cb;
};

// }}}

// {{{ channel wapper

template <typename _Ty>
struct stChannelWapper
{
        typedef boost::fibers::channel_op_status channel_op_status;
        typedef typename boost::fibers::buffered_channel<_Ty>::value_type value_type;

        explicit stChannelWapper( std::size_t capacity)
                : _ch(capacity)
        {
        }

        stChannelWapper( stChannelWapper const& other) = delete;
        stChannelWapper & operator=( stChannelWapper const& other) = delete;

        void close() noexcept
        {
                try
                {
                        _ch.close();
                }
                catch(...)
                {
                        LOG_FATAL("");
                }
        }

        channel_op_status push( value_type const& va)
        {
                try
                {
                        return _ch.push(va);
                }
                catch(...)
                {
                        LOG_FATAL("");
                }
                return channel_op_status::success;
        }

        channel_op_status push( value_type && va)
        {
                try
                {
                        return _ch.push(std::move(va));
                }
                catch(...)
                {
                        LOG_FATAL("");
                }
                return channel_op_status::success;
        }

        template< typename Rep, typename Period >
        channel_op_status push_wait_for(value_type const& va, std::chrono::duration< Rep, Period > const& timeout_duration)
        {
                try
                {
                        return _ch.push_wait_for(va, timeout_duration);
                }
                catch(...)
                {
                        LOG_FATAL("");
                }
                return channel_op_status::success;
        }

        template< typename Rep, typename Period >
        channel_op_status push_wait_for( value_type && va, std::chrono::duration< Rep, Period > const& timeout_duration)
        {
                try
                {
                        return _ch.push_wait_for(va, timeout_duration);
                }
                catch(...)
                {
                        LOG_FATAL("");
                }
                return channel_op_status::success;
        }

        template< typename Clock, typename Duration >
        channel_op_status push_wait_until(value_type const& va, std::chrono::time_point< Clock, Duration > const& timeout_time)
        {
                try
                {
                        return _ch.push_wait_until(va, timeout_time);
                }
                catch(...)
                {
                        LOG_FATAL("");
                }
                return channel_op_status::success;
        }

        template< typename Clock, typename Duration >
        channel_op_status push_wait_until(value_type && va, std::chrono::time_point< Clock, Duration > const& timeout_time)
        {
                try
                {
                        return _ch.push_wait_until(va, timeout_time);
                }
                catch(...)
                {
                        LOG_FATAL("");
                }
                return channel_op_status::success;
        }

        channel_op_status try_push( value_type const& va)
        {
                try
                {
                        return _ch.try_push(va);
                }
                catch(...)
                {
                        LOG_FATAL("");
                }
                return channel_op_status::success;
        }

        channel_op_status try_push( value_type && va)
        {
                try
                {
                        return _ch.try_push(std::move(va));
                }
                catch(...)
                {
                        LOG_FATAL("");
                }
                return channel_op_status::success;
        }

        channel_op_status pop( value_type & va)
        {
                try
                {
                        return _ch.pop(va);
                }
                catch(...)
                {
                        LOG_FATAL("");
                }
                return channel_op_status::success;
        }

        value_type value_pop()
        {
                try
                {
                        return _ch.value_pop();
                }
                catch(...)
                {
                        LOG_FATAL("");
                }
                return value_type();
        }

        template< typename Rep, typename Period >
        channel_op_status pop_wait_for(value_type & va, std::chrono::duration< Rep, Period > const& timeout_duration)
        {
                try
                {
                        return _ch.pop_wait_for(va, timeout_duration);
                }
                catch(...)
                {
                        LOG_FATAL("");
                }
                return channel_op_status::success;
        }

        template< typename Clock, typename Duration >
        channel_op_status pop_wait_until(value_type & va, std::chrono::time_point< Clock, Duration > const& timeout_time)
        {
                try
                {
                        return _ch.pop_wait_until(va, timeout_time);
                }
                catch(...)
                {
                        LOG_FATAL("");
                }
                return channel_op_status::success;
        }

        channel_op_status try_pop( value_type & va)
        {
                try
                {
                        return _ch.try_pop(va);
                }
                catch(...)
                {
                        LOG_FATAL("");
                }
                return channel_op_status::success;
        }

        boost::fibers::buffered_channel<_Ty> _ch;
};

/*
template <typename _Ty>
using channel_t = stChannelWapper<_Ty>;
*/

template <typename _Ty>
using channel_t = boost::fibers::buffered_channel<_Ty>;

// }}}

// {{{ IActor

class ActorCallMail;
typedef std::shared_ptr<ActorCallMail> ActorCallMailPtr;

class IService;
class IActor
{
public :
        virtual ~IActor() { }

        virtual uint64_t GetType() const { return 0; }
        virtual uint64_t GetID() const { assert(false); return 0; }
        virtual IService* GetService() { assert(false); return nullptr; }

        virtual void Push(const IActorMailPtr& m) { assert(false); }

        FORCE_INLINE void SendPush(const CallbackMail::CallbackType& cb)
        {
                auto mail = std::make_shared<CallbackMail>(std::move(cb));
                Push(mail);
        }

        virtual void SendPush(const IActorPtr& from,
                              uint64_t mainType,
                              uint64_t subType,
                              const ActorMailDataPtr& msg)
        {
                assert(false);
        }

        virtual bool CallPushInternal(const IActorPtr& from,
                              uint64_t mainType,
                              uint64_t subType,
                              const ActorMailDataPtr& msg,
                              uint16_t guid)
        {
                assert(false);
                return false;
        }

        virtual bool CallPushInternal(const IActorPtr& from,
                                      uint64_t mainType,
                                      uint64_t subType,
                                      const ActorMailDataPtr& msg,
                                      uint16_t guid,
                                      const std::shared_ptr<void>& bufRef,
                                      const char* buf,
                                      std::size_t bufSize)
        {
                assert(false);
                return false;
        }

        // 调用方式：target->AfterCallPush(_ch); 由 from 提供 _ch，target 提供等待方式。
        virtual ActorCallMailPtr AfterCallPush(channel_t<ActorCallMailPtr>& ch,
                                               uint64_t mt,
                                               uint64_t st,
                                               uint16_t& guid)
        {
                assert(false);
                return nullptr;
        }

        virtual void CallRet(const ActorMailDataPtr& msg,
                             uint16_t guid,
                             uint64_t mainType,
                             uint64_t subType)
        {
                assert(false);
        }

        virtual void RemoteCallRet(uint64_t mainType, uint64_t subType, uint16_t guid, const ActorCallMailPtr& mail) { assert(false); }

        virtual void Terminate() { }
        virtual void WaitForTerminate() { }

public :
        static constexpr time_t scCallRemoteTimeOut = 5;
};

// }}}

// {{{ actor mail

class ActorCallMail : public ActorMail
{
public :
        ActorCallMail(const IActorPtr& from,
                      const ActorMailDataPtr& msg,
                      uint64_t type,
                      uint16_t guid)
                : ActorMail(from, msg, type)
                  , _guid(guid)
        {
        }

        void Finish(const ActorMailDataPtr& msg) override
        {
                if (!msg)
                        return;
                // LOG_ERROR_IF(!msg, "call ret is nullptr!!! mt[{:#x}] st[{:#x}]", MsgMainType(_type), MsgSubType(_type));
                auto f = _from.lock();
                if (f)
                        f->CallRet(msg, _guid, MsgMainType(_type), MsgSubType(_type));
        }

        const uint16_t _guid = 0;
};

class TimerMail : public IActorMail
{
public :
        TimerMail(TimerGuidType guid, const std::function<void(TimerGuidType)>& cb)
                : _timerGuid(guid)
                  , _cb(std::move(cb))
        {
        }

        uint64_t Flag() const override { return 1; }
        void Run(IActor* act, void* handlerList) override { _cb(_timerGuid); }

        const TimerGuidType _timerGuid = 0;
        std::function<void(TimerGuidType)> _cb;
};

// }}}

}; // end of namespace nl::af

// vim: fenc=utf8:expandtab:ts=8:noma
