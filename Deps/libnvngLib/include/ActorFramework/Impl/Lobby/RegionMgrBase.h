#pragma once

#include "ActorFramework/ActorImpl.hpp"
#include "ActorFramework/ServiceImpl.hpp"
#include "LobbyGameMgrSession.h"
#include "LobbyGameSession.h"
#include "PlayerBase.h"

namespace nl::af::impl {

class RegionMgrBase;
extern RegionMgrBase* GetRegionMgrBase();

class MainCityActor;
typedef std::shared_ptr<MainCityActor> MainCityActorPtr;

class RegionMgrBase : public ServiceImpl
{
        typedef ServiceImpl SuperType;
public :
        RegionMgrBase();
        ~RegionMgrBase() override;

	bool Init() override;
        static FORCE_INLINE RegionMgrBase* GetService() { return GetRegionMgrBase(); }

        IActorPtr GetRegionRelation(const PlayerBasePtr& p);
        void DealRegionRelation(int64_t id, int64_t mt, int64_t st, const ActorMailDataPtr& msg);

        EInternalErrorType ReqExitRegion(const PlayerBasePtr& p, const IActorPtr& region);
	IActorPtr RequestEnterMainCity(const PlayerBasePtr& p);
	IActorPtr RequestEnterRegion(const PlayerBasePtr& p, const MsgReqEnterRegion& msg);

        LobbyGameMgrSession::ActorAgentTypePtr
        RequestQueue(const PlayerBasePtr& p,
                     const MsgReqQueue& reqQueueMsg,
                     EClientErrorType& errorType);
        virtual void PackReqQueueInfoExtra(const PlayerBasePtr& p, ERegionType regionType, MsgPlayerQueueInfo& msg) { }

        IActorPtr DirectEnterRegion(const PlayerBasePtr& p,
                                    const std::shared_ptr<LobbyGameMgrSession>& gameMgrSes,
                                    ERegionType regionType,
                                    uint64_t regionGuid,
                                    uint64_t gameSesSID);
        FORCE_INLINE std::shared_ptr<LobbyGameSession> GetGameSession(uint64_t sid)
        { return _gameSesList.Get(sid).lock(); }
        virtual void PackPlayerInfo2RegionExtra(const PlayerBasePtr& p, ERegionType regionType, MailSyncPlayerInfo2Region& msg);

	void Terminate() override;
	void WaitForTerminate() override;

public :
        ThreadSafeUnorderedMap<int64_t, std::shared_ptr<MailPlayerRegionRelationInfo>> _regionRelationList;
private :
	friend class LobbyGameMgrSession;
	friend class LobbyGameSession;
	std::weak_ptr<LobbyGameMgrSession> _gameMgrSes;
        ThreadSafeUnorderedMap<uint64_t, std::weak_ptr<LobbyGameSession>> _gameSesList;
        SpinLock _regionRelationListMutex;
        ThreadSafeUnorderedSet<ERegionType> _regionRelationSyncList;

public :
        FORCE_INLINE MainCityActorPtr GetMainCityActor(uint64_t id) const { return _mainCityActorArr[id & _mainCityActorArrSize]; }
public :
        MainCityActorPtr* _mainCityActorArr = nullptr;
        const int64_t _mainCityActorArrSize = (1 << 3) - 1;

public :
        ThreadSafeUnorderedMap<int64_t, std::shared_ptr<MsgMatchQueueInfo>> _competitionQueueInfoList;
};

class MainCityActor : public ActorImpl<MainCityActor, RegionMgrBase>
{
        typedef ActorImpl<MainCityActor, RegionMgrBase> SuperType;
public :
        MainCityActor();
        ~MainCityActor() override;

private :
        uint64_t GetType() const override { return E_RT_MainCity; }
        static FORCE_INLINE uint64_t GenGuid() { static uint64_t sGuid = UINT64_MAX; return ++sGuid; }
	UnorderedMap<uint64_t, std::weak_ptr<PlayerBase>> _loadPlayerList;
	UnorderedMap<uint64_t, std::weak_ptr<PlayerBase>> _playerList;

        friend class ActorImpl<MainCityActor, RegionMgrBase>;
        DECLARE_SHARED_FROM_THIS(MainCityActor);
	EXTERN_ACTOR_MAIL_HANDLE();
};

}; // end of namespace nl::af::impl

// vim: fenc=utf8:expandtab:ts=8
