#pragma once

#include "Tools/Util.h"
#include "Tools/LogHelper.h"

namespace nl::af
{

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
        virtual ActorCallMailPtr AfterCallPush(boost::fibers::buffered_channel<ActorCallMailPtr>& ch,
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

}; // end of namespace nl::af

// vim: fenc=utf8:expandtab:ts=8:noma
