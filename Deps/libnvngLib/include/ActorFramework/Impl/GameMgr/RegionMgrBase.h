#pragma once

#include "GameMgrGameSession.h"
#include "GameMgrLobbySession.h"

struct stSnowflakeRegionGuidTag;
typedef Snowflake<stSnowflakeRegionGuidTag, uint64_t, 1692871881000> SnowflakeRegionGuid;

class RequestActor;

struct stPlayerInfo
{
        uint32_t _lobbySID = 0;
        uint32_t _gateSID = 0;
        uint32_t _rank = 0;
        bool _isMain = false;
        uint64_t _param = 0;
        int64_t _camp = 0;
        GameMgrLobbySession::ActorAgentTypePtr _agent;
        FORCE_INLINE uint64_t GetID() const { return _agent ? _agent->GetID() : 0; }
        std::vector<uint32_t> _attrList;
        std::string _nickName;

        stPlayerInfo()
        {
                DLOG_INFO("stPlayerInfo::stPlayerInfo id[{}]", GetID());
        }

        ~stPlayerInfo()
        {
                DLOG_INFO("stPlayerInfo::~stPlayerInfo id[{}]", GetID());
        }

        void Pack(auto& msg)
        {
                for (auto idx : _attrList)
                        msg.add_attr_list(idx);
                msg.set_nick_name(_nickName);
                msg.set_is_main(_isMain);
                msg.set_camp(_camp);
                msg.set_rank(_rank);
        }

        void UnPackFromLobby(const auto& msg)
        {
                _attrList.clear();
                _attrList.reserve(msg.attr_list_size());
                for (auto idx : msg.attr_list())
                        _attrList.emplace_back(idx);
                _nickName = msg.nick_name();
                _rank = msg.rank();
        }

        void Pack2Region(auto& msg)
        {
                msg.set_gate_sid(_gateSID);
                msg.set_lobby_sid(_lobbySID);
                msg.set_player_guid(GetID());
                msg.set_rank(_rank);
                msg.set_camp(_camp);
                msg.set_param(_param);
        }
};
typedef std::shared_ptr<stPlayerInfo> stPlayerInfoPtr;

// 根据 regionType 来分。

struct stRegionMgrMailCustom : public stActorMailBase
{
        ActorMailDataPtr _msg;
};

struct stQueueInfo;
SPECIAL_ACTOR_DEFINE_BEGIN(RegionMgrActor, 0xeff)

        explicit RegionMgrActor(const std::shared_ptr<RegionCfg>& cfg);
        GameMgrGameSession::ActorAgentTypePtr ReqCreateRegionAgent(const std::shared_ptr<MailRegionCreateInfo>& cfg,
                                            const std::shared_ptr<RequestActor>& reqActor);
        FORCE_INLINE ERegionType GetRegionType() const { return _cfg->region_type(); }

        virtual ActorMailDataPtr DelRegion(const IActorPtr& from, const std::shared_ptr<MailRegionDestroyInfo>& msg);
        virtual ActorMailDataPtr ExitRegion(const IActorPtr& from, const std::shared_ptr<MailReqExit>& msg);
        virtual ActorMailDataPtr CreateRegion(const IActorPtr& from, const std::shared_ptr<stQueueInfo>& msg);

        virtual ActorMailDataPtr Custom(const IActorPtr& from, const std::shared_ptr<google::protobuf::MessageLite>& msg) { return nullptr; }

protected :
        std::shared_ptr<RegionCfg> _cfg;
	std::unordered_map<uint64_t, GameMgrGameSession::ActorAgentTypePtr> _regionList;

SPECIAL_ACTOR_DEFINE_END(RegionMgrActor);

class CompetitionKnockoutQueueMgrActor;
class CompetitionKnockoutRegionMgrActor : public RegionMgrActor
{
        typedef RegionMgrActor SuperType;
public :
        explicit CompetitionKnockoutRegionMgrActor(const std::shared_ptr<RegionCfg>& cfg, EQueueType qt)
                : SuperType(cfg), _queueType(qt), _guid(SnowflakeRegionGuid::Gen())
        {
                _regionGuidList.reserve(128);
        }

        ~CompetitionKnockoutRegionMgrActor()
        {
                DLOG_INFO("111111111111 CompetitionKnockoutRegionMgrActor::~CompetitionKnockoutRegionMgrActor");
        }

        ActorMailDataPtr DelRegion(const IActorPtr& from, const std::shared_ptr<MailRegionDestroyInfo>& msg) override;
        ActorMailDataPtr Custom(const IActorPtr& from, const std::shared_ptr<google::protobuf::MessageLite>& msg) override;

        void StartRound(std::vector<stPlayerInfoPtr>&& playerList, std::vector<std::shared_ptr<MailSyncPlayerInfo2Region>>&& robotList);
        void SyncMatchInfo(time_t nextRoundStartTime = 0);

public :
        int64_t _param = 0;
        int64_t _param_1 = 0;
        int64_t _roundCnt = 0;
        int64_t _lastRoundRegionCnt = -1;
        int64_t _rank = 0;
        std::vector<int64_t> _mapList;
        SteadyTimer _startRoundTimer;
        EQueueType _queueType = E_QT_None;
        std::shared_ptr<stQueueInfo> _qInfo;
        std::unordered_map<uint64_t, stPlayerInfoPtr> _playerList;
        std::unordered_map<uint64_t, std::shared_ptr<MailSyncPlayerInfo2Region>> _robotList;
        std::vector<stPlayerInfoPtr> _nextRoundPlayerList;
        std::vector<std::shared_ptr<MailSyncPlayerInfo2Region>> _nextRoundRobotList;
        MultiCastWapper<GameMgrLobbySession> _multicastInfo;

        const uint64_t _guid = 0;
        std::vector<uint64_t> _regionGuidList;

        DECLARE_SHARED_FROM_THIS(CompetitionKnockoutRegionMgrActor);
};
typedef std::shared_ptr<CompetitionKnockoutRegionMgrActor> CompetitionKnockoutRegionMgrActorPtr;

struct stQueueInfo : public IActor, public stActorMailBase
{
public :
        stQueueInfo() { }
        stQueueInfo(uint64_t id, ERegionType regionType, EQueueType queueType)
                : _id(id), _regionType(regionType), _queueType(queueType)
        {
                DLOG_INFO("stQueueInfo::stQueueInfo id[{}]", _id);
        }

        ~stQueueInfo() override
        {
                DLOG_INFO("stQueueInfo::~stQueueInfo id[{}]", _id);
        }

        uint64_t GetID() const override { return _id; }

        bool Equal(const MsgQueueBaseInfo& baseInfo)
        {
                return _regionType == baseInfo.region_type()
                        && _queueType == baseInfo.queue_type()
                        && static_cast<int64_t>(GetID()) == baseInfo.queue_guid()
                        && _time == baseInfo.time();
        }

        const uint64_t _id = 0;
        const ERegionType _regionType = E_RT_None;
        const EQueueType _queueType = E_QT_None;
        uint64_t _guid = 0;
        int64_t _rank = 0;
        time_t _time = 0;
        bool _matched = false;
        uint64_t _param = 0;
        uint64_t _param_1 = 0;
        uint64_t _param_2 = 0;
        IActorPtr _regionMgr;
        SteadyTimer _checkTimer;

        std::unordered_map<uint64_t, stPlayerInfoPtr> _playerList;
        std::unordered_map<uint64_t, std::shared_ptr<MailSyncPlayerInfo2Region>> _robotList;

        stQueueInfo& CopyBaseFrom(const stQueueInfo& rhs)
        {
                _guid = rhs._guid;
                _rank = rhs._rank;
                _time = rhs._rank;
                _matched = rhs._matched;
                _param = rhs._param;
                _param_1 = rhs._param_1;
                _param_2 = rhs._param_2;
                _regionMgr = rhs._regionMgr;

                return *this;
        }

        void PackBaseInfo(auto& msg)
        {
                msg.set_region_type(_regionType);
                msg.set_queue_type(_queueType);
                msg.set_queue_guid(_id);
                msg.set_guid(_guid);
                msg.set_time(_time);
        }

        void PackInfo(auto& msg)
        {
                PackBaseInfo(*msg.mutable_base_info());
                msg.set_param(_param);
                msg.set_param_1(_param_1);
                msg.set_param_2(_param_2);

                switch (_queueType)
                {
                case E_QT_Manual :
                        PackPlayerList(*msg.mutable_player_list());
                        break;
                case E_QT_CompetitionKnokout :
                        msg.set_param_2(_playerList.size() + _robotList.size());
                        break;
                default :
                        break;
                }
        }

        void Sync2Lobby()
        {
                auto mail = std::make_shared<MsgSyncQueueInfo>();
                PackInfo(*mail);
                for (auto& val : _playerList)
                        val.second->_agent->SendPush(val.second->_agent->GetBindActor(), E_MCMT_QueueCommon, E_MCQCST_SyncQueueInfo, mail);
        }


        void PackPlayerList(auto& msg)
        {
                for (auto& val : _playerList)
                        val.second->Pack(*msg.Add());
        }
};
typedef std::shared_ptr<stQueueInfo> stQueueInfoPtr;
typedef std::weak_ptr<stQueueInfo> stQueueInfoWeakPtr;

struct stMailExitQueue : public stActorMailBase
{
        EInternalErrorType _errorType = E_IET_Fail;
        stQueueInfoPtr _queue;
        uint64_t _playerGuid = 0;
};

struct stMailCreateRegionAgent : public stActorMailBase
{
        GameMgrGameSession::ActorAgentTypeWeakPtr _region;
};

struct stMailReqRegionRelationInfo : public stActorMailBase
{
        std::shared_ptr<GameMgrLobbySession> _ses;
};

struct stMailReqQueue : public stActorMailBase
{
        GameMgrLobbySession::ActorAgentTypePtr _agent;
        ActorMailDataPtr _msg;
        GameMgrLobbySession::MsgHeaderType _msgHead;
};

// 根据 regionType 来分。

struct by_guid;
struct by_rank_time;
struct by_time;

typedef boost::multi_index::multi_index_container
<
	stQueueInfoPtr,
	boost::multi_index::indexed_by <

	boost::multi_index::hashed_unique<
	boost::multi_index::tag<by_guid>,
	BOOST_MULTI_INDEX_CONST_MEM_FUN(stQueueInfo, uint64_t, GetID)
	>,

	boost::multi_index::ordered_non_unique<
	boost::multi_index::tag<by_time>,
	BOOST_MULTI_INDEX_MEMBER(stQueueInfo, time_t, _time)
	>,

	boost::multi_index::ordered_non_unique
	<
	boost::multi_index::tag<by_rank_time>,
	boost::multi_index::composite_key
	<
	stQueueInfo,
	BOOST_MULTI_INDEX_MEMBER(stQueueInfo, int64_t, _rank),
	BOOST_MULTI_INDEX_MEMBER(stQueueInfo, time_t, _time)
	>
	>

	>
> QueueListType;

struct stMailReqEnterRegion : public stActorMailBase
{
        GameMgrLobbySession::ActorAgentTypePtr _agent;
        ActorMailDataPtr _msg;
        uint8_t _guid = 0;
        bool _new = false;
};

SPECIAL_ACTOR_DEFINE_BEGIN(QueueMgrActor, E_MCMT_QueueCommon)
        explicit QueueMgrActor(const std::shared_ptr<RegionCfg>& cfg);
        virtual void InitCheckTimer() { }

        virtual void ReqQueue(const std::shared_ptr<stMailReqQueue>& msg) { }
        virtual EInternalErrorType ExitQueue(const std::shared_ptr<MsgExitQueue>& msg, bool self) { return E_IET_None; }
        virtual stQueueInfoPtr QueueOpt(const std::shared_ptr<MsgQueueOpt>& msg) { return nullptr; }

        FORCE_INLINE ERegionType GetRegionType() const { return _cfg->region_type(); }
        virtual EQueueType GetQueueType() const { return E_QT_None; }

protected :
        ::nl::util::SteadyTimer _checkTimer;
        friend class GameMgrLobbySession;
        std::shared_ptr<RegionCfg> _cfg;
        QueueListType _queueList;
SPECIAL_ACTOR_DEFINE_END(QueueMgrActor);

class NormalQueueMgrActor : public QueueMgrActor
{
        typedef QueueMgrActor SuperType;
public :
        NormalQueueMgrActor(const std::shared_ptr<RegionCfg>& cfg)
                : SuperType(cfg)
        {
        }

        bool Init() override;
        EQueueType GetQueueType() const override { return E_QT_Normal; }
        void ReqQueue(const std::shared_ptr<stMailReqQueue>& msg) override;
        EInternalErrorType ExitQueue(const std::shared_ptr<MsgExitQueue>& msg, bool self) override;
        void InitCheckTimer() override;

        std::vector<stQueueInfoPtr> CheckSuitableImmediately(const stQueueInfoPtr& qInfo);
        std::vector<stQueueInfoPtr> CheckSuitableTimeout(const stQueueInfoPtr& qInfo, time_t now, bool& isRobot);
        bool MatchQueue(const std::vector<stQueueInfoPtr>& queueList, const stQueueInfoPtr& rhs, bool timeout);

        DECLARE_SHARED_FROM_THIS(NormalQueueMgrActor);
};

class ManualQueueMgrActor : public QueueMgrActor
{
        typedef QueueMgrActor SuperType;
public :
        ManualQueueMgrActor(const std::shared_ptr<RegionCfg>& cfg)
                : SuperType(cfg)
        {
        }

        EQueueType GetQueueType() const override { return E_QT_Manual; }
        void ReqQueue(const std::shared_ptr<stMailReqQueue>& msg) override;
        EInternalErrorType ExitQueue(const std::shared_ptr<MsgExitQueue>& msg, bool self) override;
        stQueueInfoPtr QueueOpt(const std::shared_ptr<MsgQueueOpt>& msg) override;
};

class CompetitionKnockoutQueueMgrActor : public QueueMgrActor
{
        typedef QueueMgrActor SuperType;
public :
        CompetitionKnockoutQueueMgrActor(const std::shared_ptr<RegionCfg>& cfg)
                : SuperType(cfg)
        {
        }

        bool Init() override;

        EQueueType GetQueueType() const override { return E_QT_CompetitionKnokout; }
        void ReqQueue(const std::shared_ptr<stMailReqQueue>& msg) override;
        EInternalErrorType ExitQueue(const std::shared_ptr<MsgExitQueue>& msg, bool self) override;
        void StartCompetition(const stQueueInfoPtr& qInfo);

        void InitSyncQueueInfoTimer();
        ::nl::util::SteadyTimer _syncQueueInfoTimer;

        DECLARE_SHARED_FROM_THIS(CompetitionKnockoutQueueMgrActor);
};

class RegionMgrBase;
extern RegionMgrBase* GetRegionMgrBase();

class RegionMgrBase : public ServiceImpl
{
	typedef ServiceImpl SuperType;

protected :
	RegionMgrBase();
	~RegionMgrBase() override;

public :
	bool Init() override;

        FORCE_INLINE static IService* GetService() { return GetRegionMgrBase(); }

        GameMgrGameSession::ActorAgentTypePtr
                CreateRegionAgent(const std::shared_ptr<MailRegionCreateInfo>& cfg,
                                  const std::shared_ptr<GameMgrGameSession>& ses,
                                  const std::shared_ptr<RequestActor>& b);

public :
        std::shared_ptr<RequestActor> GetReqActor(uint64_t id) const;
        FORCE_INLINE std::shared_ptr<RequestActor> DistReqActor(const uint64_t regionGuid)
        { return _reqActorArr[regionGuid % _reqActorArr.size()]; }

        FORCE_INLINE std::shared_ptr<GameMgrGameSession> GetGameSes(int64_t sid)
        {
                auto it = std::find_if(_gameSesArr.begin(), _gameSesArr.end(), [sid](const auto& wlhs) {
                        auto lhs = wlhs.lock();
                        return lhs ? sid == lhs->GetSID() : false;
                });
                return _gameSesArr.end() != it ? (*it).lock() : nullptr;
        }
        FORCE_INLINE std::shared_ptr<GameMgrGameSession> DistGameSes(ERegionType t, int64_t regionGuid)
        {
                std::lock_guard l(_gameSesArrMutex);
                return (!ERegionType_GameValid(t) || _gameSesArr.empty()) ? nullptr : _gameSesArr[regionGuid % _gameSesArr.size()].lock();
        }
        void RegistGameSes(const std::shared_ptr<GameMgrGameSession>& ses)
        {
                std::lock_guard l(_gameSesArrMutex);
                _gameSesArr.push_back(ses);
                std::sort(_gameSesArr.begin(), _gameSesArr.end(), [](const auto& wlhs, const auto& wrhs) {
                        auto lhs = wlhs.lock();
                        auto rhs = wrhs.lock();
                        return lhs && rhs ? lhs->GetSID() < rhs->GetSID() : false;
                });
        }

        void UnRegistGameSes(const std::shared_ptr<GameMgrGameSession>& ses);
        FORCE_INLINE std::shared_ptr<GameMgrLobbySession> GetLobbySes(uint64_t sid)
        { return _lobbySesList.Get(sid).lock(); }

	virtual RegionMgrActorPtr CreateRegionMgrActor(const std::shared_ptr<RegionCfg>& cfg) const;
        virtual QueueMgrActorPtr CreateQueueMgrActor(const std::shared_ptr<RegionCfg>& cfg, EQueueType queueType) const;
        inline RegionMgrActorPtr GetRegionMgrActor(ERegionType regionType)
        { return ERegionType_GameValid(regionType) ? _regionMgrActorArr[regionType] : nullptr; }
        inline QueueMgrActorPtr GetQueueMgrActor(ERegionType regionType, EQueueType queueType)
        { return ERegionType_GameValid(regionType) && EQueueType_GameValid(queueType) ? _queueMgrActorArr[queueType][regionType] : nullptr; }

        std::shared_ptr<RegionCfg> GetRegionCfg(ERegionType rt) { return _regionCfgList.Get(rt); }
        ::nl::af::impl::stCompetitionCfgInfoPtr GetCompetitionCfg(int64_t id) { return _competitionCfgList.Get(id); }

        void Terminate() override;
        void WaitForTerminate() override;

private :
        friend class RegionMgrActor;
        friend class QueueMgrActor;
        friend class RequestActor;
        friend class GameMgrLobbySession;
        friend class GameMgrGameSession;

	ThreadSafeUnorderedSet<uint64_t> _reqList;
	RegionMgrActorPtr _regionMgrActorArr[ERegionType_ARRAYSIZE];
	QueueMgrActorPtr  _queueMgrActorArr[EQueueType_ARRAYSIZE][ERegionType_ARRAYSIZE];
        std::vector<std::shared_ptr<RequestActor>> _reqActorArr;

        SpinLock _gameSesArrMutex;
        std::vector<std::weak_ptr<GameMgrGameSession>> _gameSesArr;

        UnorderedMap<ERegionType, std::shared_ptr<RegionCfg>> _regionCfgList;
        UnorderedMap<int64_t, ::nl::af::impl::stCompetitionCfgInfoPtr> _competitionCfgList;

public :
        ThreadSafeUnorderedMap<uint64_t, std::weak_ptr<GameMgrLobbySession>> _lobbySesList;
};

// vim: fenc=utf8:expandtab:ts=8
