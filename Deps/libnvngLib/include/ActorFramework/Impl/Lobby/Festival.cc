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
        std::shared_ptr<MsgActivityFestivalGroupCfg> _msgGroup;
};

// {{{ ActivityMgrBase
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
        festivalData->_groupCfgList.Foreach([this, &p, &cnt](const auto& msg) {
                if (!FestivalGroup::IsOpen(*msg))
                        return;

                ++cnt;
                auto group = _fesGroupList.Get(msg->group_guid());
                if (!group)
                {
                        group = FestivalGroup::Create(msg->type());
                        if (group->Init(p, *msg))
                        {
                                _fesGroupList.Add(group->_id, group);
                                group->OnDataReset(p, *_playerChange);
                        }
                }
        });
        if (cnt >= festivalData->_groupCfgList.Size())
                _version = festivalData->_version;

        if (_playerChange && _playerChange->ByteSizeLong() > 0)
                msg.add_player_change_list()->CopyFrom(*_playerChange);
        _playerChange.reset();
}

void ActivityMgrBase::OnOffline(const PlayerPtr& p)
{
        _fesGroupList.Foreach([&p](const auto& group) {
                group->OnOffline(p);
        });
}

void ActivityMgrBase::OnEvent(MsgPlayerChange& msg,
                          const PlayerPtr& p,
                          int64_t eventType,
                          int64_t cnt,
                          int64_t param,
                          ELogServiceOrigType logType,
                          uint64_t logParam)
{
        _fesGroupList.Foreach([&msg, &p, eventType, cnt, param, logType, logParam](const auto& group) {
                auto fesSyncData = ActivityMgrBase::_festivalSyncData;
                if (fesSyncData)
                {
                        auto cfg = fesSyncData->_groupCfgList.Get(group->_id);
                        if (cfg && 1 == cfg->state())
                                group->OnEvent(msg, p, eventType, cnt, param, logType, logParam);
                }
        });
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

        auto now = GetClock().GetTimeStamp();
        auto fesSyncData = ActivityMgrBase::_festivalSyncData;
        for (auto& val : msg->group_list())
        {
                auto msgGroup = val.second;
                LOG_INFO("收到 GMServer 推送下来的 group:{} active_time:{}",
                         msgGroup.group_guid(), msgGroup.time().active_time());
                const auto groupGuid = msgGroup.group_guid();
                for (auto& val : msgGroup.activity_list())
                        testParseFestivalCfgFunc(groupGuid, val.second);

                if (msgGroup.has_activity())
                        testParseFestivalCfgFunc(groupGuid, msgGroup.activity());

                ActivityMgrActorWeakPtr weakAct = shared_from_this();
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
                        /*
                        LOG_INFO("88888888888888888888888888888 id:{} active_time:{} now:{}",
                                 msgGroup.group_guid(), msgGroup.time().active_time(), GetClock().GetTimeStamp());
                                 */
                        time_t t = GetClock().GetSteadyTime() + msgGroup.time().active_time() - GetClock().GetTimeStamp();
                        _timer.Start(weakAct, t + 1.0, [weakAct, groupMsg, v{msg->version()}]() {
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
                                m->_msgGroup = groupMsg;
                                PlayerMgr::GetInstance()->BroadCast(act, E_MCMT_Activity, E_MCATST_Active, m);
                        });
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
        
        auto t = _activityMgr._fesGroupList.Get(msg->_msgGroup->group_guid());
        if (t)
                return nullptr;

        FestivalGroupPtr group = FestivalGroup::Create(msg->_msgGroup->type());
        LOG_INFO("88888888888888888888888888 active_time:{}", msg->_msgGroup->time().active_time());
        if (group->Init(shared_from_this(), *msg->_msgGroup))
        {
                _activityMgr._fesGroupList.Add(group->_id, group);

                if (_activityMgr._playerChange && _activityMgr._playerChange->ByteSizeLong() > 0)
                        Send2Client(E_MCMT_ClientCommon, E_MCCCST_PlayerChange, _activityMgr._playerChange);
                _activityMgr._playerChange.reset();

                group->OnDataReset(shared_from_this(), *_activityMgr._playerChange);
        }

        return nullptr;
}

ACTOR_MAIL_HANDLE(Player, E_MCMT_Activity, E_MCATST_Mark, MsgActivityMark)
{
        do
        {
                for (auto& item : msg->item_list())
                {
                        auto commonItem = item.common();
                        auto taskID = commonItem.task_id();
                        auto group = _activityMgr._fesGroupList.Get(FestivalTask::ParseGroupGuid(taskID));
                        if (!group)
                        {
                                LOG_WARN("玩家[{}] Activity Mark id[{}] 时，g[{}] f[{}] t[{}] 未找到 group!!!",
                                         GetID(),
                                         taskID,
                                         FestivalTask::ParseGroupGuid(taskID),
                                         FestivalTask::ParseFesID(taskID),
                                         FestivalTask::ParseTaskID(taskID));
                                break;
                        }

                        auto logGuid = LogService::GetInstance()->GenGuid();
                        group->Mark(shared_from_this(), *msg->mutable_player_change(), taskID, commonItem.cnt(), item.param(), E_LSOT_FestivalMark, logGuid);
                }
                msg->set_error_type(E_CET_Success);
                Save2DB();
        } while (0);

        Send2Client(E_MCMT_Activity, E_MCATST_Mark, msg);
        return nullptr;
}

ACTOR_MAIL_HANDLE(Player, E_MCMT_Activity, E_MCATST_Reward, MsgActivityReward)
{
        do
        {
                msg->set_error_type(E_CET_Success);
                const auto taskID = msg->id();
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

                Save2DB();
        } while (0);

        Send2Client(E_MCMT_Activity, E_MCATST_Reward, msg);
        return nullptr;
}
// }}}

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

// {{{ FestivalGroup

const std::pair<std::shared_ptr<MsgActivityFestivalGroupCfg>, const MsgActivityFestivalActivityCfg&> FestivalGroup::GetCfg(int64_t groupID, int64_t fesID)
{
        static MsgActivityFestivalActivityCfg emptyRet;
        auto fesSyncData = ActivityMgr::_festivalSyncData;
        if (!fesSyncData)
                return { nullptr, emptyRet };

        auto groupCfg = fesSyncData->_groupCfgList.Get(groupID);
        if (!groupCfg)
                return { nullptr, emptyRet };

        if (groupCfg->has_activity() && groupCfg->activity().cfg_id() % (10 * 1000) == fesID)
        {
                return { groupCfg, groupCfg->activity() };
        }
        else
        {

                auto it = groupCfg->activity_list().find(76000000 + fesID);
                if (groupCfg->activity_list().end() != it)
                        return { groupCfg, it->second };
        }
        return { groupCfg, emptyRet };
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

bool Festival::Init(const FestivalGroupPtr& group,
                    const PlayerPtr& p,
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

bool Festival::IsOpen(const MsgActivityFestivalActivityCfg& msg)
{
        auto now = GetClock().GetTimeStamp();
        switch (msg.time().type())
        {
        case 1 :
                return 1 == msg.state() && msg.time().active_time() <= now;
                break;
        case 2 :
                return 1 == msg.state() && msg.time().active_time() <= now && now < msg.time().end_time();
                break;
        default :
                break;
        }

        return true;
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
        auto dealFunc = [this, &msg, &p](const auto& act) {
                if (act.cfg_id() % 10000 == _id)
                {
                        if (1 == act.state())
                        {
                                _param += 1;
                                OnEvent(msg, p, E_AET_DataReset, 1, -1, E_LSOT_None, 0);
                        }
                        return true;
                }
                return false;
        };

        if (groupCfg.has_activity())
                dealFunc(groupCfg.activity());

        auto it = groupCfg.activity_list().find(76000000 + _id);
        if (groupCfg.activity_list().end() != it)
                dealFunc(it->second);
}

void Festival::OnEvent(MsgPlayerChange& msg,
                       const PlayerPtr& p,
                       int64_t eventType,
                       int64_t cnt,
                       int64_t param,
                       ELogServiceOrigType logType,
                       uint64_t logParam)
{
        auto& seq = _taskList.get<by_task_event_type>();
        auto range = seq.equal_range(eventType);
        for (auto it=range.first; range.second!=it; ++it)
        {
                (*it)->_cnt += cnt;

                std::string str = fmt::format("{}\"id\":{},\"type\":{},\"eventType\":{},\"cnt\":{},\"new_cnt\":{},\"param\":{}{}", "{", _id, _type, eventType, cnt, (*it)->_cnt, param, "}");
                LogService::GetInstance()->Log<E_LSLMT_Content>(p->GetID(), str, E_LSLST_Festival, 1, logType, logParam);
        }
}

void Festival::Mark(const PlayerPtr& p,
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
                (*it)->Mark(p, msg, cnt, param, logType, logParam);
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
void FestivalTask::Mark(const PlayerPtr& p,
                        MsgPlayerChange& msg,
                        int64_t cnt,
                        int64_t param,
                        ELogServiceOrigType logType,
                        uint64_t logParam)
{
        _cnt += cnt;
        p->Save2DB();

        std::string str = fmt::format("{}\"id\":{},\"cnt\":{},\"param\":{}{}", "{", _id, cnt, param, "}");
        LogService::GetInstance()->Log<E_LSLMT_Content>(p->GetID(), str, E_LSLST_Festival, 0, logType, logParam);
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

        auto logGuid = LogService::GetInstance()->GenGuid();
        BagMgr::GetInstance()->DoDrop(p, msg, cfg->_rewardList, E_LSOT_Festival, logGuid);

        std::string str = fmt::format("{}\"id\":{},\"param\":{}{}", "{", _id, param, "}");
        LogService::GetInstance()->Log<E_LSLMT_Content>(p->GetID(), str, E_LSLST_Festival, 2, logType, logParam);

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

        std::string str = fmt::format("{}\"id\":{},\"param\":{}{}", "{", _id, param, "}");
        LogService::GetInstance()->Log<E_LSLMT_Content>(p->GetID(), str, E_LSLST_Festival, 2, logType, logParam);

        for (auto& reward : rewardItem->goods_list())
        {
                auto& item = reward.goods_item();
                p->AddDrop(msg, item.type(), item.id(), item.num(), E_RT_MainCity, logType, logParam);
        }

        FLAG_ADD(_flag, 1);
        p->Save2DB();
        return true;
}
// }}}

// vim: fenc=utf8:expandtab:ts=8
