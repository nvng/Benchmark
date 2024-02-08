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

#define SPECIAL_ACTOR_DEFINE_BEGIN_EXTRA(at, mt) \
        constexpr static uint64_t sc##at##MailMainType = mt; \
        constexpr static uint64_t sc##at##HandleArraySize = ActorMail::scSubTypeMax + 1; \
        class at : public ActorImpl<at, SpecialActorMgr, ActorMail, ActorMail::scArraySize> { \
                friend class ActorImpl<at, SpecialActorMgr, ActorMail, ActorMail::scArraySize>; \
                typedef ActorImpl<at, SpecialActorMgr, ActorMail, ActorMail::scArraySize> SuperType; \
        public : \
                using SuperType::SendPush; \
                FORCE_INLINE void SendPush(uint64_t subType, const ActorMailDataPtr& msg) \
                { return SuperType::SendPush(nullptr, sc##at##MailMainType, subType, msg); } \
                FORCE_INLINE void SendPush(const IActorPtr& from, uint64_t subType, const ActorMailDataPtr& msg) \
                { return SuperType::SendPush(from, sc##at##MailMainType, subType, msg); } \
                void Push(const IActorMailPtr& m) override { \
                        const auto mainType = SuperType::ActorMailType::MsgMainType(m->Type()); \
                        const auto subType = SuperType::ActorMailType::MsgSubType(m->Type()); \
                        const bool checkTypeRet = 0 != m->Flag() || (mt==mainType && 0<=subType && subType<sc##at##HandleArraySize); \
                        LOG_ERROR_PS_IF(!checkTypeRet \
                                        , "{} Push 时消息 type[{:#x}] 出错!!! flag[{}] mt[{:#x}] mainType[{:#x}] HandleArraySize[{}]" \
                                        , #at, subType, m->Flag(), mt, mainType, sc##at##HandleArraySize); \
                        if (checkTypeRet && boost::fibers::channel_op_status::success != _msgQueue.try_push(m)) { \
                                LOG_ERROR("{} send 阻塞!!! subType[{:#x}]", #at, subType); \
                                if (!IsTerminate()) \
                                        _msgQueue.push(m); \
                                else \
                                        LOG_WARN("{} id:{} 已停止，mail 被丢弃!!! subType[{:#x}]", #at, GetID(), subType); \
                                LOG_WARN("{} send 阻塞结束!!! subType[{:#x}]", #at, subType); \
                        } \
                } \
                void Run() override { \
                        auto handlerList = SuperType::GetHandlerList(); \
                        while (!IsTerminate()) { \
                                IActorMailPtr mail = _msgQueue.value_pop(); \
                                if (mail) { \
                                        const auto mainType = SuperType::ActorMailType::MsgMainType(mail->Type()); \
                                        const auto subType = SuperType::ActorMailType::MsgSubType(mail->Type()); \
                                        if (0!=mail->Flag() || (mt==mainType && 0<=subType && subType<sc##at##HandleArraySize)) \
                                                mail->Run(this, handlerList[subType]); \
                                        else \
                                                LOG_ERROR("处理邮件时 type[{:#x}] 检查出错!!! flag[{}] mt[{:#x}] mainType[{:#x}] HandleArraySize[{}]" \
                                                          , subType, mail->Flag(), mt, mainType, sc##at##HandleArraySize); \
                                } \
                        } \
                        FLAG_ADD(SuperType::_flag, 1<<15); \
                } \
                FORCE_INLINE static void RegisterMailHandler(uint64_t mainType, uint64_t subType, typename ActorMailType::HandleType cb) { \
                        auto handlerList = GetHandlerList(); \
                        LOG_FATAL_IF(mt!=mainType ||  subType<0 || sc##at##HandleArraySize<=subType || nullptr != handlerList[subType] \
                                     , "{} 重复注册消息!!! mt[{:#x}] mainType[{:#x}] subType[{:#x}] HandleArraySize[{}]" \
                                     , #at, mt, mainType, subType, sc##at##HandleArraySize); \
                        handlerList[subType] = cb; \
                }

#define SPECIAL_ACTOR_DEFINE_END(at) \
                DECLARE_SHARED_FROM_THIS(at); \
                EXTERN_ACTOR_MAIL_HANDLE(); \
        }; \
        typedef std::shared_ptr<at> at##Ptr; \
        typedef std::weak_ptr<at> at##WeakPtr

#define SPECIAL_ACTOR_DEFINE_BEGIN_BASE(at)     SPECIAL_ACTOR_DEFINE_BEGIN_EXTRA(at, 0)
#define SPECIAL_ACTOR_DEFINE_BEGIN_FUNC(_1, _2, FUNC_NAME, ...)      FUNC_NAME
#define SPECIAL_ACTOR_DEFINE_BEGIN(...)         SPECIAL_ACTOR_DEFINE_BEGIN_FUNC(__VA_ARGS__, SPECIAL_ACTOR_DEFINE_BEGIN_EXTRA, SPECIAL_ACTOR_DEFINE_BEGIN_BASE, ...)(__VA_ARGS__)

#define SPECIAL_ACTOR_MAIL_HANDLE_BASE(at, st, dt) ACTOR_MAIL_HANDLE(at, sc##at##MailMainType, st, dt)
#define SPECIAL_ACTOR_MAIL_HANDLE_EMPTY(at, st)    ACTOR_MAIL_HANDLE(at, sc##at##MailMainType, st)

#define SPECIAL_ACTOR_GET_MAIL_HANDLE_FUNC(_1, _2, _3, FUNC_NAME, ...)      FUNC_NAME
#define SPECIAL_ACTOR_MAIL_HANDLE(...)     SPECIAL_ACTOR_GET_MAIL_HANDLE_FUNC(__VA_ARGS__, SPECIAL_ACTOR_MAIL_HANDLE_BASE, SPECIAL_ACTOR_MAIL_HANDLE_EMPTY, ...)(__VA_ARGS__)

}; // end of namespace nl::af

// vim: fenc=utf8:expandtab:ts=8
