#pragma once

#include "ActorFramework/ServiceImpl.hpp"
#include "ActorFramework/ActorImpl.hpp"

namespace nl::af
{

class SpecialActorMgr : public ServiceImpl, public Singleton<SpecialActorMgr>
{
        SpecialActorMgr() : ServiceImpl("SpecialActorMgr") { }
        ~SpecialActorMgr() override { }
        friend class Singleton<SpecialActorMgr>;

public :
        static SpecialActorMgr* GetService() { return SpecialActorMgr::GetInstance(); }
        FORCE_INLINE static uint64_t GenActorID()
        {
                static std::atomic_uint64_t _guid = 0;
                return ++_guid;
        }
};

class SpecialActor : public ActorImpl<SpecialActor, SpecialActorMgr>
{
        typedef ActorImpl<SpecialActor, SpecialActorMgr> SuperType;
protected :
        SpecialActor(std::size_t queueSize = 1 << 16)
                : SuperType(SpecialActorMgr::GenActorID(), queueSize)
        {
        }

        ~SpecialActor() override
        {
        }

        friend class ActorImpl<SpecialActor, SpecialActorMgr>;
        DECLARE_SHARED_FROM_THIS(SpecialActor);
};

#define SPECIAL_ACTOR_DEFINE_BEGIN(at, mt) \
        struct stSpecialActorMainType##mt { }; \
        constexpr static uint64_t sc##at##MailMainType = mt; \
        static_assert(0<=mt && mt<=nl::af::SpecialActor::ActorMailType::scMainTypeMax); \
        class at; \
        typedef std::shared_ptr<at> at##Ptr; \
        typedef std::weak_ptr<at> at##WeakPtr; \
        class at : public nl::af::SpecialActor { \
                typedef nl::af::SpecialActor SuperType; \
                using SuperType::SendPush; \
        public : \
                FORCE_INLINE void SendPush(const CallbackMail::CallbackType& cb) \
                { SuperType::SendPush(cb); } \
                FORCE_INLINE void SendPush(uint64_t subType, const ActorMailDataPtr& msg) \
                { return SuperType::SendPush(nullptr, sc##at##MailMainType, subType, msg); } \
                FORCE_INLINE void SendPush(const IActorPtr& from, uint64_t subType, const ActorMailDataPtr& msg) \
                { return SuperType::SendPush(from, sc##at##MailMainType, subType, msg); }

#define SPECIAL_ACTOR_DEFINE_END(at) \
                DECLARE_SHARED_FROM_THIS(at); \
                EXTERN_ACTOR_MAIL_HANDLE(); \
        }

#define SPECIAL_ACTOR_MAIL_HANDLE_BASE(at, st, dt) ACTOR_MAIL_HANDLE(at, sc##at##MailMainType, st, dt)
#define SPECIAL_ACTOR_MAIL_HANDLE_EMPTY(at, st)    ACTOR_MAIL_HANDLE(at, sc##at##MailMainType, st)

#define SPECIAL_ACTOR_GET_MAIL_HANDLE_FUNC(_1, _2, _3, FUNC_NAME, ...)      FUNC_NAME
#define SPECIAL_ACTOR_MAIL_HANDLE(...)     SPECIAL_ACTOR_GET_MAIL_HANDLE_FUNC(__VA_ARGS__, SPECIAL_ACTOR_MAIL_HANDLE_BASE, SPECIAL_ACTOR_MAIL_HANDLE_EMPTY, ...)(__VA_ARGS__)

}; // end of namespace nl::af

// vim: fenc=utf8:expandtab:ts=8
