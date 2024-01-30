#pragma once

#include "GameLobbySession.h"
#include "GameGateSession.h"
#include "GameMgrSession.h"

template <typename _Sy>
struct stMailServerReconnect : public stActorMailBase
{
        std::weak_ptr<_Sy> _ses;
};
typedef stMailServerReconnect<GameMgrSession> stMailGameMgrServerReconnect;
typedef stMailServerReconnect<GameLobbySession> stMailGameLobbyServerReconnect;

struct stMailFighterReconnect : public stActorMailBase
{
        std::shared_ptr<GameLobbySession> _ses;
        std::shared_ptr<MailFighterReconnect> _msg;
};

SPECIAL_ACTOR_DEFINE_BEGIN(RegionMgrActor, E_MIMT_GameCommon)
public :
        RegionMgrActor(const std::shared_ptr<RegionCfg>& cfg)
                : _cfg(cfg)
        {
        }

private :
        std::shared_ptr<RegionCfg> _cfg;
SPECIAL_ACTOR_DEFINE_END(RegionMgrActor);

struct stBattleLootInfo
{
        int64_t _version = 0;
        int64_t _id = 0;
        int64_t _maxLv = 0;
        static constexpr int64_t _scMaxLV = 37;
        std::vector<std::pair<int64_t, int64_t>> _goodsListArr[_scMaxLV];
};
typedef std::shared_ptr<stBattleLootInfo> stBattleLootInfoPtr;

struct stRegionOptInfo
{
        std::function<bool(void)> _initCfgFunc;
        std::function<IActorPtr(const std::shared_ptr<MailRegionCreateInfo>&, const std::shared_ptr<RegionCfg>&, const GameMgrSession::ActorAgentTypePtr&)> _createRegionFunc;
};

class RegionMgr : public ServiceImpl, public Singleton<RegionMgr>
{
	RegionMgr();
	~RegionMgr() override;
	friend class Singleton<RegionMgr>;

	typedef ServiceImpl SuperType;

public :
	bool Init() override;
        FORCE_INLINE static IService* GetService() { return RegionMgr::GetInstance(); }
        IActorPtr CreateRegion(const std::shared_ptr<MailRegionCreateInfo>& cInfo,
                               const GameMgrSession::ActorAgentTypePtr& agent);

public :
        UnorderedMap<ERegionType, std::shared_ptr<RegionCfg>> _regionCfgList;

public :
        bool RegisterOpt(ERegionType regionType,
                         decltype(stRegionOptInfo::_initCfgFunc) initCfg,
                         decltype(stRegionOptInfo::_createRegionFunc) createRegion);

private :
        static FORCE_INLINE auto& GetRegionOptList()
        { static UnorderedMap<ERegionType, std::shared_ptr<stRegionOptInfo>> _l; return _l; }

public :
	ThreadSafeUnorderedSet<uint64_t> _reqList;

        void Terminate() override
        {
                SuperType::Terminate();
                ForeachRegionMgrActor([](const auto& act, ERegionType rt) {
                        act->Terminate();
                });
        }

        void WaitForTerminate() override
        {
                SuperType::WaitForTerminate();
                ForeachRegionMgrActor([](const auto& act, ERegionType rt) {
                        act->WaitForTerminate();
                });
        }

        FORCE_INLINE RegionMgrActorPtr DistRegionMgrActor(ERegionType regionType)
        { return ERegionType_GameValid(regionType) ? _regionMgrActorArr[regionType] : nullptr; }
        FORCE_INLINE void ForeachRegionMgrActor(const auto& cb)
        {
                for (int32_t i=0; i<ERegionType_ARRAYSIZE; ++i)
                {
                        if (ERegionType_GameValid(i))
                        {
                                auto act = _regionMgrActorArr[i];
                                if (act)
                                        cb(act, static_cast<ERegionType>(i));
                        }
                }
        }
private :
	friend class GameMgrSession;
        friend class GameLobbySession;
	RegionMgrActorPtr _regionMgrActorArr[ERegionType_ARRAYSIZE];

public :
        UnorderedMap<int64_t, ::nl::af::impl::stCompetitionCfgInfoPtr> _competitionCfgList;
};

// vim: fenc=utf8:expandtab:ts=8
