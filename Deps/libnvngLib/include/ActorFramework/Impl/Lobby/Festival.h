#pragma once

#include "Tools/Clock.h"
#include "msg_activity.pb.h"

#define FESTIVAL_REGISTER(t, g, f, fs) \
        AutoRelease ar_##t([]() { \
                FestivalGroup::RegisterOpt(t, []() { return std::make_shared<g>(); }, \
                                           []() { return std::make_shared<f>(); }, \
                                           []() { return std::make_shared<fs>(); }); \
        }, []() { })

#define FESTIVAL_TIME_CHECK(msg) \
                auto now = GetClock().GetTimeStamp(); \
                switch (msg.time().type()) { \
                case 1 : return 1 == msg.state() && msg.time().active_time() <= now; break; \
                case 2 : return 1 == msg.state() && msg.time().active_time() <= now && now < msg.time().end_time(); break; \
                default : break; \
                } \
                return true

class Player;
typedef std::shared_ptr<Player> PlayerPtr;

class Festival;
class FestivalGroup;
typedef std::shared_ptr<FestivalGroup> FestivalGroupPtr;

// {{{ stFestivalTime
struct stFestivalTime
{
        static constexpr time_t scDelInterval = 3600 * 24 * 31 * 6;
        time_t _activeTime = 0;
        time_t _startTime = 0;
        time_t _optStartTime = 0;
        time_t _optEndTime = 0;
        time_t _endTime = 0;
        time_t _delTime = 0;

        bool Init(const PlayerPtr& p, const MsgActivityTime& msg);

        void Pack(MsgFestivalTime& msg)
        {
                msg.set_active_time(_activeTime);
                msg.set_start_time(_startTime);
                msg.set_opt_start_time(_optStartTime);
                msg.set_opt_end_time(_optEndTime);
                msg.set_end_time(_endTime);
                msg.set_del_time(_delTime);
        }

        void UnPack(const MsgFestivalTime& msg)
        {
               _activeTime = msg.active_time();
               _startTime = msg.start_time();
               _optStartTime = msg.opt_start_time();
               _optEndTime = msg.opt_end_time();
               _endTime = msg.end_time();
               _delTime = msg.del_time();
        }
};
// }}}

// {{{ FestivalTask
class FestivalTask
{
public :
        virtual ~FestivalTask() { }

        int64_t _id = 0;
        int64_t _cnt = 0;
        int16_t _eventType = 0;
        int16_t _refreshType = 0;
        int64_t _flag = 0;

        FORCE_INLINE static uint64_t GenGuid(int64_t groupGuid, int64_t fesID, int64_t taskID)
        { return groupGuid << 40 | (fesID % 10000) << 24 | (taskID % (1000 * 1000)); }
        FORCE_INLINE static uint64_t ClearFesID(uint64_t id) { return id & 0xFFFFFF0000000000; }
        FORCE_INLINE static uint64_t ClearTaskID(uint64_t id) { return id & 0xFFFFFFFFFF000000; }
        FORCE_INLINE static int64_t ParseGroupGuid(uint64_t id) { return id >> 40; }
        FORCE_INLINE static int64_t ParseFesID(uint64_t id) { return (id & 0xFFFF000000) >> 24; }
        FORCE_INLINE static int64_t ParseTaskID(uint64_t id) { return id & 0xFFFFFF; }

        virtual void Reset()
        {
                _cnt = 0;
                _flag = 0;
        }

        void Pack(MsgFestivalTask& msg)
        {
                msg.set_id(_id);
                msg.set_cnt(_cnt);
                msg.set_event_type(_eventType);
                msg.set_refresh_type(_refreshType);
                msg.set_flag(_flag);
        }

        void UnPack(const MsgFestivalTask& msg)
        {
               _id = msg.id();
               _cnt = msg.cnt();
               _eventType = msg.event_type();
               _refreshType = msg.refresh_type();
               _flag = msg.flag();
        }

        virtual bool Mark(const PlayerPtr& p, MsgPlayerChange& msg, int64_t cnt, int64_t param, ELogServiceOrigType logType, uint64_t logParam);

        virtual bool Reward(const PlayerPtr& p,
                            const std::shared_ptr<FestivalGroup>& group,
                            const std::shared_ptr<Festival>& fes,
                            MsgPlayerChange& msg,
                            int64_t param,
                            ELogServiceOrigType logType,
                            uint64_t logParam);

        virtual void OnOffline(const PlayerPtr& p) { }
        virtual void OnDataReset(const PlayerPtr& p, MsgPlayerChange& msg) { }
};
typedef std::shared_ptr<FestivalTask> FestivalTaskPtr;
// }}}

// {{{ FestivalTaskImpl
class FestivalTaskImpl : public FestivalTask
{
public :
        bool Reward(const PlayerPtr& p,
                    const std::shared_ptr<FestivalGroup>& group,
                    const std::shared_ptr<Festival>& fes,
                    MsgPlayerChange& msg,
                    int64_t param,
                    ELogServiceOrigType logType,
                    uint64_t logParam) override;
};
// }}}

// {{{ Festival
struct by_id;
struct by_task_event_type;
struct by_task_refresh_type;

typedef boost::multi_index::multi_index_container
<
	FestivalTaskPtr,
	boost::multi_index::indexed_by <

        boost::multi_index::ordered_unique<
        boost::multi_index::tag<by_id>,
        BOOST_MULTI_INDEX_MEMBER(FestivalTask, int64_t, _id)
        >,

        boost::multi_index::hashed_non_unique<
        boost::multi_index::tag<by_task_event_type>,
        BOOST_MULTI_INDEX_MEMBER(FestivalTask, int16_t, _eventType)
        >,

        boost::multi_index::hashed_non_unique<
        boost::multi_index::tag<by_task_refresh_type>,
        BOOST_MULTI_INDEX_MEMBER(FestivalTask, int16_t, _refreshType)
        >

	>
> FestivalTaskListType;

class Festival : public std::enable_shared_from_this<Festival>
{
public :
        virtual ~Festival() { }

        bool InitCommon(const FestivalGroupPtr& group,
                        const PlayerPtr& p,
                        const MsgActivityFestivalActivityCfg& msg);

        virtual bool Init(MsgPlayerChange& playerChange,
                          const PlayerPtr& p,
                          const FestivalGroupPtr& group,
                          const MsgActivityFestivalActivityCfg& msg);

        virtual bool IsOpen(const MsgActivityFestivalActivityCfg& msg)
        { FESTIVAL_TIME_CHECK(msg); }
        virtual bool IsOpen(int64_t groupID);
        virtual void Pack(MsgFestival& msg);
        virtual void UnPack(const MsgFestival& msg);
        virtual void OnOffline(const PlayerPtr& p) { }

        virtual void OnDataReset(const PlayerPtr& p,
                                 MsgPlayerChange& msg,
                                 MsgActivityFestivalGroupCfg& groupCfg);

        virtual bool OnEvent(MsgPlayerChange& msg, const PlayerPtr& p, int64_t eventType, int64_t cnt, int64_t param, ELogServiceOrigType logType, uint64_t logParam);

        virtual bool Mark(const PlayerPtr& p, MsgPlayerChange& msg, int64_t taskID, int64_t cnt, int64_t param, ELogServiceOrigType logType, uint64_t logParam);

        virtual bool Reward(const PlayerPtr& p,
                            const std::shared_ptr<FestivalGroup>& group,
                            int64_t taskID,
                            MsgPlayerChange& msg,
                            int64_t param,
                            ELogServiceOrigType logType,
                            uint64_t logParam);

        int64_t _id = 0;
        int64_t _type = 1;
        int64_t _param = 0;
        stFestivalTime _time; 
        FestivalTaskListType _taskList;
};
typedef std::shared_ptr<Festival> FestivalPtr;
// }}}

// {{{ FestivalGroup
struct stFestivalOpt
{
        int64_t _type = 0;
        std::function<std::shared_ptr<FestivalGroup>()> _createFestivalGroupFunc;
        std::function<std::shared_ptr<Festival>()> _createFestivalFunc;
        std::function<std::shared_ptr<FestivalTask>()> _createFestivalTaskFunc;
};
typedef std::shared_ptr<stFestivalOpt> stFestivalOptPtr;

class FestivalGroup : public std::enable_shared_from_this<FestivalGroup>
{
public :
        FestivalGroup()
        {
        }

        virtual ~FestivalGroup()
        {
        }

        FORCE_INLINE static std::shared_ptr<FestivalGroup> Create(int64_t type)
        {
                auto opt = GetOptList().Get(type);
                if (opt && opt->_createFestivalGroupFunc)
                        return opt->_createFestivalGroupFunc();
                return std::make_shared<FestivalGroup>();
        }

        FORCE_INLINE static std::shared_ptr<Festival> CreateFestival(int64_t type)
        {
                auto opt = GetOptList().Get(type);
                if (opt && opt->_createFestivalFunc)
                        return opt->_createFestivalFunc();
                return std::make_shared<Festival>();
        }

        FORCE_INLINE static std::shared_ptr<FestivalTask> CreateFestivalTask(int64_t type)
        {
                auto opt = GetOptList().Get(type);
                if (opt && opt->_createFestivalTaskFunc)
                        return opt->_createFestivalTaskFunc();
                return std::make_shared<FestivalTaskImpl>();
        }

        FORCE_INLINE static bool RegisterOpt(int64_t t,
                                             const decltype(stFestivalOpt::_createFestivalGroupFunc)& createFestivalGroupFunc,
                                             const decltype(stFestivalOpt::_createFestivalFunc)& createFestivalFunc,
                                             const decltype(stFestivalOpt::_createFestivalTaskFunc)& createFestivalTaskFunc)
        {
                auto info = std::make_shared<stFestivalOpt>();
                info->_type = t;
                info->_createFestivalGroupFunc = createFestivalGroupFunc;
                info->_createFestivalFunc = createFestivalFunc;
                info->_createFestivalTaskFunc = createFestivalTaskFunc;
                return GetOptList().Add(t, info);
        }

        FORCE_INLINE static UnorderedMap<int64_t, stFestivalOptPtr>& GetOptList()
        { static UnorderedMap<int64_t, stFestivalOptPtr> _l; return _l; }

        static const std::shared_ptr<MsgActivityFestivalGroupCfg> GetGroupCfg(int64_t groupID);
        static const MsgActivityFestivalActivityCfg& GetFesCfg(const std::shared_ptr<MsgActivityFestivalGroupCfg>& groupCfg, int64_t groupID);
        static const std::pair<std::shared_ptr<MsgActivityFestivalGroupCfg>, const MsgActivityFestivalActivityCfg&> GetCfg(int64_t groupID, int64_t fesID);
        static const std::pair<std::shared_ptr<MsgActivityFestivalGroupCfg>, const MsgActivityFestivalActivityCfg&> GetCfg(int64_t taskID)
        { return GetCfg(FestivalTask::ParseGroupGuid(taskID), FestivalTask::ParseFesID(taskID)); }

        virtual bool Init(MsgPlayerChange& playerChange, const PlayerPtr& p, const MsgActivityFestivalGroupCfg& msg)
        {
                if (!_time.Init(p, msg.time()))
                        return false;

                _type = msg.type();
                _id = msg.group_guid();
                _param = 1;

                auto thisPtr = shared_from_this();
                auto createFesFunc = [this, &playerChange, &thisPtr, &p](const MsgActivityFestivalActivityCfg& msg) {
                        // 非开启状态，直接跳过。
                        if (1 != msg.state())
                                return true;
                        auto fes = CreateFestival(msg.type());
                        if (fes->IsOpen(msg) && fes->Init(playerChange, p, thisPtr, msg))
                                return _fesList.Add(fes->_id, fes);
                        return false;
                };

                if (msg.has_activity())
                {
                        if (!createFesFunc(msg.activity()))
                                return false;
                }

                for (auto& val : msg.activity_list())
                {
                        if (!createFesFunc(val.second))
                                return false;
                }

                return true;
        }

        static FORCE_INLINE bool IsOpen(const MsgActivityFestivalGroupCfg& msg)
        { FESTIVAL_TIME_CHECK(msg); }

        virtual bool IsOpen()
        {
                if (0 == _id) return true;
                auto groupCfg = GetGroupCfg(_id);
                return groupCfg ? IsOpen(*groupCfg) : false;
        }

        virtual void Pack(MsgFestivalGroup& msg)
        {
                msg.set_type(_type);
                _time.Pack(*msg.mutable_time());
                msg.set_id(_id);
                msg.set_param(_param);

                _fesList.Foreach([&msg](const auto& fes) {
                        fes->Pack(*msg.add_fes_list());
                });
        }

        virtual void UnPack(const MsgFestivalGroup& msg)
        {
                _type = msg.type();
                _time.UnPack(msg.time());
                _id = msg.id();
                _param = msg.param();

                _fesList.Clear();
                for (auto& fesMsg : msg.fes_list())
                {
                        auto fes = CreateFestival(fesMsg.type());
                        fes->UnPack(fesMsg);
                        _fesList.Add(fes->_id, fes);
                }
        }

        virtual bool Mark(const PlayerPtr& p
                          , MsgPlayerChange& msg
                          , int64_t taskID
                          , int64_t cnt
                          , int64_t param
                          , ELogServiceOrigType logType
                          , uint64_t logParam)
        {
                auto fes = _fesList.Get(FestivalTask::ParseFesID(taskID));
                if (fes && fes->IsOpen(FestivalTask::ParseGroupGuid(taskID)))
                        return fes->Mark(p, msg, taskID, cnt, param, logType, logParam);
                else
                        return false;
        }

        virtual bool Reward(const PlayerPtr& p
                            , int64_t taskID
                            , MsgPlayerChange& msg
                            , int64_t param
                            , ELogServiceOrigType logType
                            , uint64_t logParam)
        {
                auto fes = _fesList.Get(FestivalTask::ParseFesID(taskID));
                return fes ? fes->Reward(p, shared_from_this(), taskID, msg, param, logType, logParam) : false;
        }

        virtual void OnOffline(const PlayerPtr& p)
        {
                _fesList.Foreach([p](const auto& fes) {
                        fes->OnOffline(p);
                });
        }

        void OnDataReset(const PlayerPtr& p, MsgPlayerChange& msg);
        virtual bool OnEvent(MsgPlayerChange& msg
                             , const PlayerPtr& p
                             , int64_t eventType
                             , int64_t cnt
                             , int64_t param
                             , ELogServiceOrigType logType
                             , uint64_t logParam)
        {
                bool ret = false;
                _fesList.Foreach([this, &msg, &p, &ret, eventType, cnt, param, logType, logParam](const auto& fes) {
                        if (fes->IsOpen(_id) && fes->OnEvent(msg, p, eventType, cnt, param, logType, logParam))
                                ret = true;
                });
                return ret;
        }

        int64_t _id = 0;
        int64_t _type = 0;
        int64_t _param = 0;
        stFestivalTime _time;
        UnorderedMap<uint64_t, FestivalPtr> _fesList;
};
typedef std::shared_ptr<FestivalGroup> FestivalGroupPtr;
// }}}

struct stFestivalCfg
{
        int64_t _id = 0;
        int64_t _fesTimeType = 0; // 1=开服活动 2=限时活动
        int64_t _fesClass = 0; // 所属活动。
        int64_t _fesType = 0; // 活动类型。
        std::vector<int64_t> _fesParam1List; // 活动参数1。
        std::vector<int64_t> _fesParam2List; // 活动参数2。
        std::vector<int64_t> _fesPointsList; // 活动积分。
};
typedef std::shared_ptr<stFestivalCfg> stFestivalCfgPtr;

struct stActivityCfg
{
        int64_t _id = 0;
        int64_t _version = 0;
        std::vector<std::pair<int64_t, int64_t>> _rewardList;
        int64_t _refreshType = 0;
        int64_t _eventType = 0;
        int64_t _targetCnt = 0;
};
typedef std::shared_ptr<stActivityCfg> stActivityCfgPtr;

SPECIAL_ACTOR_DEFINE_BEGIN(ActivityMgrActor, 0xefe);

public :
        ActivityMgrActor() : SuperType(1 << 6) { }

SPECIAL_ACTOR_DEFINE_END(ActivityMgrActor);

struct stActivityFestivalSyncData
{
        int64_t _version = 0;
        std::string_view _data;
        std::shared_ptr<void> _bufRef;
        UnorderedMap<uint64_t, std::shared_ptr<MsgActivityFestivalActivityCfg::MsgActivityRewardItem>> _rewardCfgList;
        Map<uint64_t, std::shared_ptr<MsgActivityFestivalGroupCfg>> _groupCfgList;
};

struct stTaskInfo
{
        int64_t _id = 0;
        int64_t _refreshType = 0;
        int64_t _target = 0;
        int64_t _optType = 0;
        int64_t _misc = 0;
        int64_t _value = 0;
        int64_t _reward = 0;
};
typedef std::shared_ptr<stTaskInfo> stTaskInfoPtr;

class ActivityMgrBase
{
public :
        static bool InitOnce();

        static bool ReadCfg();
        static UnorderedMap<int64_t, stActivityCfgPtr> _cfgList;
        static UnorderedMap<int64_t, stActivityCfgPtr> _everyCfgList;
        static std::vector<std::pair<std::shared_ptr<stRandomSelectType<stActivityCfgPtr>>, int64_t>> _randomInfoList;

        static ActivityMgrActorPtr _activityMgrActor;
        static std::shared_ptr<stActivityFestivalSyncData> _festivalSyncData;

public :
        static bool ReadFestivalCfg();
        static UnorderedMap<int64_t, stFestivalCfgPtr> _festivalCfgList;

public :
        static UnorderedMap<int64_t, stTaskInfoPtr> _taskCfgList;

public :
        void Pack(MsgActivityMgr& msg, bool toDB);
        void UnPack(const MsgActivityMgr& msg);

        virtual void OnPlayerCreate(const PlayerPtr& p);
        void OnDataReset(const PlayerPtr& p, MsgPlayerChange& msg);
        void OnPlayerLogin(const PlayerPtr& p, MsgPlayerInfo& msg);
        void OnOffline(const PlayerPtr& p);

        bool OnEvent(MsgPlayerChange& msg
                     , const PlayerPtr& p
                     , int64_t eventType
                     , int64_t cnt
                     , int64_t param
                     , ELogServiceOrigType logType
                     , uint64_t logParam);

public :
        int64_t _version = 0;
        UnorderedMap<uint64_t, FestivalGroupPtr> _fesGroupList;
};

class FestivalTaskCommon : public FestivalTaskImpl
{
        typedef FestivalTaskImpl SuperType;
public :
        bool Reward(const PlayerPtr& p,
                    const std::shared_ptr<FestivalGroup>& group,
                    const std::shared_ptr<Festival>& fes,
                    MsgPlayerChange& msg,
                    int64_t param,
                    ELogServiceOrigType logType,
                    uint64_t logParam) override;
};

class FestivalCommon : public Festival
{
public :
        bool Init(MsgPlayerChange& playerChange
                  , const PlayerPtr& p
                  , const FestivalGroupPtr& group
                  , const MsgActivityFestivalActivityCfg& msg) override;
        void OnDataReset(const PlayerPtr& p, MsgPlayerChange& msg, MsgActivityFestivalGroupCfg& groupCfg) override;
        bool OnEvent(MsgPlayerChange& msg
                     , const PlayerPtr& p
                     , int64_t eventType
                     , int64_t cnt
                     , int64_t param
                     , ELogServiceOrigType logType
                     , uint64_t logParam) override;
};

class FestivalShopBase : public Festival
{
        typedef Festival SuperType;
public :
        bool Init(MsgPlayerChange& playerChange
                  , const PlayerPtr& p
                  , const FestivalGroupPtr& group
                  , const MsgActivityFestivalActivityCfg& msg) override;
};

// vim: fenc=utf8:expandtab:ts=8
