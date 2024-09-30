#pragma once

#if defined(ANNOUNCEMENT_SERVICE_SERVER) || defined(ANNOUNCEMENT_SERVICE_CLIENT)

#include "ActorFramework/ServiceExtra.hpp"

SPECIAL_ACTOR_DEFINE_BEGIN(AnnouncementActor, E_MCMT_Announcement);

public :
        AnnouncementActor() : SuperType(1 << 3) { }
        bool Init() override;
        void Sync();

        void LoadFromDB();
        void Flush2DB();

        void Terminate() override;

        SpinLock _cacheMutex;
        std::shared_ptr<MsgAnnouncementList> _cache;

SPECIAL_ACTOR_DEFINE_END(AnnouncementActor);

template <typename ServiceType, bool IsServer, typename _Tag = stDefaultTag>
class AnnouncementSession : public ServiceSession<ServiceType, IsServer, _Tag>
{
        typedef ServiceSession<ServiceType, IsServer, _Tag> SuperType;

public :
        typedef typename SuperType::MsgHeaderType MsgHeaderType;
public :
        explicit AnnouncementSession(typename SuperType::SocketType&& s)
                : SuperType(std::move(s))
        {
        }

        void OnConnect() override
        {
                SuperType::OnConnect();
#ifdef ANNOUNCEMENT_SERVICE_CLIENT
                SuperType::SendPB(nullptr, E_MCMT_Announcement, E_MCANNST_Sync, SuperType::MsgHeaderType::E_RMT_Send, 0, 0, 0);
#endif
        }

        EXTERN_MSG_HANDLE();
        DECLARE_SHARED_FROM_THIS(AnnouncementSession);
};

DECLARE_SERVICE_BASE_BEGIN(Announcement, SessionDistributeSID, AnnouncementSession);

private :
        AnnouncementServiceBase()
                : SuperType("AnnouncementService")
        {
        }

        ~AnnouncementServiceBase() override { }

public :
        bool Init() override;

#ifdef ANNOUNCEMENT_SERVICE_SERVER
        template <typename ... Args>
        bool StartLocal(int64_t actCnt, Args ... args)
        {
                auto act = std::make_shared<typename SuperType::ActorType>(std::forward<Args>(args)...);
                SuperType::_actorArr.emplace_back(act);
                act->Start();
                return true;
        }
#endif

        FORCE_INLINE AnnouncementActorPtr GetActor_() const { return SuperType::_actorArr[0].lock(); }
        std::shared_ptr<MsgAnnouncementList> _cache;

DECLARE_SERVICE_BASE_END(Announcement);

#ifdef ANNOUNCEMENT_SERVICE_SERVER
typedef AnnouncementServiceBase<E_ServiceType_Server, stGateServerInfo> AnnouncementService;
#endif

#ifdef ANNOUNCEMENT_SERVICE_CLIENT
typedef AnnouncementServiceBase<E_ServiceType_Client, stGMServerInfo> AnnouncementService;
#endif

#endif

// vim: fenc=utf8:expandtab:ts=8
