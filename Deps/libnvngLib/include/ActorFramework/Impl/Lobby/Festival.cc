#include "Festival.h"

#include "Player/Player.h"
#include "LobbyGMSession.h"
#include "LobbyGateSession.h"

UnorderedMap<int64_t, stActivityCfgPtr> ActivityMgrBase::_cfgList;
UnorderedMap<int64_t, stActivityCfgPtr> ActivityMgrBase::_everyCfgList;
std::vector<std::pair<std::shared_ptr<stRandomSelectType<stActivityCfgPtr>>, int64_t>> ActivityMgrBase::_randomInfoList;

ActivityMgrActorPtr ActivityMgrBase::_activityMgrActor;

UnorderedMap<int64_t, stFestivalCfgPtr> ActivityMgrBase:: _festivalCfgList;
std::shared_ptr<stActivityFestivalSyncData> ActivityMgrBase::_festivalSyncData;

struct stFestivalActive : public stActorMailBase
{
        std::vector<std::shared_ptr<MsgActivityFestivalGroupCfg>> _msgGroupList;
};

// {{{ ActivityMgrBase
UnorderedMap<int64_t, stTaskInfoPtr> ActivityMgrBase::_taskCfgList;
bool ActivityMgrBase::InitOnce()
{
        _activityMgrActor = std::make_shared<ActivityMgrActor>();
        _activityMgrActor->Start();

        return ReadCfg()
                && ReadFestivalCfg();
}

bool ActivityMgrBase::ReadCfg()
{
        return true;
        std::string fileName = ServerCfgMgr::GetInstance()->GetConfigAbsolute("Mission.txt");
        std::stringstream ss;
        if (!ReadFileToSS(ss, fileName))
                return false;

        std::map<int64_t, std::shared_ptr<stRandomSelectType<stActivityCfgPtr>>> randomList;
        std::vector<int64_t> tmpList;
        int64_t idx = 0;
        std::string tmpStr;
        _cfgList.Clear();
        while (ReadTo(ss, "#"))
        {
                if (++idx <= 3)
                        continue;

                stActivityCfgPtr cfg = std::make_shared<stActivityCfg>();
                ss >> cfg->_id
                        >> tmpStr
                        >> cfg->_version
                        >> tmpStr
                        >> tmpStr
                        >> tmpStr
                        >> cfg->_refreshType
                        >> tmpStr
                        >> tmpStr
                        >> tmpStr
                        >> tmpStr
                        >> cfg->_eventType
                        >> tmpStr
                        >> cfg->_targetCnt
                        >> tmpList
                        ;
                for (auto& v : tmpList)
                        cfg->_rewardList.emplace_back(std::make_pair(v, 1));

                LOG_FATAL_IF(!_cfgList.Add(cfg->_id, cfg),
                             "文件[{}] 唯一ID重复，id[{}]!!!",
                             fileName, cfg->_id);

#if 0
                switch (cfg->_refreshType)
                {
                case 1 :
                case 2 :
                        {
                                auto& rs = randomList[cfg->_refreshType];
                                if (!rs)
                                {
                                        rs = std::make_shared<stRandomSelectType<stActivityCfgPtr>>(RandInRange<int64_t>);
                                        _randomInfoList.emplace_back(std::make_pair(rs, 4));
                                }

                                auto t = std::make_shared<const stRandomInfo<stActivityCfgPtr>>(100, cfg);
                                rs->Add(100, t);
                        }
                        break;
                default :
                        _everyCfgList.Add(cfg->_id, cfg);
                        break;
                }
#endif

                LOG_FATAL_IF(!CheckConfigEnd(ss, fileName),
                             "文件[{}] 读取错位，id[{}] 没检测到结束符 <end> !!!",
                             fileName, cfg->_id);
        }
        return true;
}

bool ActivityMgrBase::ReadFestivalCfg()
{
        std::string fileName = ServerCfgMgr::GetInstance()->GetConfigAbsolute("Festival.txt");
        std::stringstream ss;
        if (!ReadFileToSS(ss, fileName))
                return false;

        int64_t idx = 0;
        std::string tmpStr;
        _festivalCfgList.Clear();
        while (ReadTo(ss, "#"))
        {
                if (++idx <= 3)
                        continue;

                auto cfg = std::make_shared<stFestivalCfg>();
                ss >> cfg->_id
                        >> tmpStr
                        >> tmpStr
                        >> cfg->_fesTimeType
                        >> cfg->_fesClass
                        >> cfg->_fesType
                        >> tmpStr
                        >> tmpStr
                        >> cfg->_fesParam1List
                        >> cfg->_fesParam2List
                        >> cfg->_fesPointsList
                        ;

                LOG_FATAL_IF(!_festivalCfgList.Add(cfg->_id, cfg),
                             "文件[{}] 唯一ID重复，id[{}]!!!",
                             fileName, cfg->_id);

                LOG_FATAL_IF(!CheckConfigEnd(ss, fileName),
                             "文件[{}] 读取错位，id[{}] 没检测到结束符 <end> !!!",
                             fileName, cfg->_id);
        }
        return true;
}

void ActivityMgrBase::Pack(MsgActivityMgr& msg, bool toDB)
{
        if (!toDB)
        {
                auto fesSyncData = ActivityMgrBase::_festivalSyncData;
                if (fesSyncData)
                        msg.set_festival_data(fesSyncData->_data.data(), fesSyncData->_data.length());
        }
        _fesGroupList.Foreach([&msg](const auto& group) {
                group->Pack(*msg.add_group_list());
        });
}

void ActivityMgrBase::UnPack(const MsgActivityMgr& msg)
{
        const auto now = GetClock().GetTimeStamp();

        _fesGroupList.Clear();
        for (auto& msgGroup : msg.group_list())
        {
                if (msgGroup.time().del_time() <= now)
                        continue;

                auto group = FestivalGroup::Create(msgGroup.type());
                group->UnPack(msgGroup);
                _fesGroupList.Add(group->_id, group);
        }
}

void ActivityMgrBase::OnPlayerCreate(const PlayerPtr& p)
{
        auto group = FestivalGroup::Create(0);
        auto fes = FestivalGroup::CreateFestival(0);
        fes->_type = 0;
        fes->_id = 72;
        group->_fesList.Add(fes->_id, fes);
        group->_time._delTime = INT64_MAX;
        MsgActivityFestivalActivityCfg msg;
        // msg.set_cfg_id(73);

        MsgPlayerChange playerChange;
        if (fes->Init(playerChange, p, group, msg))
                p->_activityMgr._fesGroupList.Add(group->_id, group);
}

void ActivityMgrBase::OnDataReset(const PlayerPtr& p, MsgPlayerChange& msg)
{
        _fesGroupList.Foreach([&p, &msg](const auto& group) {
                group->OnDataReset(p, msg);
        });
}

void ActivityMgrBase::OnPlayerLogin(const PlayerPtr& p, MsgPlayerInfo& msg)
{
        auto festivalData = _festivalSyncData;
        if (!festivalData/* || _version == festivalData->_version*/)
                return;

        uint64_t cnt = 0;
        MsgPlayerChange playerChange;
        festivalData->_groupCfgList.Foreach([this, &p, &playerChange, &cnt](const auto& msg) {
                if (!FestivalGroup::IsOpen(*msg))
                        return;

                ++cnt;
                auto group = _fesGroupList.Get(msg->group_guid());
                if (!group)
                {
                        group = FestivalGroup::Create(msg->type());
                        if (group->Init(playerChange, p, *msg))
                        {
                                _fesGroupList.Add(group->_id, group);
                                group->OnDataReset(p, playerChange);
                        }
                }
        });
        if (cnt >= festivalData->_groupCfgList.Size())
                _version = festivalData->_version;

        if (playerChange.ByteSizeLong() > 0)
                msg.add_player_change_list()->CopyFrom(playerChange);
}

void ActivityMgrBase::OnOffline(const PlayerPtr& p)
{
        _fesGroupList.Foreach([&p](const auto& group) {
                group->OnOffline(p);
        });
}

bool ActivityMgrBase::OnEvent(MsgPlayerChange& msg,
                          const PlayerPtr& p,
                          int64_t eventType,
                          int64_t cnt,
                          int64_t param,
                          ELogServiceOrigType logType,
                          uint64_t logParam)
{
        bool ret = false;
        _fesGroupList.Foreach([&msg, &p, &ret, eventType, cnt, param, logType, logParam](const auto& group) {
                auto fesSyncData = ActivityMgrBase::_festivalSyncData;
                if (fesSyncData)
                {
                        auto cfg = fesSyncData->_groupCfgList.Get(group->_id);
                        if (cfg && FestivalGroup::IsOpen(*cfg))
                        {
                                if (group->OnEvent(msg, p, eventType, cnt, param, logType, logParam))
                                        ret = true;
                        }
                }
        });

        return ret;
}
// }}}

// {{{ ActivityMgrActor
SPECIAL_ACTOR_MAIL_HANDLE(ActivityMgrActor, 0x0, MsgActivityFestivalCfg)
{
        bool hasError = false;
        auto fesList = std::make_shared<stActivityFestivalSyncData>();
        fesList->_version = msg->version();
        auto testParseFestivalCfgFunc = [&fesList](int64_t groupGuid, const auto& msg) {
                auto cfg = ActivityMgrBase::_festivalCfgList.Get(msg.cfg_id());
                if (!cfg)
                {
                        LOG_WARN("活动 groupGuid[{}] fes[{}] 配置未找到!!!", groupGuid, msg.cfg_id());
                        return;
                }

                const_cast<MsgActivityFestivalActivityCfg*>(&msg)->set_type(cfg->_fesType);
                for (auto& val : msg.reward_list())
                {
                        auto cfg = ActivityMgrBase::_cfgList.Get(val.task_id());
                        if (cfg)
                                const_cast<MsgActivityFestivalActivityCfg::MsgActivityRewardItem*>(&val)->set_event_type(cfg->_eventType);
                        auto info = std::make_shared<MsgActivityFestivalActivityCfg::MsgActivityRewardItem>();
                        info->CopyFrom(val);
                        fesList->_rewardCfgList.Add(FestivalTask::GenGuid(groupGuid, msg.cfg_id(), info->task_id()), info);
                }

                int64_t i = 0;
                for (auto& val : msg.cum_rewards())
                {
                        auto info = std::make_shared<MsgActivityFestivalActivityCfg::MsgActivityRewardItem>();
                        info->mutable_goods_list()->CopyFrom(val.goods_list());
                        fesList->_rewardCfgList.Add(FestivalTask::GenGuid(groupGuid, msg.cfg_id(), ++i), info);
                }
        };

        auto directOpenMail = std::make_shared<stFestivalActive>();
        auto thisPtr = shared_from_this();
        auto now = GetClock().GetTimeStamp();
        auto fesSyncData = ActivityMgrBase::_festivalSyncData;
        for (auto& val : msg->group_list())
        {
                auto msgGroup = val.second;
                LOG_INFO("收到 GMServer 推送下来的 group:{} timeType:{} active_time:{}",
                         msgGroup.group_guid(), msgGroup.time().type(), msgGroup.time().active_time());
                const auto groupGuid = msgGroup.group_guid();
                for (auto& val : msgGroup.activity_list())
                        testParseFestivalCfgFunc(groupGuid, val.second);

                if (msgGroup.has_activity())
                        testParseFestivalCfgFunc(groupGuid, msgGroup.activity());

                ActivityMgrActorWeakPtr weakAct = thisPtr;
                auto groupMsg = std::make_shared<MsgActivityFestivalGroupCfg>();
                groupMsg->CopyFrom(msgGroup);
                fesList->_groupCfgList.Add(groupMsg->group_guid(), groupMsg);

                bool needTimer = true;
                if (fesSyncData)
                {
                        auto tCfg = fesSyncData->_groupCfgList.Get(groupMsg->group_guid());
                        if (tCfg && tCfg->time().active_time() < now)
                                needTimer = false;
                }

                if (needTimer)
                {
                        time_t t = msgGroup.time().active_time() - GetClock().GetTimeStamp();
                        /*
                        LOG_INFO("88888888888888888888888888888 id:{} active_time:{} now:{} nows:{} t:{}"
                                 , msgGroup.group_guid(), msgGroup.time().active_time()
                                 , GetClock().GetTimeStamp(), GetClock().GetSteadyTime(), t);
                                 */
                        ::nl::util::SteadyTimer::StaticStart(weakAct, t + 1.0, [weakAct, groupMsg, v{msg->version()}]() {
                                /*
                                LOG_INFO("99999999999999999999999999999 id:{} active_time:{} now:{}",
                                         groupMsg->group_guid(), groupMsg->time().active_time(), GetClock().GetTimeStamp());
                                         */
                                auto fesSyncData = ActivityMgrBase::_festivalSyncData;
                                auto act = weakAct.lock();
                                if (!act || !fesSyncData || v!=fesSyncData->_version || !FestivalGroup::IsOpen(*groupMsg))
                                        return;

                                /*
                                LOG_INFO("10101010101010101010101010109 id:{} active_time:{} now:{}",
                                         groupMsg->group_guid(), groupMsg->time().active_time(), GetClock().GetTimeStamp());
                                         */
                                auto m = std::make_shared<stFestivalActive>();
                                m->_msgGroupList.emplace_back(groupMsg);
                                PlayerMgr::GetInstance()->BroadCast(act, E_MCMT_Activity, E_MCATST_Active, m);
                        });
                }
                else // 直接开启。
                {
                        directOpenMail->_msgGroupList.emplace_back(groupMsg);
                }
        }

        if (!hasError)
        {
                auto [bufRef, bufSize] = Compress::SerializeAndCompress<LobbyGateSession::CompressType>(*msg);
                fesList->_data = std::string_view{ bufRef.get(), bufSize };
                fesList->_bufRef = bufRef;
                ActivityMgrBase::_festivalSyncData = fesList;

                GetApp()->_gateSesList.Foreach([id{GetID()}, &msg](const auto& ws) {
                        auto ses = ws.lock();
                        if (ses)
                                ses->BroadCast(msg, E_MCMT_Activity, E_MCATST_Sync, id);
                });

                if (!directOpenMail->_msgGroupList.empty())
                        PlayerMgr::GetInstance()->BroadCast(thisPtr, E_MCMT_Activity, E_MCATST_Active, directOpenMail);
        }

        return nullptr;
}

NET_MSG_HANDLE(LobbyGMSession, E_MIMT_Internal, E_MIIST_SyncRelation, MsgActivityFestivalCfg)
{
        ActivityMgrBase::_activityMgrActor->SendPush(0x0, msg);
}
// }}}

// {{{ ACTOR_MAIL_HANDLE
ACTOR_MAIL_HANDLE(Player, E_MCMT_Activity, E_MCATST_Active, stFestivalActive)
{
        if (!FLAG_HAS(_internalFlag, E_PIF_Online))
                return nullptr;

        auto playerChange = std::make_shared<MsgPlayerChange>();
        for (auto& groupCfg : msg->_msgGroupList)
        {
                auto t = _activityMgr._fesGroupList.Get(groupCfg->group_guid());
                if (t)
                        continue;

                FestivalGroupPtr group = FestivalGroup::Create(groupCfg->type());
                LOG_INFO("88888888888888888888888888 id[{}] active_time[{}]", groupCfg->group_guid(), groupCfg->time().active_time());
                if (group && group->Init(*playerChange, shared_from_this(), *groupCfg))
                {
                        _activityMgr._fesGroupList.Add(group->_id, group);
                        group->OnDataReset(shared_from_this(), *playerChange);
                }
        }

        if (playerChange->ByteSizeLong() > 0)
                Send2Client(E_MCMT_ClientCommon, E_MCCCST_PlayerChange, playerChange);

        return nullptr;
}

ACTOR_MAIL_HANDLE(Player, E_MCMT_Activity, E_MCATST_Mark, MsgActivityMark)
{
        do
        {
                msg->set_error_type(E_CET_Success);
                for (auto& item : msg->item_list())
                {
                        auto commonItem = item.common();
                        auto taskID = commonItem.task_id();
                        /*
                        LOG_WARN("玩家[{}] Activity Mark id[{}] 时，g[{}] f[{}] t[{}]!!!",
                                 GetID(),
                                 taskID,
                                 FestivalTask::ParseGroupGuid(taskID),
                                 FestivalTask::ParseFesID(taskID),
                                 FestivalTask::ParseTaskID(taskID));
                                 */
                        auto group = _activityMgr._fesGroupList.Get(FestivalTask::ParseGroupGuid(taskID));
                        if (!group || !group->IsOpen())
                        {
                                LOG_WARN("玩家[{}] Activity Mark id[{}] 时，g[{}] f[{}] t[{}] 未找到 group!!!",
                                         GetID(),
                                         taskID,
                                         FestivalTask::ParseGroupGuid(taskID),
                                         FestivalTask::ParseFesID(taskID),
                                         FestivalTask::ParseTaskID(taskID));
                                msg->set_error_type(E_CET_CfgNotFound);
                                break;
                        }

                        auto logGuid = LogService::GetInstance()->GenGuid();
                        if (!group->Mark(shared_from_this(), *msg->mutable_player_change(), taskID, commonItem.cnt(), item.param(), E_LSOT_FestivalMark, logGuid))
                        {
                                LOG_INFO("玩家[{}] Activity Mark id[{}] 时，g[{}] f[{}] t[{}] 失败!!!",
                                         GetID(),
                                         taskID,
                                         FestivalTask::ParseGroupGuid(taskID),
                                         FestivalTask::ParseFesID(taskID),
                                         FestivalTask::ParseTaskID(taskID));

                                msg->set_error_type(E_CET_ParamError);
                                break;
                        }

                        std::string str = fmt::format("{}\"id\":{},\"cnt\":{},\"param\":{}{}", "{", taskID, commonItem.cnt(), item.param(), "}");
                        LogService::GetInstance()->Log<E_LSLMT_Content>(GetID(), str, E_LSLST_Festival, 99999998, E_LSOT_FestivalMark, logGuid);
                }

                Save2DB();
        } while (0);

        return msg;
}

ACTOR_MAIL_HANDLE(Player, E_MCMT_Activity, E_MCATST_OnEvent, MsgActivityOnEvent)
{
        auto thisPtr = shared_from_this();
        auto playerChange = msg->mutable_player_change();
        for (auto& item : msg->item_list())
        {
                auto logGuid = LogService::GetInstance()->GenGuid();
                _activityMgr.OnEvent(*playerChange
                                     , thisPtr
                                     , item.event_type()
                                     , item.cnt()
                                     , item.param()
                                     , E_LSOT_FestivalOnEvent
                                     , logGuid);
        }

        return msg;
}

ACTOR_MAIL_HANDLE(Player, E_MCMT_Activity, E_MCATST_Reward, MsgActivityReward)
{
        do
        {
                msg->set_error_type(E_CET_Success);
                const auto taskID = msg->id();
                /*
                LOG_INFO("玩家[{}] Activity Reward id[{}] 时，g[{}] f[{}] t[{}] !!!",
                         GetID(),
                         taskID,
                         FestivalTask::ParseGroupGuid(taskID),
                         FestivalTask::ParseFesID(taskID),
                         FestivalTask::ParseTaskID(taskID));
                         */
                auto group = _activityMgr._fesGroupList.Get(FestivalTask::ParseGroupGuid(taskID));
                if (!group)
                {
                        LOG_INFO("玩家[{}] Activity Reward id[{}] 时，g[{}] f[{}] t[{}] 未找到 group!!!",
                                 GetID(),
                                 taskID,
                                 FestivalTask::ParseGroupGuid(taskID),
                                 FestivalTask::ParseFesID(taskID),
                                 FestivalTask::ParseTaskID(taskID));

                        msg->set_error_type(E_CET_CfgNotFound);
                        break;
                }

                auto logGuid = LogService::GetInstance()->GenGuid();
                if (!group->Reward(shared_from_this(), taskID, *msg->mutable_player_change(), msg->param(), E_LSOT_FestivalReward, logGuid))
                {
                        LOG_INFO("玩家[{}] Activity Reward id[{}] 时，g[{}] f[{}] t[{}] 失败!!!",
                                 GetID(),
                                 taskID,
                                 FestivalTask::ParseGroupGuid(taskID),
                                 FestivalTask::ParseFesID(taskID),
                                 FestivalTask::ParseTaskID(taskID));

                        msg->set_error_type(E_CET_ParamError);
                        break;
                }

                std::string str = fmt::format("{}\"id\":{},\"param\":{}{}", "{", taskID, msg->param(), "}");
                LogService::GetInstance()->Log<E_LSLMT_Content>(GetID(), str, E_LSLST_Festival, 99999999, E_LSOT_FestivalReward, logGuid);
                Save2DB();
        } while (0);

        return msg;
}
// }}}

// {{{ stFestivalTime
bool stFestivalTime::Init(const PlayerPtr& p, const MsgActivityTime& msg)
{
        _activeTime = msg.active_time();

        auto lastLoginTime = p->GetAttr<E_PAT_LastLoginTime>();
        switch (msg.type())
        {
        case 1 :
                _startTime = lastLoginTime + msg.start_time();
                _optStartTime = lastLoginTime + msg.start_time();
                _optEndTime = lastLoginTime + msg.opt_end_time();
                _endTime = lastLoginTime + msg.end_time();
                _delTime = INT64_MAX;
                break;
        case 2 :
                _startTime = msg.start_time();
                _optStartTime = msg.start_time();
                _optEndTime = msg.opt_end_time();
                _endTime = msg.end_time();
                _delTime = _endTime + scDelInterval;
                break;
        default :
                break;
        }
        return true;
}
// }}}

// {{{ FestivalGroup

const std::shared_ptr<MsgActivityFestivalGroupCfg> FestivalGroup::GetGroupCfg(int64_t groupID)
{
        auto fesSyncData = ActivityMgr::_festivalSyncData;
        return fesSyncData ? fesSyncData->_groupCfgList.Get(groupID) : nullptr;
}

const MsgActivityFestivalActivityCfg& FestivalGroup::GetFesCfg(const std::shared_ptr<MsgActivityFestivalGroupCfg>& groupCfg, int64_t fesID)
{
        static const MsgActivityFestivalActivityCfg emptyRet;
        if (!groupCfg)
                return emptyRet;

        if (groupCfg->has_activity() && groupCfg->activity().cfg_id() % (10 * 1000) == fesID)
        {
                return groupCfg->activity();
        }
        else
        {

                auto it = groupCfg->activity_list().find(76000000 + fesID);
                if (groupCfg->activity_list().end() != it)
                        return it->second;
        }
        return emptyRet;
}

const std::pair<std::shared_ptr<MsgActivityFestivalGroupCfg>, const MsgActivityFestivalActivityCfg&> FestivalGroup::GetCfg(int64_t groupID, int64_t fesID)
{
        static MsgActivityFestivalActivityCfg emptyRet;
        auto groupCfg = GetGroupCfg(groupID);
        if (!groupCfg)
                return { nullptr, emptyRet };
        return { groupCfg, GetFesCfg(groupCfg, fesID) };
}

void FestivalGroup::OnDataReset(const PlayerPtr& p, MsgPlayerChange& msg)
{
        auto fesSyncData = ActivityMgrBase::_festivalSyncData;
        if (!fesSyncData)
                return;

        auto groupCfg = std::make_shared<MsgActivityFestivalGroupCfg>();
        if (0 != _id)
        {
                groupCfg = fesSyncData->_groupCfgList.Get(_id);
                if (!groupCfg || 1 != groupCfg->state())
                        return;
        }

        _param += 1;
        _fesList.Foreach([&p, &msg, &groupCfg](const auto& fes) {
                fes->OnDataReset(p, msg, *groupCfg);
        });
}
// }}}

// {{{ Festival
bool Festival::InitCommon(const FestivalGroupPtr& group,
                          const PlayerPtr& p,
                          const MsgActivityFestivalActivityCfg& msg)
{
        if (!_time.Init(p, msg.time()))
                return false;

        _id = msg.cfg_id() % (10 * 1000);
        _type = msg.type();
        return true;
}

bool Festival::Init(MsgPlayerChange& playerChange,
                    const PlayerPtr& p,
                    const FestivalGroupPtr& group,
                    const MsgActivityFestivalActivityCfg& msg)
{
        if (!InitCommon(group, p, msg))
                return false;

        _taskList.clear();
        for (auto& val : msg.reward_list())
        {
                auto task = FestivalGroup::CreateFestivalTask(_type);
                task->_id = FestivalTask::GenGuid(group->_id, _id, val.task_id());
                task->_eventType = val.event_type();
                _taskList.emplace(task);
        }

        for (int64_t i=0; i<msg.cum_rewards_size(); ++i)
        {
                auto task = FestivalGroup::CreateFestivalTask(_type);
                task->_id = FestivalTask::GenGuid(group->_id, _id, i+1);
                _taskList.emplace(task);
        }

        return true;
}

bool Festival::IsOpen(int64_t groupID)
{
        if (0 == groupID) return true;
        auto [groupCfg, fesCfg] = FestivalGroup::GetCfg(groupID, _id);
        return groupCfg ? IsOpen(fesCfg) : false;
}

void Festival::Pack(MsgFestival& msg)
{
        msg.set_id(_id);
        msg.set_type(_type);
        msg.set_param(_param);
        _time.Pack(*msg.mutable_time());
        for (auto& task : _taskList.get<by_id>())
                task->Pack(*msg.add_fes_task_list());
}

void Festival::UnPack(const MsgFestival& msg)
{
        _id = msg.id();
        _type = msg.type();
        _time.UnPack(msg.time());
        _param = msg.param();

        _taskList.clear();
        for (auto& taskMsg : msg.fes_task_list())
        {
                auto task = FestivalGroup::CreateFestivalTask(_type);
                task->UnPack(taskMsg);
                _taskList.emplace(task);
        }
}

void Festival::OnDataReset(const PlayerPtr& p,
                           MsgPlayerChange& msg,
                           MsgActivityFestivalGroupCfg& groupCfg)
{
        _param += 1;
        OnEvent(msg, p, E_AET_DataReset, 1, -1, E_LSOT_None, 0);
}

bool Festival::OnEvent(MsgPlayerChange& msg,
                       const PlayerPtr& p,
                       int64_t eventType,
                       int64_t cnt,
                       int64_t param,
                       ELogServiceOrigType logType,
                       uint64_t logParam)
{
        /*
        LOG_INFO("111111111111111111111 id[{}] eventType[{}] cnt[{}] param[{}]"
                 , _id, eventType, cnt, param);
                 */
        auto& seq = _taskList.get<by_task_event_type>();
        auto range = seq.equal_range(eventType);
        for (auto it=range.first; range.second!=it; ++it)
                (*it)->_cnt += cnt;
        return range.first != range.second;
}

bool Festival::Mark(const PlayerPtr& p,
                    MsgPlayerChange& msg,
                    int64_t taskID,
                    int64_t cnt,
                    int64_t param,
                    ELogServiceOrigType logType,
                    uint64_t logParam)
{
        auto& seq = _taskList.get<by_id>();
        auto it = seq.find(taskID);
        if (seq.end() != it)
        {
                (*it)->Mark(p, msg, cnt, param, logType, logParam);
                return true;
        }
        return false;
}

bool Festival::Reward(const PlayerPtr& p,
                      const std::shared_ptr<FestivalGroup>& group,
                      int64_t taskID, MsgPlayerChange& msg,
                      int64_t param,
                      ELogServiceOrigType logType,
                      uint64_t logParam)
{
        auto& seq = _taskList.get<by_id>();
        auto it = seq.find(taskID);
        if (seq.end() != it)
                return (*it)->Reward(p, group, shared_from_this(), msg, param, logType, logParam);
        return false;
}
// }}}

// {{{ FestivalTask
bool FestivalTask::Mark(const PlayerPtr& p,
                        MsgPlayerChange& msg,
                        int64_t cnt,
                        int64_t param,
                        ELogServiceOrigType logType,
                        uint64_t logParam)
{
        _cnt += cnt;
        p->Save2DB();
        return true;
}

bool FestivalTask::Reward(const PlayerPtr& p,
                          const std::shared_ptr<FestivalGroup>& group,
                          const std::shared_ptr<Festival>& fes,
                          MsgPlayerChange& msg,
                          int64_t param,
                          ELogServiceOrigType logType,
                          uint64_t logParam)
{
        auto cfg = ActivityMgrBase::_cfgList.Get(_id);
        if (!cfg || FLAG_HAS(_flag, 1))
                return false;

        BagMgr::GetInstance()->DoDrop(p, msg, cfg->_rewardList, logType, logParam);

        FLAG_ADD(_flag, 1);
        p->Save2DB();
        return true;
}
// }}}

// {{{ FestivalTaskImpl

bool FestivalTaskImpl::Reward(const PlayerPtr& p,
                              const std::shared_ptr<FestivalGroup>& group,
                              const std::shared_ptr<Festival>& fes,
                              MsgPlayerChange& msg,
                              int64_t param,
                              ELogServiceOrigType logType,
                              uint64_t logParam)
{
        if (FLAG_HAS(_flag, 1))
                return false;

        auto fesSyncData = ActivityMgrBase::_festivalSyncData;
        if (!fesSyncData)
                return false;

        auto rewardItem = fesSyncData->_rewardCfgList.Get(_id);
        if (!rewardItem)
        {
                LOG_WARN("玩家[{}] 未找到 rewardItem id[{}]!!! g[{}] f[{}] t[{}]",
                         p->GetID(),
                         _id,
                         FestivalTask::ParseGroupGuid(_id),
                         FestivalTask::ParseFesID(_id),
                         FestivalTask::ParseTaskID(_id));
                return false;
        }

        for (auto& reward : rewardItem->goods_list())
        {
                auto& item = reward.goods_item();
                p->AddDrop(msg, item.type(), item.id(), item.num(), logType, logParam);
        }

        FLAG_ADD(_flag, 1);
        p->Save2DB();
        return true;
}
// }}}

// {{{ FestivalCommon
FESTIVAL_REGISTER(0, FestivalGroup, FestivalCommon, FestivalTaskCommon);
bool FestivalTaskCommon::Reward(const PlayerPtr& p,
                                const std::shared_ptr<FestivalGroup>& group,
                                const std::shared_ptr<Festival>& fes,
                                MsgPlayerChange& msg,
                                int64_t param,
                                ELogServiceOrigType logType,
                                uint64_t logParam)
{
        auto cfg = ActivityMgr::_taskCfgList.Get(_id);
        if (!cfg)
                return false;

        if (_cnt < cfg->_value)
                return false;

        auto errorType = BagMgr::GetInstance()->DoDrop(p, msg, {{ cfg->_reward, 1}}, logType, logParam);
        if (E_CET_Success != errorType)
                return false;

        FLAG_ADD(_flag, 1);
        return true;
}

bool FestivalCommon::Init(MsgPlayerChange& playerChange
                          , const PlayerPtr& p
                          , const FestivalGroupPtr& group
                          , const MsgActivityFestivalActivityCfg& msg)
{
        // LOG_INFO("77777777 id:{}", _id);
        ActivityMgr::_taskCfgList.Foreach([this](const auto& cfg) {
                if (0 != cfg->_reward)
                {
                        auto task = FestivalGroup::CreateFestivalTask(0);
                        task->_id = cfg->_id;
                        task->_eventType = cfg->_target;
                        task->_refreshType = cfg->_refreshType;
                        _taskList.emplace(task);
                }
        });

        return true;
}

void FestivalCommon::OnDataReset(const PlayerPtr& p,
                                 MsgPlayerChange& msg,
                                 MsgActivityFestivalGroupCfg& groupCfg)
{
        std::vector<int64_t> refreshTypeArr = { 1 };
        if (0 == GetClock().GetWeekDay())
                refreshTypeArr.emplace_back(2);

        auto& seq = _taskList.get<by_task_refresh_type>();
        for (auto t : refreshTypeArr)
        {
                auto range = seq.equal_range(t);
                for (auto it=range.first; range.second!=it; ++it)
                        (*it)->Reset();
        }
}

bool FestivalCommon::OnEvent(MsgPlayerChange& msg
                             , const PlayerPtr& p
                             , int64_t eventType
                             , int64_t cnt
                             , int64_t param
                             , ELogServiceOrigType logType
                             , uint64_t logParam)
{
        auto& seq = _taskList.get<by_task_event_type>();
        auto range = seq.equal_range(eventType);
        for (auto it=range.first; range.second!=it; ++it)
        {
                auto task = *it;
                if (!task)
                        continue;

                auto taskCfg = ActivityMgr::_taskCfgList.Get(task->_id);
                if (!taskCfg)
                        continue;

                switch (taskCfg->_optType)
                {
                case 1 : // 等于
                        if (taskCfg->_misc != param)
                                continue;
                        break;
                case 2 : // 小于等于
                        if (param > taskCfg->_misc)
                                continue;
                        break;
                case 3 : // 大于等于
                        if (param < taskCfg->_misc)
                                continue;
                        break;
                default :
                        continue;
                        break;
                }

                task->_cnt += cnt;
        }

        return range.first != range.second;
}
// }}}

// {{{ FestivalShopBase
bool FestivalShopBase::Init(MsgPlayerChange& playerChange
                            , const PlayerPtr& p
                            , const FestivalGroupPtr& group
                            , const MsgActivityFestivalActivityCfg& msg)
{
        if (!SuperType::Init(playerChange, p, group, msg))
                return false;

        auto [groupCfg, fesCfg] = FestivalGroup::GetCfg(msg.param(), msg.cfg_id() % (1000 * 1000));
        if (groupCfg && 0 != fesCfg.cfg_id())
        {
                group->_time.Init(p, fesCfg.time());
                _time.Init(p, fesCfg.time());
        }

        return true;
}
// }}}

// vim: fenc=utf8:expandtab:ts=8
