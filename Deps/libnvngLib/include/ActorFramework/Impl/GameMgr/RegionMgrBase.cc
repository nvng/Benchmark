#include "RequestActor.h"

#include "GameMgrLobbySession.h"
#include "GameMgrGameSession.h"

// {{{ RegionMgrActor

// except 用于避免 queueGuid 和 regionGuid 碰撞，导致 agent 添加失败，
// 只能发送无法接收消息，LobbyServer 的 Player 可能永远无法回到大厅。
//
// Note: 禁止使用 void* 代替 guid，当机需要恢复的时候，会出问题。
template <typename _Ty>
static int64_t GenGuid(_Ty& l, ERegionType rt)
{
        constexpr static int64_t _guidCnt = UINT32_MAX / ERegionType_ARRAYSIZE;
        while (true)
        {
                int64_t ret = rt * _guidCnt + RandInRange(10, _guidCnt);
                auto it = l.find(ret);
                if (l.end() != it)
                {
                        DLOG_WARN("生成ID出现重复!!! rt[{}] lname[{}] size[{}]",
                                  rt, typeid(l).name(), l.size());
                        continue;
                }
                return ret;
        }
        return 0;
}

template <typename _Ty>
static int64_t GenGuid(_Ty& l, ERegionType rt, const std::unordered_set<int64_t>& exceptList)
{
        constexpr static int64_t _guidCnt = UINT32_MAX / ERegionType_ARRAYSIZE;
        while (true)
        {
                int64_t ret = rt * _guidCnt + RandInRange(10, _guidCnt);
                {
                        auto it = exceptList.find(ret);
                        if (exceptList.end() != it)
                        {
                                DLOG_WARN("生成ID出现重复!!! rt[{}] size[{}]",
                                          rt, exceptList.size());
                                continue;
                        }
                }

                auto it = l.find(ret);
                if (l.end() != it)
                {
                        DLOG_WARN("生成ID出现重复!!! rt[{}] lname[{}] size[{}]",
                                  rt, typeid(l).name(), l.size());
                        continue;
                }
                return ret;
        }
        return 0;
}

RegionMgrActor::RegionMgrActor(const std::shared_ptr<RegionCfg>& cfg)
        : _cfg(cfg)
{
}

        GameMgrGameSession::ActorAgentTypePtr
RegionMgrActor::ReqCreateRegionAgent(const std::shared_ptr<MailRegionCreateInfo>& cfg,
                                     const RequestActorPtr& reqActor)
{
        // TODO: gameSes 分配失败!!!
        auto gameSes = GetRegionMgrBase()->DistGameSes(cfg->region_type(), cfg->region_id());
        if (!gameSes)
        {
                LOG_WARN("玩家 请求进入场景时，分配 gameSes 失败!!!");
                return nullptr;
        }

        auto gameAgent = GetRegionMgrBase()->CreateRegionAgent(cfg, gameSes, reqActor);
        if (!gameSes->AddAgent(gameAgent)
            || !_regionList.emplace(gameAgent->GetID(), gameAgent).second)
        {
                assert(false);
                return nullptr;
        }

        gameAgent->_regionMgr = shared_from_this();
        return gameAgent;
}

std::shared_ptr<RequestActor> RegionMgrBase::GetReqActor(uint64_t id) const
{
        auto it = std::find_if(_reqActorArr.begin(), _reqActorArr.end(), [id](const auto& lhs) { return id == lhs->GetID(); });
        return _reqActorArr.end() != it ? *it : nullptr;
}

SPECIAL_ACTOR_MAIL_HANDLE(RegionMgrActor, 0x0, stMailReqEnterRegion)
{
        auto pb = std::dynamic_pointer_cast<MailReqEnterRegion>(msg->_msg);
        EInternalErrorType errorType = E_IET_Fail;
        const auto& fInfo = pb->fighter_list(0);
        auto reqAgent = msg->_agent;
        do
        {
                // TODO: CanEnter 会操作 regionAgent 内部，可能存在多线程问题。
                GameMgrGameSession::ActorAgentTypePtr region;
                switch (pb->region_type())
                {
                case E_RT_None :
                        for (auto& val : _regionList)
                        {
                                auto& reg = val.second;
                                if (reg->CanEnter(pb) && reg->GetID() != (uint64_t)pb->old_region_id())
                                {
                                        region = reg;
                                        break;
                                }
                        }
                        break;
                default :
                        break;
                }

                const auto regionGuid = region ? region->GetID() : GenGuid(_regionList, GetRegionType());
                auto reqActor = GetRegionMgrBase()->DistReqActor(regionGuid);
                reqAgent->BindActor(reqActor);
                reqAgent->BindExtra(region);
                if (!region)
                {
                        // TODO: 读取 region cfg 配置
                        auto mail = std::make_shared<MailRegionCreateInfo>();
                        mail->set_region_type(pb->region_type());
                        mail->set_region_id(regionGuid);
                        mail->set_param(fInfo.param());

                        auto gameAgent = ReqCreateRegionAgent(mail, reqActor);
                        if (!gameAgent)
                        {
                                errorType = E_IET_CreateRegionAgentError;
                                assert(false);
                                break;
                        }

                        reqAgent->BindExtra(gameAgent);
                        msg->_new = true;
                }

                auto lobbySes = reqAgent->GetSession();
                if (!lobbySes || !lobbySes->AddAgent(reqAgent))
                {
                        errorType = E_IET_AddAgentError;
                        break;
                }

                Send(reqActor, E_MCMT_GameCommon, E_MCGCST_ReqEnterRegion, msg);
                return nullptr;
        } while (0);

        if (reqAgent)
        {
                auto retMsg = std::make_shared<MailReqEnterRegionRet>();
                retMsg->set_error_type(errorType);
                reqAgent->CallRet(retMsg, msg->_guid, E_MCMT_GameCommon, E_MCGCST_ReqEnterRegion);
        }

        GetRegionMgrBase()->_reqList.Remove(fInfo.player_guid());
        return nullptr;
}

ActorMailDataPtr RegionMgrActor::DelRegion(const IActorPtr& from, const std::shared_ptr<MailRegionDestroyInfo>& msg)
{
        auto mail = std::make_shared<MailResult>();
        mail->set_error_type(E_IET_Success);

        _regionList.erase(msg->region_id());

        return mail;
}

SPECIAL_ACTOR_MAIL_HANDLE(RegionMgrActor, 0x1, MailRegionDestroyInfo)
{
        return DelRegion(from, msg);
}

ActorMailDataPtr RegionMgrActor::ExitRegion(const IActorPtr& from, const std::shared_ptr<MailReqExit>& msg)
{
        auto ret = std::make_shared<MailResult>();
        ret->set_error_type(E_IET_Success);

        auto it = _regionList.find(msg->region_id());
        if (_regionList.end() == it)
                return ret;

        auto region = it->second;
        if (!region)
        {
                _regionList.erase(it);
                return ret;
        }

        region->RemoveFighter(msg->player_guid());
        return ret;
}

SPECIAL_ACTOR_MAIL_HANDLE(RegionMgrActor, 0x2, MailReqExit)
{
        return ExitRegion(from, msg);
}

ActorMailDataPtr RegionMgrActor::CreateRegion(const IActorPtr& from, const std::shared_ptr<stQueueInfo>& msg)
{
        auto retMsg = std::make_shared<stMailCreateRegionAgent>();

        std::unordered_set<int64_t> idList;
        idList.reserve(msg->_playerList.size() * 2);
        for (auto& val : msg->_playerList)
        {
                const auto& agent = val.second->_agent;
                idList.emplace(agent->GetLocalID());
        }

        auto mail = std::make_shared<MailRegionCreateInfo>();
        mail->set_region_type(msg->_regionType);
        mail->set_region_id(GenGuid(_regionList, GetRegionType(), idList));
        mail->set_parent_id(msg->_parentID);
        mail->set_guid(msg->_guid);
        mail->set_param(msg->_param);
        mail->set_param_1(msg->_param_1);
        mail->set_param_2(msg->_param_2);

        auto regionAgent = ReqCreateRegionAgent(mail, std::dynamic_pointer_cast<RequestActor>(from));
        if (regionAgent)
        {
                regionAgent->_queueType = msg->_queueType;
                retMsg->_region = regionAgent;
        }

        return retMsg;
}

SPECIAL_ACTOR_MAIL_HANDLE(RegionMgrActor, 0x3, stQueueInfo)
{
        return CreateRegion(from, msg);
}

SPECIAL_ACTOR_MAIL_HANDLE(RegionMgrActor, 0xe, stRegionMgrMailCustom)
{
        return Custom(from, msg->_msg);
}

SPECIAL_ACTOR_MAIL_HANDLE(RegionMgrActor, 0xf, MailRegionRelationInfo)
{
        /*
         * 不必考虑 region release 异步情况。
         * 只要 GameServer 中的 region 没收到 Destroy 返回消息，就以此为准。
         * 连接只要断开，在这里加入之前，网络收到的消息是不会被处理的。
         * 因为 old ses 已经释放，其 agent 对应关系已经被删除。
         */

        auto gameSes = GetRegionMgrBase()->GetGameSes(msg->game_sid());
        if (!gameSes)
                return nullptr;

        auto thisPtr = shared_from_this();
        // 整体处理，防止因异步导致部分查找 lobby 出错。
        // 相同 sid lobby，要到全部找到，要到全部找不到。

        auto it = _regionList.find(msg->id());
        if (_regionList.end() == it) // game server 断连情况下，无法加入和退出region，因此数据不会发生变化。
        {
                auto reqActor = GetRegionMgrBase()->GetReqActor(msg->requestor_id());
                if (!reqActor)
                        return nullptr;

                // 没找到，RegionMgrServer 收到了 RegionDestroy 消息，但 GameServer 没收到。
                // 因此需要再创建一个 RegionAgent 给 GameServer 删，若 lobby player 重复收到处理一下。
                auto mail = std::make_shared<MailRegionCreateInfo>();
                mail->set_region_type(msg->region_type());
                mail->set_region_id(msg->id());
                mail->set_param(msg->param());

                auto regionAgent = GetRegionMgrBase()->CreateRegionAgent(mail, gameSes, reqActor);
                if (!regionAgent)
                {
                        LOG_WARN("GameServer[{}] 向 GameMgrServer 同步信息时，创建 regionAgent 失败!!!", msg->game_sid());
                        return nullptr;
                }

                for (auto& fInfo : msg->fighter_list())
                {
                        auto lobbySes = GetRegionMgrBase()->_lobbySesList.Get(fInfo.lobby_sid()).lock();
                        LOG_WARN_IF(!lobbySes, "GameServer[{}] 向 GameMgrServer 同步信息时，玩家[{}] 获取 lobbySes[{}] 失败!!!",
                                    msg->game_sid(), fInfo.id(), fInfo.lobby_sid());
                        auto reqAgent = std::make_shared<GameMgrLobbySession::ActorAgentType>(fInfo.id(), lobbySes);
                        reqAgent->BindActor(reqActor);
                        reqAgent->BindExtra(regionAgent);
                        if (lobbySes)
                        {
                                lobbySes->AddAgent(reqAgent);
                        }
                        else
                        {
                                LOG_WARN("GameServer sid[{}] 同步 region 时，lobby sid[{}] 未找到!!!", gameSes->GetSID(), fInfo.lobby_sid());;
                                GameMgrLobbySession::AddAgentOffline(fInfo.lobby_sid(), reqAgent);
                        }

                        regionAgent->AddFighter(reqAgent);
                }

                // 必须在最后设置，防止 GameServer 消息过来，能够找到 regionAgent，但找不到 reqAgent。
                _regionList.emplace(regionAgent->GetID(), regionAgent);
                regionAgent->_regionMgr = thisPtr;
                gameSes->AddAgent(regionAgent);
        }

        return nullptr;
}

ActorMailDataPtr CompetitionKnockoutRegionMgrActor::DelRegion(const IActorPtr& from, const std::shared_ptr<MailRegionDestroyInfo>& msg)
{
        auto mail = std::make_shared<MailResult>();

        auto it = _regionList.find(msg->region_id());
        if (_regionList.end() == it)
        {
                LOG_WARN("比赛中，删除 Region[{}] 时，未找到!!!", msg->region_id());
                return mail;
        }

        auto region = it->second;
        if (!region)
        {
                LOG_WARN("比赛中，删除 Region[{}] 时，region is nullptr!!!", msg->region_id());
                return mail;
        }

        for (auto& info : msg->player_info_list())
        {
                auto it = _playerList.find(info.id());
                if (_playerList.end() != it)
                {
                        _nextRoundPlayerList.emplace_back(it->second);
                }
                else
                {
                        auto it_ = _robotList.find(info.id());
                        if (_robotList.end() != it_)
                        {
                                _nextRoundRobotList.emplace_back(it_->second);
                                _robotList.erase(it_);
                        }
                }
        }

        region->ForeachFighter([this, &region, &msg](const auto& f) {
                if (_lastRoundRegionCnt > 1)
                {
                        for (auto& info : msg->player_info_list())
                        {
                                if (info.id() == f->GetID())
                                        return;
                        }
                }

                auto reqActor = std::dynamic_pointer_cast<RequestActor>(region->GetBindActor());
                if (reqActor)
                {
                        auto exitQueueMsg = std::make_shared<MailResult>();
                        exitQueueMsg->set_error_type(E_IET_Success);
                        Send(reqActor, f, E_MIMT_QueueCommon, E_MIQCST_ExitQueue, exitQueueMsg);
                }

                _playerList.erase(f->GetID());
                _multicastInfo.Remove(f->GetID());
        });

        _regionList.erase(it);
        if (_regionList.empty())
        {
                if (!_robotList.empty())
                {
                        auto backRobotMsg = std::make_shared<MailBackRobot>();
                        for (auto& val : _robotList)
                                backRobotMsg->add_robot_list(val.first);
                        RobotService::GetInstance()->BackRobot(shared_from_this(), backRobotMsg);
                }

                if (_lastRoundRegionCnt <= 1 || _nextRoundPlayerList.empty())
                {
                        Terminate();
                }
                else
                {
                        SyncMatchInfo(GetClock().GetTimeStamp() + 10);
                        std::weak_ptr<CompetitionKnockoutRegionMgrActor> weakThis = shared_from_this();
                        _startRoundTimer.Start(weakThis, 10, [weakThis]() {
                                auto thisPtr = weakThis.lock();
                                if (thisPtr)
                                        thisPtr->StartRound(std::move(thisPtr->_nextRoundPlayerList), std::move(thisPtr->_nextRoundRobotList));
                        });
                }
        }
        else
        {
                SyncMatchInfo();
        }

        mail->set_error_type(E_IET_Success);
        return mail;
}

ActorMailDataPtr CompetitionKnockoutRegionMgrActor::Custom(const IActorPtr& from, const std::shared_ptr<google::protobuf::MessageLite>& msg)
{
        auto info = std::dynamic_pointer_cast<MailMatchReqRank>(msg);
        if (!info)
                return nullptr;

        auto competitionCfg = GetRegionMgrBase()->GetCompetitionCfg(_param_1);
        if (!competitionCfg)
                return nullptr;

        info->set_error_type(E_IET_Success);
        info->set_rank(_rank);
        _rank -= _cfg->player_limit_max() - competitionCfg->_promotionParam;
        return info;
}

void CompetitionKnockoutRegionMgrActor::StartRound(std::vector<stPlayerInfoPtr>&& playerList
                                                   , std::vector<std::shared_ptr<MailSyncPlayerInfo2Region>>&& robotList)
{
        auto competitionCfg = GetRegionMgrBase()->GetCompetitionCfg(_param_1);
        if (!competitionCfg)
                return;

        if (-1 == _lastRoundRegionCnt)
        {
                for (auto& p : playerList)
                {
                        _multicastInfo.Add(p->GetID(), p->_agent->GetSession());
                        _playerList.emplace(p->GetID(), p);
                }
        }

        _robotList.clear();
        auto backRobotMsg = std::make_shared<MailBackRobot>();

        std::vector<uint64_t> regionGuidList;
        const int64_t mapID = GetValueOrLast(_mapList, _roundCnt);
        ++_roundCnt;
        auto thisPtr = shared_from_this();
        _lastRoundRegionCnt = std::ceil((playerList.size() + robotList.size()) / (double)_cfg->player_limit_max());
        regionGuidList.reserve(_lastRoundRegionCnt);
        _rank = _lastRoundRegionCnt * _cfg->player_limit_max();
        const auto playerCntPer = playerList.size() / _lastRoundRegionCnt;
        int64_t moreIdx = playerList.size() % _lastRoundRegionCnt;
        for (int64_t i=0; i<_lastRoundRegionCnt; ++i)
        {
                auto tmpPlayerCnt = playerCntPer + (moreIdx--<=0?0:1);
                auto robotCnt = _cfg->player_limit_max() - tmpPlayerCnt;
                if (tmpPlayerCnt > 0)
                {
                        auto tmpInfo = std::make_shared<stQueueInfo>(i, GetRegionType(), _queueType);
                        tmpInfo->_regionMgr = thisPtr;
                        tmpInfo->_param = mapID;
                        tmpInfo->_param_1 = _param_1;
                        tmpInfo->_param_2 = _roundCnt;

                        tmpInfo->_parentID = _guid;
                        tmpInfo->_guid = LogService::GetInstance()->GenGuid();
                        regionGuidList.emplace_back(tmpInfo->_guid);

                        for (auto& p : Random(playerList, tmpPlayerCnt))
                                tmpInfo->_playerList.emplace(p->GetID(), p);

                        if (robotCnt > 0)
                        {
                                for (auto& r : Random(robotList, robotCnt))
                                {
                                        tmpInfo->_robotList.emplace(r->player_guid(), r);
                                        _robotList.emplace(r->player_guid(), r);
                                }
                        }

                        auto reqActor = GetRegionMgrBase()->DistReqActor(tmpInfo->GetID());
                        Send(reqActor, E_MIMT_QueueCommon, E_MIQCST_ReqQueue, tmpInfo);
                }
                else
                {
                        LOG_INFO("该房间全是机器人!!!");
                        int64_t idx = 0;
                        _rank -= _cfg->player_limit_max() - competitionCfg->_promotionParam;
                        for (auto& r : Random(robotList, robotCnt))
                        {
                                if (++idx <= competitionCfg->_promotionParam)
                                        _nextRoundRobotList.emplace_back(r);
                                else
                                        backRobotMsg->add_robot_list(r->player_guid());
                        }
                }
        }

        if (backRobotMsg->robot_list_size() > 0)
                RobotService::GetInstance()->BackRobot(thisPtr, backRobotMsg);

        std::string idListStr;
        for (auto id : regionGuidList)
                idListStr += fmt::format("{},", id);
        if (!idListStr.empty())
                idListStr.pop_back();
        std::string str = fmt::format("{}\"id_list\":[{}],\"round\":{},\"parent_id\":{}{}", "{", idListStr, _roundCnt, _parentID, "}");
        LogService::GetInstance()->Log<E_LSLMT_Content>(_guid, str, E_LSLST_Competition, _queueType, E_LSOT_CompetitionRoundStart, _guid);
}

void CompetitionKnockoutRegionMgrActor::SyncMatchInfo(time_t nextRoundStartTime/* = 0*/)
{
        auto m = std::make_shared<MsgMatchInfo>();
        m->set_cfg_id(_param_1);
        m->set_cur_round(_roundCnt);
        m->set_prompotion_cnt(_nextRoundPlayerList.size() + _nextRoundRobotList.size());
        m->set_left_region_cnt(_regionList.size());
        m->set_next_round_start_time(nextRoundStartTime);
        _multicastInfo.MultiCast(_qInfo->GetID(), E_MIMT_MatchCommon, E_MIMCST_Info, m);
}

// }}}

// {{{ RegionMgrBase
RegionMgrBase::RegionMgrBase()
        : SuperType("RegionMgrBase")
          , _reqList("RegionMgrBase_reqList")
          , _lobbySesList("RegionMgrBasse_lobbySesList")
{
}

RegionMgrBase::~RegionMgrBase()
{
}

bool RegionMgrBase::Init()
{
	if (!SuperType::Init())
		return false;

        LOG_FATAL_IF(!nl::af::impl::ReadCompetitionCfg(_competitionCfgList), "读取 Match.txt 配置错误!!!");

        if (!nl::af::impl::ReadRegionCfg(_regionCfgList))
                return false;

        for (int32_t i=0; i<ERegionType_ARRAYSIZE; ++i)
        {
                if (ERegionType_GameValid(i))
                {
                        auto cfg = _regionCfgList.Get(static_cast<ERegionType>(i));
                        if (!cfg)
                                continue;

                        switch (i)
                        {
                        case E_RT_PVP_CompetitionKnockout :
                                break;
                        default :
                                {
                                        auto mgr = CreateRegionMgrActor(cfg);
                                        mgr->Start();
                                        _regionMgrActorArr[i] = mgr;
                                }
                                break;
                        }

                        for (int64_t j=0; j<EQueueType_ARRAYSIZE; ++j)
                        {
                                if (EQueueType_GameValid(j))
                                {
                                        auto qMgr = CreateQueueMgrActor(cfg, static_cast<EQueueType>(j));
                                        qMgr->Start();
                                        _queueMgrActorArr[j][i] = qMgr;
                                }
                        }
                }
        }

        auto gameMgrInfo = ServerListCfgMgr::GetInstance()->GetFirst<stGameMgrServerInfo>();
        for (int32_t i=0; i<gameMgrInfo->_workersCnt * gameMgrInfo->_actorCntPerWorkers /*512*/; ++i)
        {
                auto req = std::make_shared<RequestActor>(GetApp()->GetSID() * 1000 * 1000 + i);
                req->Start();
                _reqActorArr.emplace_back(req);
        }

        std::sort(_reqActorArr.begin(), _reqActorArr.end(), [](const auto& lhs, const auto& rhs) {
                return lhs->GetID() < rhs->GetID();
        });

	return true;
}

GameMgrGameSession::ActorAgentTypePtr RegionMgrBase::CreateRegionAgent(const std::shared_ptr<MailRegionCreateInfo>& cfg,
                                                                   const std::shared_ptr<GameMgrGameSession>& ses,
                                                                   const RequestActorPtr& b)
{
        switch (cfg->region_type())
        {
        default :
                return std::make_shared<GameMgrGameSession::ActorAgentType>(cfg, ses, b);
                break;
        }

        return nullptr;
}

void RegionMgrBase::UnRegistGameSes(const std::shared_ptr<GameMgrGameSession>& ses)
{
        if (!ses)
                return;

        std::lock_guard l(_gameSesArrMutex);
        if (_gameSesArr.empty())
                return;

        auto it = std::find_if(_gameSesArr.begin(), _gameSesArr.end(), [sid{ses->GetSID()}](const auto& wlhs) {
                auto lhs = wlhs.lock();
                return lhs ? sid == lhs->GetSID() : false;
        });
        _gameSesArr.erase(it);
        std::sort(_gameSesArr.begin(), _gameSesArr.end(), [](const auto& wlhs, const auto& wrhs) {
                auto lhs = wlhs.lock();
                auto rhs = wrhs.lock();
                return lhs && rhs ? lhs->GetSID() < rhs->GetSID() : false;
        });
}

RegionMgrActorPtr RegionMgrBase::CreateRegionMgrActor(const std::shared_ptr<RegionCfg>& cfg) const
{
        switch (cfg->region_type())
        {
        case E_RT_PVP_CompetitionKnockout :
                // return std::make_shared<CompetitionKnockoutRegionMgrActor>(cfg);
                break;
        default :
                return std::make_shared<RegionMgrActor>(cfg);
                break;
        }
        return nullptr;
}

QueueMgrActorPtr RegionMgrBase::CreateQueueMgrActor(const std::shared_ptr<RegionCfg>& cfg, EQueueType queueType) const
{
        switch (queueType)
        {
        case E_QT_Normal : return std::make_shared<NormalQueueMgrActor>(cfg); break;
        case E_QT_Manual : return std::make_shared<ManualQueueMgrActor>(cfg); break;
        case E_QT_CompetitionKnokout : return std::make_shared<CompetitionKnockoutQueueMgrActor>(cfg); break;
        default : return nullptr; break;
        }
}

void RegionMgrBase::Terminate()
{
        for (int64_t i=0; i<ERegionType_ARRAYSIZE; ++i)
        {
                if (ERegionType_GameValid(i))
                {
                        if (_regionMgrActorArr[i])
                                _regionMgrActorArr[i]->Terminate();

                        for (int64_t j=0; j<EQueueType_ARRAYSIZE; ++j)
                        {
                                if (EQueueType_GameValid(j))
                                {
                                        if (_queueMgrActorArr[j][i])
                                                _queueMgrActorArr[j][i]->Terminate();
                                }
                        }
                }
        }

        for (int64_t i=0; i<ERegionType_ARRAYSIZE; ++i)
        {
                if (ERegionType_GameValid(i))
                {
                        if (_regionMgrActorArr[i])
                                _regionMgrActorArr[i]->WaitForTerminate();

                        for (int64_t j=0; j<EQueueType_ARRAYSIZE; ++j)
                        {
                                if (EQueueType_GameValid(j))
                                {
                                        if (_queueMgrActorArr[j][i])
                                                _queueMgrActorArr[j][i]->WaitForTerminate();
                                }
                        }
                }
        }

        SuperType::Terminate();
}

void RegionMgrBase::WaitForTerminate()
{
        SuperType::WaitForTerminate();
}

NET_MSG_HANDLE(GameMgrGameSession, E_MIMT_Internal, E_MIIST_SyncRelation, MailRegionRelationInfo)
{
        auto regionMgr = GetRegionMgrBase()->GetRegionMgrActor(msg->region_type());
        if (!regionMgr)
        {
                LOG_WARN("GameServer sid[{}] 在同步 region type[{}] id[{}] 时，获取 regionMgrActor 失败!!!",
                         GetSID(), msg->region_type(), msg->id());
                return;
        }

        regionMgr->SendPush(0xf, msg);

        return;
}

NET_MSG_HANDLE(GameMgrLobbySession, E_MCMT_GameCommon, E_MCGCST_ReqEnterRegion, MailReqEnterRegion)
{
        // LOG_INFO("收到玩家[{}] 请求进入场景消息!!!", msg->player_guid());
        if (!ERegionType_GameValid(msg->region_type())
            || msg->fighter_list_size() <= 0)
                return;

        auto fInfo = msg->fighter_list(0);
        if (GetRegionMgrBase()->_reqList.Add(fInfo.player_guid()))
        {
                auto data = std::make_shared<stMailReqEnterRegion>();
                data->_agent = std::make_shared<GameMgrLobbySession::ActorAgentType>(fInfo.player_guid(), shared_from_this());
                data->_guid = msgHead._guid;
                data->_msg = msg;

                GetRegionMgrBase()->GetRegionMgrActor(msg->region_type())->SendPush(0, data);
        }
        else
        {
                LOG_WARN("玩家[{}] 请求进入场景时，已经有别的请求了!!!", fInfo.player_guid());
                auto sendMsg = std::make_shared<MailResult>();
                sendMsg->set_error_type(E_IET_AlreadyReqEnterRegion);
                SendPB(sendMsg, E_MCMT_GameCommon, E_MCGCST_ReqEnterRegion,
                       MsgHeaderType::E_RMT_CallRet, msgHead._guid, msgHead._to, msgHead._from);
        }
}

NET_MSG_HANDLE(GameMgrLobbySession, E_MCMT_QueueCommon, E_MCQCST_ReqQueue, MailReqQueue)
{
        // DLOG_INFO("收到玩家[{}] 请求排队消息!!!", msg->player_guid());
        auto queueBaseInfo = msg->base_info();
        if (ERegionType_GameValid(queueBaseInfo.region_type())
            && EQueueType_GameValid(queueBaseInfo.queue_type())
            && msg->player_list_size() > 0
            && GetRegionMgrBase()->_reqList.Add(msg->player_guid()))
        {
                ++GetApp()->_reqQueueCnt;
                auto data = std::make_shared<stMailReqQueue>();
                data->_agent = std::make_shared<GameMgrLobbySession::ActorAgentType>(msg->player_guid(), shared_from_this());
                data->_msgHead = msgHead;
                data->_msg = msg;

                GetRegionMgrBase()->GetQueueMgrActor(queueBaseInfo.region_type(), queueBaseInfo.queue_type())->SendPush(E_MCQCST_ReqQueue, data);
        }
        else
        {
                LOG_WARN("玩家[{}] 请求排队时，已经有别的请求了!!!", msg->player_guid());
                auto sendMsg = std::make_shared<MailResult>();
                sendMsg->set_error_type(E_IET_AlreadyReqQueue);
                SendPB(sendMsg, E_MCMT_QueueCommon, E_MCQCST_ReqQueue,
                       MsgHeaderType::E_RMT_CallRet, msgHead._guid, msgHead._to, msgHead._from);
        }
}

NET_MSG_HANDLE(GameMgrLobbySession, E_MCMT_QueueCommon, E_MCQCST_ExitQueue, MsgExitQueue)
{
        // DLOG_INFO("收到玩家[{}] 请求退出排队消息!!!", msg->player_guid());
        auto queueBaseInfo = msg->base_info();
        if (ERegionType_GameValid(queueBaseInfo.region_type())
            && EQueueType_GameValid(queueBaseInfo.queue_type())
            && GetRegionMgrBase()->_reqList.Add(msg->player_guid()))
        {
                auto agent = GetAgent(msgHead._from, msgHead._to);
                if (agent)
                {
                        GetRegionMgrBase()->GetQueueMgrActor(queueBaseInfo.region_type(), queueBaseInfo.queue_type())->
                                CallPushInternal(agent, E_MCMT_QueueCommon, E_MCQCST_ExitQueue, msg, msgHead._guid);
                        return;
                }
                GetRegionMgrBase()->_reqList.Remove(msg->player_guid());
        }

        LOG_WARN("玩家[{}] 请求退出排队时，已经有别的请求了!!!", msg->player_guid());
        auto sendMsg = std::make_shared<MailResult>();
        sendMsg->set_error_type(E_IET_AlreadyExitQueue);
        SendPB(sendMsg, E_MCMT_QueueCommon, E_MCQCST_ExitQueue,
               MsgHeaderType::E_RMT_CallRet, msgHead._guid, msgHead._to, msgHead._from);
}

NET_MSG_HANDLE(GameMgrLobbySession, E_MCMT_QueueCommon, E_MCQCST_Opt, MsgQueueOpt)
{
        // DLOG_INFO("收到玩家[{}] 请求退出排队消息!!!", msg->player_guid());
        auto queueBaseInfo = msg->base_info();
        if (ERegionType_GameValid(queueBaseInfo.region_type())
            && EQueueType_GameValid(queueBaseInfo.queue_type())
            && GetRegionMgrBase()->_reqList.Add(msg->player_guid()))
        {
                auto agent = GetAgent(msgHead._from, msgHead._to);
                if (agent)
                {
                        GetRegionMgrBase()->GetQueueMgrActor(queueBaseInfo.region_type(), queueBaseInfo.queue_type())->
                                CallPushInternal(agent, E_MCMT_QueueCommon, E_MCQCST_Opt, msg, msgHead._guid);
                        return;
                }
                GetRegionMgrBase()->_reqList.Remove(msg->player_guid());
        }

        LOG_WARN("玩家[{}] 请求操作排队时，已经有别的请求了!!!", msg->player_guid());
        auto sendMsg = std::make_shared<MailResult>();
        sendMsg->set_error_type(E_IET_AlreadyExitQueue);
        SendPB(sendMsg, E_MCMT_QueueCommon, E_MCQCST_Opt,
               MsgHeaderType::E_RMT_CallRet, msgHead._guid, msgHead._to, msgHead._from);
}

NET_MSG_HANDLE(GameMgrLobbySession, E_MCMT_QueueCommon, E_MCQCST_ReqQueueList, MsgReqQueueList)
{
        if (ERegionType_GameValid(msg->region_type())
            && EQueueType_GameValid(msg->queue_type())
            && GetRegionMgrBase()->_reqList.Add(msg->player_guid()))
        {
                auto data = std::make_shared<stMailReqQueue>();
                data->_agent = std::make_shared<GameMgrLobbySession::ActorAgentType>(msg->player_guid(), shared_from_this());
                data->_msgHead = msgHead;
                data->_msg = msg;

                GetRegionMgrBase()->GetQueueMgrActor(msg->region_type(), msg->queue_type())->
                        CallPushInternal(data->_agent, E_MCMT_QueueCommon, E_MCQCST_ReqQueueList, data, msgHead._guid);
        }
        else
        {
                LOG_WARN("玩家[{}] 请求排队列表时，已经有别的请求了!!!", msg->player_guid());
                msg->set_error_type(E_CET_Fail);
                SendPB(msg, E_MCMT_QueueCommon, E_MCQCST_ReqQueueList,
                       MsgHeaderType::E_RMT_CallRet, msgHead._guid, msgHead._to, msgHead._from);
        }
}

// }}}

// {{{ QueueMgrActor

QueueMgrActor::QueueMgrActor(const std::shared_ptr<RegionCfg>& cfg)
        : _cfg(cfg)
{
}

SPECIAL_ACTOR_MAIL_HANDLE(QueueMgrActor, E_MCQCST_ReqQueue, stMailReqQueue)
{
        auto pb = std::dynamic_pointer_cast<MailReqQueue>(msg->_msg);
        DLOG_INFO("收到玩家[{}] 请求排队消息!!!", pb->player_guid());
        ReqQueue(msg);
        GetRegionMgrBase()->_reqList.Remove(pb->player_guid());
        return nullptr;
}

SPECIAL_ACTOR_MAIL_HANDLE(QueueMgrActor, E_MCQCST_ExitQueue, MsgExitQueue)
{
        DLOG_INFO("收到玩家[{}] 请求退出排队消息!!!", msg->player_guid());
        auto retMsg = std::make_shared<MailResult>();
        do
        {
                auto queueBaseInfo = msg->base_info();
                if (GetRegionType() != queueBaseInfo.region_type()
                    || GetQueueType() != queueBaseInfo.queue_type())
                {
                        LOG_WARN("玩家[{}] 排队操作时，actor rt[{}] qt[{}] player rt[{}] qt[{}]",
                                  msg->player_guid(),
                                  GetRegionType(), GetQueueType(),
                                  queueBaseInfo.region_type(), queueBaseInfo.region_type());
                        retMsg->set_error_type(E_IET_ParamError);
                        break;
                }

                retMsg->set_guid(msg->guid());
                retMsg->set_error_type(ExitQueue(msg, true));
        } while (0);

        GetRegionMgrBase()->_reqList.Remove(msg->player_guid());
        return retMsg;
}

SPECIAL_ACTOR_MAIL_HANDLE(QueueMgrActor, E_MCQCST_Opt, MsgQueueOpt)
{
        auto ret = std::make_shared<MailResult>();
        do
        {
                auto queueBaseInfo = msg->base_info();
                if (GetRegionType() != queueBaseInfo.region_type()
                    || GetQueueType() != queueBaseInfo.queue_type())
                {
                        LOG_WARN("玩家[{}] 排队操作时，actor rt[{}] qt[{}] player rt[{}] qt[{}]",
                                 msg->player_guid(),
                                 GetRegionType(), GetQueueType(),
                                 queueBaseInfo.region_type(), queueBaseInfo.region_type());
                        break;
                }

                auto qInfo = QueueOpt(msg);
                if (qInfo)
                {
                        switch (msg->opt_type())
                        {
                        case E_QOT_Start :
                                {
                                        auto reqAgent = std::dynamic_pointer_cast<GameMgrLobbySession::ActorAgentType>(from);
                                        if (reqAgent)
                                        {
                                                qInfo->_regionMgr = GetRegionMgrBase()->GetRegionMgrActor(qInfo->_regionType);
                                                qInfo->_guid = LogService::GetInstance()->GenGuid();
                                                Send(reqAgent->GetBindActor(), E_MIMT_QueueCommon, E_MIQCST_ReqQueue, qInfo);
                                        }
                                }
                                break;
                        default :
                                break;
                        }
                }
                ret->set_error_type(qInfo ? E_IET_Success : E_IET_QueueOptError);
        } while (0);

        GetRegionMgrBase()->_reqList.Remove(msg->player_guid());
        return ret;
}

SPECIAL_ACTOR_MAIL_HANDLE(QueueMgrActor, E_MCQCST_ReqQueueList, stMailReqQueue)
{
        auto pb = std::dynamic_pointer_cast<MsgReqQueueList>(msg->_msg);
        DLOG_INFO("收到玩家[{}] 请求排队列表消息!!!", pb->player_guid());

        if (0 == pb->queue_guid())
        {
                for (auto& qInfo : _queueList.get<by_time>())
                {
                        if (qInfo->_matched)
                                continue;

                        auto msgInfo = pb->add_queue_list();
                        qInfo->PackBaseInfo(*msgInfo->mutable_base_info());
                        msgInfo->set_param(qInfo->_param);
                        if (pb->queue_list_size() >= 10)
                                break;
                }

                pb->set_error_type(E_CET_Success);
        }
        else
        {
                auto& seq = _queueList.get<by_guid>();
                auto it = seq.find(pb->queue_guid());
                if (seq.end() != it)
                {
                        auto qInfo = *it;
                        pb->set_error_type(E_CET_Success);

                        auto msgInfo = pb->add_queue_list();
                        qInfo->PackBaseInfo(*msgInfo->mutable_base_info());
                        msgInfo->set_param(qInfo->_param);
                }
                else
                {
                        pb->set_error_type(E_CET_Fail);
                }
        }

        GetRegionMgrBase()->_reqList.Remove(pb->player_guid());
        return pb;
}

// SPECIAL_ACTOR_MAIL_HANDLE(QueueMgrActor, 0x2, MsgExitQueue)
// {
//         auto retMsg = std::make_shared<MailResult>();
//         do
//         {
//                 auto queueBaseInfo = msg->base_info();
//                 if (GetRegionType() != queueBaseInfo.region_type()
//                     || GetQueueType() != queueBaseInfo.queue_type())
//                 {
//                         LOG_WARN("玩家[{}] 排队操作时，actor rt[{}] qt[{}] player rt[{}] qt[{}]",
//                                   msg->player_guid(),
//                                   GetRegionType(), GetQueueType(),
//                                   queueBaseInfo.region_type(), queueBaseInfo.region_type());
//                         retMsg->set_error_type(E_IET_ParamError);
//                         break;
//                 }
// 
//                 retMsg->set_error_type(ExitQueue(msg, true));
//         } while (0);
//         return retMsg;
// }

SPECIAL_ACTOR_MAIL_HANDLE(QueueMgrActor, 0xf, MailInt)
{
        /* TODO:
        if (GetRegionType() != msg->region_type()
            || GetQueueType() != msg->queue_type())
        {
                LOG_WARN("玩家[{}] 排队操作时，actor rt[{}] qt[{}] player rt[{}] qt[{}]",
                          msg->player_guid(),
                          GetRegionType(), GetQueueType(),
                          msg->region_type(), msg->region_type());
                return nullptr;
        }
        */

        auto& seq = _queueList.get<by_guid>();
        auto it = seq.find(msg->val());
        if (seq.end() != it)
        {
                auto qInfo = *it;
                qInfo->_matched = false;
                for (auto& val : qInfo->_playerList)
                {
                        val.second->_attrList[0] = 0;
                        auto& agent = val.second->_agent;
                        auto ses = agent->GetSession();
                        if (ses)
                        {
                                agent->BindExtra(qInfo);
                                ses->AddAgent(agent);
                        }
                }
                qInfo->Sync2Lobby();
        }
        return nullptr;
}

// }}}

// {{{ NomalQueueMgrActor

bool NormalQueueMgrActor::Init()
{
        if (!SuperType::Init())
                return false;

        InitCheckTimer();
        return true;
}

std::vector<stQueueInfoPtr> NormalQueueMgrActor::CheckSuitableImmediately(const stQueueInfoPtr& qInfo)
{
        const int64_t cnt = _cfg->player_limit_max() - 1;
        std::vector<stQueueInfoPtr> retList;
        if (!qInfo)
                return retList;

        retList.reserve(cnt);
        auto& seq = _queueList.get<by_rank_time>();
        auto range = seq.equal_range(qInfo->_rank);
        for (auto it=range.first; range.second!=it; ++it)
        {
                auto info = *it;
                if (!info->_matched
                    && qInfo != info
                    && info->_param == qInfo->_param)
                {
                        retList.emplace_back(info);
                        if (static_cast<int64_t>(retList.size()) >= cnt)
                                return retList;
                }
        }
        return retList;
}

std::vector<stQueueInfoPtr> NormalQueueMgrActor::CheckSuitableTimeout(const stQueueInfoPtr& qInfo, time_t now, bool& isRobot)
{
        const int64_t cnt = _cfg->player_limit_max() - 1;
        std::vector<stQueueInfoPtr> retList;
        if (!qInfo)
                return retList;

        retList.reserve(cnt);
        auto& seq = _queueList.get<by_rank_time>();
        const auto diff = GlobalSetup_CH::GetInstance()->CalQueueIntervalCnt(now - qInfo->_time);

        if (0 < diff)
        {
                for (int64_t i=1; i<=diff; ++i)
                {
                        for (int64_t t : { 1, -1 })
                        {
                                auto it = seq.lower_bound(boost::make_tuple(qInfo->_rank + i * t, now - GlobalSetup_CH::GetInstance()->_queueIntervalList[i])); // 大于等于。
                                const auto itEnd = seq.lower_bound(boost::make_tuple(qInfo->_rank + i * t, now - GlobalSetup_CH::GetInstance()->_queueIntervalList[i-1]));
                                for (; itEnd!=it; ++it)
                                {
                                        auto info = *it;
                                        if (!info->_matched
                                            && info->_param == qInfo->_param)
                                        {
                                                retList.emplace_back(info);
                                                if (static_cast<int64_t>(retList.size()) >= cnt)
                                                        return retList;
                                        }
                                }
                        }
                }
                retList.clear();
        }
        else // 匹配机器人特殊处理。
        {
                if (now - qInfo->_time < GlobalSetup_CH::GetInstance()->_queueIntervalList.back())
                        return retList;

                isRobot = true;
                static std::vector<int64_t> rankList = []() {
                        static std::vector<int64_t> retList;
                        retList.reserve((GlobalSetup_CH::GetInstance()->_queueIntervalList.size() - 2) * 2 + 1);
                        retList.emplace_back(0);
                        for (int64_t i=0; i<static_cast<int64_t>(GlobalSetup_CH::GetInstance()->_queueIntervalList.size()-2); ++i)
                        {
                                retList.emplace_back(i+1);
                                retList.emplace_back(-i-1);
                        }
                        return retList;
                }();

                for (auto i : rankList)
                {
                        auto it = seq.lower_bound(qInfo->_rank + i);
                        const auto itEnd = seq.upper_bound(boost::make_tuple(qInfo->_rank + i, now - GlobalSetup_CH::GetInstance()->_queueIntervalList[GlobalSetup_CH::GetInstance()->_queueIntervalList.size()-2]));
                        for (; itEnd!=it; ++it)
                        {
                                auto info = *it;
                                if (!info->_matched
                                    && info->_param == qInfo->_param)
                                {
                                        retList.emplace_back(info);
                                        if (static_cast<int64_t>(retList.size()) >= cnt)
                                                return retList;
                                }
                        }
                }
        }

        return retList;
}

bool NormalQueueMgrActor::MatchQueue(const std::vector<stQueueInfoPtr>& queueList, const stQueueInfoPtr& rhs, bool timeout)
{
        if (!rhs)
                return false;

        if (timeout)
        {
                if (static_cast<int64_t>(queueList.size()) < _cfg->player_limit_min() - 1)
                        return false;
        }
        else
        {
                if (static_cast<int64_t>(queueList.size()) < _cfg->player_limit_max() - 1)
                        return false;
        }

        for (auto& val : rhs->_playerList)
        {
                val.second->_camp = val.second->GetID();
        }

        rhs->_matched = true;
        for (auto& info : queueList)
        {
                info->_matched = true;
                for (auto& val : info->_playerList)
                        val.second->_camp = val.second->GetID();
                rhs->_playerList.insert(info->_playerList.begin(), info->_playerList.end());
        }
        return true;
}


void NormalQueueMgrActor::InitCheckTimer()
{
        if (!_queueList.empty())
        {
                auto now = GetClock().GetTimeStamp();
                std::vector<int64_t> idList;
                idList.reserve(64);

                auto matchFunc = [this, &idList](const auto& otherInfoList, const auto& qInfo, bool isRobot) {
                        if (MatchQueue(otherInfoList, qInfo, isRobot))
                        {
                                auto reqActor = GetRegionMgrBase()->DistReqActor(qInfo->GetID());
                                qInfo->_regionMgr = GetRegionMgrBase()->GetRegionMgrActor(qInfo->_regionType);
                                qInfo->_guid = LogService::GetInstance()->GenGuid();
                                Send(reqActor, E_MIMT_QueueCommon, E_MIQCST_ReqQueue, qInfo);

                                idList.emplace_back(qInfo->GetID());
                                for (auto& info : otherInfoList)
                                idList.emplace_back(info->GetID());
                        }
                };

                {
                        // 只匹配机器人。
                        auto& seq = _queueList.get<by_rank_time>();
                        std::vector<stQueueInfoPtr> otherInfoList;
                        for (int64_t r=(*seq.begin())->_rank; r<=GlobalSetup_CH::GetInstance()->_queueOnlyRobotRankList[0]; ++r)
                        {
                                auto it = seq.lower_bound(r);
                                auto ie = seq.lower_bound(boost::make_tuple(r, now-GlobalSetup_CH::GetInstance()->_queueOnlyRobotRankList[1]));
                                for (; ie!=it; ++it)
                                {
                                        auto qInfo = *it;
                                        if (!qInfo->_matched)
                                                matchFunc(otherInfoList, qInfo, true);
                                }
                        }

                        for (int64_t id : idList)
                        _queueList.get<by_guid>().erase(id);
                }

                {
                        // 优先匹配玩家，超时才匹配机器人。
                        auto& seq = _queueList.get<by_time>();
                        idList.clear();
                        auto ie = seq.upper_bound(now - GlobalSetup_CH::GetInstance()->_queueIntervalList[0]);
                        for (auto it=seq.begin(); ie!=it; ++it)
                        {
                                auto qInfo = *it;
                                if (qInfo->_matched)
                                        continue;

                                bool isRobot = false;
                                auto otherInfoList = CheckSuitableTimeout(qInfo, now, isRobot);
                                matchFunc(otherInfoList, qInfo, isRobot);
                        }

                        for (int64_t id : idList)
                        _queueList.get<by_guid>().erase(id);
                }
        }

        std::weak_ptr<NormalQueueMgrActor> weakPtr = shared_from_this();
        _checkTimer.Start(weakPtr, 1, [weakPtr]() {
                auto thisPtr = weakPtr.lock();
                if (thisPtr)
                        thisPtr->InitCheckTimer();
        });
}

void NormalQueueMgrActor::ReqQueue(const std::shared_ptr<stMailReqQueue>& msg)
{
        auto pb = std::dynamic_pointer_cast<MailReqQueue>(msg->_msg);
        auto queueBaseInfo = pb->base_info();
        auto reqAgent = msg->_agent;
        auto ses = reqAgent->GetSession();
        if (!ses)
                return;

        auto now = GetClock().GetTimeStamp();
        auto p = std::make_shared<stPlayerInfo>();
        p->_agent = reqAgent;
        p->_lobbySID = pb->lobby_sid();
        p->_gateSID = pb->gate_sid();

        auto& seq = _queueList.get<by_guid>();
        const auto queueGuid = GenGuid(seq, GetRegionType());
        auto qInfo = std::make_shared<stQueueInfo>(queueGuid, queueBaseInfo.region_type(), queueBaseInfo.queue_type());
        qInfo->_rank = pb->player_list(0).rank();
        qInfo->_time = now;
        qInfo->_param = pb->param();

        qInfo->_playerList.emplace(reqAgent->GetID(), p);

        pb->set_error_type(E_IET_Success);
        qInfo->PackBaseInfo(*pb->mutable_base_info());

        auto reqActor = GetRegionMgrBase()->DistReqActor(qInfo->GetID());
        reqAgent->BindActor(reqActor);
        reqAgent->BindExtra(qInfo);
        if (!ses->AddAgent(reqAgent))
        {
                pb->set_error_type(E_IET_AddAgentError);
                ses->SendPB(pb, E_MCMT_QueueCommon, E_MCQCST_ReqQueue,
                            GameMgrLobbySession::MsgHeaderType::E_RMT_CallRet, msg->_msgHead._guid, msg->_msgHead._to, msg->_msgHead._from);
                return;
        }

        ses->SendPB(pb, E_MCMT_QueueCommon, E_MCQCST_ReqQueue,
                    GameMgrLobbySession::MsgHeaderType::E_RMT_CallRet, msg->_msgHead._guid, msg->_msgHead._to, msg->_msgHead._from);

        auto otherQueueInfoList = CheckSuitableImmediately(qInfo);
        if (MatchQueue(otherQueueInfoList, qInfo, false))
        {
                qInfo->_regionMgr = GetRegionMgrBase()->GetRegionMgrActor(qInfo->_regionType);
                qInfo->_guid = LogService::GetInstance()->GenGuid();
                Send(reqActor, E_MIMT_QueueCommon, E_MIQCST_ReqQueue, qInfo);
                for (auto& info : otherQueueInfoList)
                        seq.erase(info->GetID());
        }
        else // 匹配失败。
        {
                _queueList.emplace(qInfo);
        }
}

EInternalErrorType NormalQueueMgrActor::ExitQueue(const std::shared_ptr<MsgExitQueue>& msg, bool self)
{
        if (!msg)
                return E_IET_ParamError;

        auto& seq = _queueList.get<by_guid>();
        auto it = seq.find(msg->base_info().queue_guid());
        if (seq.end() == it)
                return E_IET_QueueNotFound;

        auto qInfo = *it;
        if (!qInfo || qInfo->_matched || !qInfo->Equal(msg->base_info()))
                return E_IET_QueueStateError;

        msg->set_guid(qInfo->_guid);
        auto& playerList = qInfo->_playerList;
        auto it_ = playerList.find(msg->player_guid());
        if (playerList.end() != it_)
        {
                playerList.erase(it_);
                if (playerList.empty())
                        seq.erase(it);
        }

        return E_IET_Success;
}

// }}}

// {{{ ManualQueueMgrActor

void ManualQueueMgrActor::ReqQueue(const std::shared_ptr<stMailReqQueue>& msg)
{
        std::shared_ptr<GameMgrLobbySession> ses;
        auto pb = std::dynamic_pointer_cast<MailReqQueue>(msg->_msg);
        auto queueBaseInfo = pb->base_info();
        pb->set_error_type(E_IET_Success);
        DLOG_INFO("收到玩家[{}] 请求排队消息!!!", pb->player_guid());
        auto reqAgent = msg->_agent;
        stQueueInfoPtr qInfo;
        do
        {
                if (!reqAgent)
                {
                        pb->set_error_type(E_IET_ReqAgentIsNull);
                        break;
                }

                ses = reqAgent->GetSession();
                if (!ses)
                {
                        pb->set_error_type(E_IET_LobbySesAlreadyRelease);
                        break;
                }

                if (pb->player_list_size() <= 0)
                {
                        pb->set_error_type(E_IET_ParamError);
                        break;
                }

                auto& seq = _queueList.get<by_guid>();
                if (0 != queueBaseInfo.queue_guid())
                {
                        auto it = seq.find(queueBaseInfo.queue_guid());
                        if (seq.end() != it)
                                qInfo = *it;
                }
                else
                {
                        auto& seq = _queueList.get<by_guid>();
                        const auto queueGuid = GenGuid(seq, GetRegionType());
                        qInfo = std::make_shared<stQueueInfo>(queueGuid, queueBaseInfo.region_type(), queueBaseInfo.queue_type());
                        qInfo->_rank = pb->player_list(0).rank();
                        qInfo->_time = GetClock().GetTimeStamp();
                        qInfo->_param = pb->param();
                        _queueList.emplace(qInfo);
                }

                if (!qInfo || qInfo->_matched)
                {
                        pb->set_error_type(E_IET_QueueStateError);
                        break;
                }

                auto reqActor = GetRegionMgrBase()->DistReqActor(qInfo->GetID());
                reqAgent->BindActor(reqActor);
                reqAgent->BindExtra(qInfo);
                if (!ses->AddAgent(reqAgent))
                {
                        pb->set_error_type(E_IET_AddAgentError);
                        break;
                }

                auto p = std::make_shared<stPlayerInfo>();
                p->_agent = reqAgent;
                p->_lobbySID = pb->lobby_sid();
                p->_gateSID = pb->gate_sid();
                p->_isMain = 0 == queueBaseInfo.queue_guid();

                p->UnPackFromLobby(pb->player_list(0));
                qInfo->_playerList.emplace(reqAgent->GetID(), p);
                qInfo->PackInfo(*pb);

                pb->clear_player_list();
                pb->set_error_type(E_IET_Success);
                qInfo->PackPlayerList(*pb->mutable_player_list());
        } while (0);

        if (ses)
        {
                ses->SendPB(pb, E_MCMT_QueueCommon, E_MCQCST_ReqQueue,
                            GameMgrLobbySession::MsgHeaderType::E_RMT_CallRet, msg->_msgHead._guid, msg->_msgHead._to, msg->_msgHead._from);
        }
        if (qInfo && E_IET_Success == pb->error_type())
                qInfo->Sync2Lobby();

        return;
}

EInternalErrorType ManualQueueMgrActor::ExitQueue(const std::shared_ptr<MsgExitQueue>& msg, bool self)
{
        if (!msg)
                return E_IET_ParamError;

        DLOG_INFO("收到玩家[{}] 请求退出队列!!!", msg->player_guid());
        auto queueBaseInfo = msg->base_info();
        auto& seq = _queueList.get<by_guid>();
        auto it = seq.find(queueBaseInfo.queue_guid());
        if (seq.end() == it)
                return E_IET_QueueNotFound;

        auto qInfo = *it;
        if (!qInfo || qInfo->_matched || !qInfo->Equal(queueBaseInfo))
                return E_IET_QueueStateError;

        auto& playerList = qInfo->_playerList;
        auto it_ = playerList.find(msg->player_guid());
        if (playerList.end() != it_)
        {
                auto p = it_->second;
                if (!self)
                        p->_agent->SendPush(p->_agent->GetBindActor(), E_MIMT_QueueCommon,
                                            E_MIQCST_ExitQueue, nullptr);

                playerList.erase(it_);
                if (p->_isMain && !playerList.empty())
                        playerList.begin()->second->_isMain = true;
                qInfo->Sync2Lobby();

                if (playerList.empty())
                        seq.erase(it);
        }
        msg->set_guid(qInfo->_guid);
        return E_IET_Success;
}

stQueueInfoPtr ManualQueueMgrActor::QueueOpt(const std::shared_ptr<MsgQueueOpt>& msg)
{
        auto queueBaseInfo = msg->base_info();
        auto& seq = _queueList.get<by_guid>();
        auto it = seq.find(queueBaseInfo.queue_guid());
        if (seq.end() != it)
        {
                auto ret = *it;
                if (ret->Equal(msg->base_info()))
                {
                        switch (msg->opt_type())
                        {
                        case E_QOT_SetMap :
                                ret->_param = msg->param();
                                break;
                        case E_QOT_SetHero :
                                            {
                                                    auto it = ret->_playerList.find(msg->player_guid());
                                                    if (ret->_playerList.end() != it && it->second->_attrList.size() >= 4)
                                                            it->second->_attrList[3] = msg->param();
                                            }
                                            break;
                        case E_QOT_Offline :
                                            {
                                                    auto it = ret->_playerList.find(msg->player_guid());
                                                    if (ret->_playerList.end() != it && it->second->_attrList.size() >= 5)
                                                            it->second->_attrList[4] = msg->param();
                                            }
                                            break;
                        case E_QOT_Kickout :
                                            {
                                                    auto exitInfo = std::make_shared<MsgExitQueue>();
                                                    ret->PackBaseInfo(*exitInfo->mutable_base_info());
                                                    exitInfo->set_player_guid(msg->param());
                                                    ExitQueue(exitInfo, false);
                                                    return ret; // ExitQueue 里面已经 Sync2Lobby。
                                            }
                                            break;
                        case E_QOT_Start :
                                            ret->_matched = true;
                                            for (auto& val : ret->_playerList)
                                                    val.second->_camp = val.second->GetID();
                                            return ret; // start 不再同步。
                                            break;
                        case E_QOT_Ready :
                                            {
                                                    auto it = ret->_playerList.find(msg->player_guid());
                                                    if (ret->_playerList.end() != it && !it->second->_attrList.empty())
                                                            it->second->_attrList[0] = msg->param();
                                            }
                                            break;
                        default :
                                            LOG_WARN("玩家[{}] 请求 QueueOpt 时，操作类型错误 optType[{}]",
                                                     msg->player_guid(), msg->opt_type());
                                            break;
                        }
                        ret->Sync2Lobby();
                        return ret;
                }
        }

        return nullptr;
}

bool CompetitionKnockoutQueueMgrActor::Init()
{
        if (!SuperType::Init())
                return false;

        InitSyncQueueInfoTimer();
        return true;
}

void CompetitionKnockoutQueueMgrActor::InitSyncQueueInfoTimer()
{
        std::weak_ptr<CompetitionKnockoutQueueMgrActor> weakThis = shared_from_this();
        _syncQueueInfoTimer.Start(weakThis, 5.0, [weakThis]() {
                auto thisPtr = weakThis.lock();
                if (thisPtr)
                {
                        auto sendMsg = std::make_shared<MsgMatchQueueInfo>();
                        sendMsg->set_queue_type(thisPtr->GetQueueType());
                        for (auto& info : thisPtr->_queueList.get<by_guid>())
                        {
                                auto msgInfo = sendMsg->add_info_list();
                                msgInfo->set_cfg_id(info->_param_1);
                                msgInfo->set_player_cnt(info->_playerList.size());
                        }

                        GetRegionMgrBase()->_lobbySesList.Foreach([&sendMsg](auto& weakSes) {
                                auto ses = weakSes.lock();
                                if (ses)
                                        ses->SendPB(sendMsg, E_MCMT_MatchCommon, E_MCMCST_ReqQueueInfo,
                                                    GameMgrLobbySession::MsgHeaderType::E_RMT_Send,
                                                    0, 0, 0);
                        });

                        thisPtr->InitSyncQueueInfoTimer();
                }
        });
}

void CompetitionKnockoutQueueMgrActor::ReqQueue(const std::shared_ptr<stMailReqQueue>& msg)
{
        std::shared_ptr<GameMgrLobbySession> ses;
        auto pb = std::dynamic_pointer_cast<MailReqQueue>(msg->_msg);
        auto queueBaseInfo = pb->base_info();
        pb->set_error_type(E_IET_Success);
        DLOG_INFO("收到玩家[{}] 请求排队消息!!!", pb->player_guid());
        auto reqAgent = msg->_agent;
        stQueueInfoPtr qInfo;
        do
        {
                if (!reqAgent)
                {
                        pb->set_error_type(E_IET_ReqAgentIsNull);
                        break;
                }

                ses = reqAgent->GetSession();
                if (!ses)
                {
                        pb->set_error_type(E_IET_LobbySesAlreadyRelease);
                        break;
                }

                if (pb->player_list_size() <= 0)
                {
                        pb->set_error_type(E_IET_ParamError);
                        break;
                }

                auto competitionCfg = GetRegionMgrBase()->GetCompetitionCfg(pb->param_1());
                if (!competitionCfg)
                {
                        pb->set_error_type(E_IET_ParamError);
                        break;
                }

                auto now = GetClock().GetTimeStamp();
                time_t baseTime = GetClock().TimeClear_Slow(now, Clock::E_CTT_DAY);
                if (2 == competitionCfg->_openTimeType)
                        baseTime = (baseTime - DAY_TO_SEC(GetClock().GetWeekDay())) + DAY_TO_SEC(competitionCfg->_openTimeTypeParam);

                auto timeInfo = competitionCfg->_timeInfoList.Get(competitionCfg->_timeInfoList.Size()<=1 ? 0 : pb->param_2());
                /*
                if (timeInfo)
                {
                        LOG_INFO("idx[{}] now[{}] baseTime[{}] openTime[{}] ocfg[{}] endTime[{}] ecfg[{}]"
                                 , competitionCfg->_timeInfoList.Size()<=1 ? 0 : pb->param_2()
                                 , GetClock().GetTimeString_Slow(now)
                                 , GetClock().GetTimeString_Slow(baseTime)
                                 , GetClock().GetTimeString_Slow(baseTime + timeInfo->_openTime)
                                 , timeInfo->_openTime
                                 , GetClock().GetTimeString_Slow(baseTime + timeInfo->_endTime)
                                 , timeInfo->_endTime);
                }
                */

                if (!timeInfo || now<baseTime+timeInfo->_openTime || baseTime+timeInfo->_endTime<now)
                {
                        if (timeInfo)
                        {
                                LOG_INFO("now[{}] openTime[{}] endTime[{}]"
                                         , GetClock().GetTimeString_Slow(now)
                                         , GetClock().GetTimeString_Slow(baseTime + timeInfo->_openTime)
                                         , GetClock().GetTimeString_Slow(baseTime + timeInfo->_endTime));
                        }
                        else
                        {
                                LOG_INFO("now[{}] timeInfo is nullptr", now);
                        }
                        pb->set_error_type(E_IET_ParamError);
                        break;
                }

                // 一个比赛配置同一时间只有一个匹配队列。
                const auto queueGuid = pb->param_1();
                auto& seq = _queueList.get<by_guid>();
                auto it = seq.find(queueGuid);
                if (seq.end() != it)
                {
                        qInfo = *it;
                }
                else
                {
                        qInfo = std::make_shared<stQueueInfo>(queueGuid, queueBaseInfo.region_type(), queueBaseInfo.queue_type());
                        qInfo->_rank = pb->player_list(0).rank();
                        qInfo->_time = GetClock().GetTimeStamp();
                        qInfo->_guid = LogService::GetInstance()->GenGuid();
                        qInfo->_param = pb->param();
                        qInfo->_param_1 = pb->param_1();
                        _queueList.emplace(qInfo);

                        std::weak_ptr<CompetitionKnockoutQueueMgrActor> weakThis = shared_from_this();
                        auto startTime = timeInfo->_openTime + competitionCfg->_interval * ((now - timeInfo->_openTime) / competitionCfg->_interval + 1);

                        LOG_INFO("startTime now[{}] startTime[{}]"
                                 , GetClock().GetTimeString_Slow(now)
                                 , GetClock().GetTimeString_Slow(startTime));

                        std::weak_ptr<stQueueInfo> weakQueue = qInfo;
                        qInfo->_checkTimer.Start(weakThis, startTime - now, [weakThis, weakQueue]() {
                                auto thisPtr = weakThis.lock();
                                auto qInfo = weakQueue.lock();
                                if (thisPtr && qInfo)
                                        thisPtr->StartCompetition(qInfo);
                        });
                }

                auto reqActor = GetRegionMgrBase()->DistReqActor(qInfo->GetID());
                reqAgent->BindActor(reqActor);
                reqAgent->BindExtra(qInfo);
                if (!ses->AddAgent(reqAgent))
                {
                        pb->set_error_type(E_IET_AddAgentError);
                        break;
                }

                auto p = std::make_shared<stPlayerInfo>();
                p->_agent = reqAgent;
                p->_lobbySID = pb->lobby_sid();
                p->_gateSID = pb->gate_sid();

                p->UnPackFromLobby(pb->player_list(0));
                qInfo->_playerList.emplace(reqAgent->GetID(), p);
                qInfo->PackInfo(*pb);

                pb->clear_player_list();
                pb->set_error_type(E_IET_Success);
                pb->set_param_2(qInfo->_playerList.size());
        } while (0);

        if (qInfo)
        {
                if (E_IET_Success == pb->error_type())
                        qInfo->Sync2Lobby();

                pb->set_guid(qInfo->_guid);
                LogService::GetInstance()->Log<E_LSLMT_Content>(pb->player_guid(), "", E_LSLST_Queue, qInfo->_queueType, E_LSOT_ReqQueue, qInfo->_guid);
        }

        if (ses)
        {
                ses->SendPB(pb, E_MCMT_QueueCommon, E_MCQCST_ReqQueue,
                            GameMgrLobbySession::MsgHeaderType::E_RMT_CallRet, msg->_msgHead._guid, msg->_msgHead._to, msg->_msgHead._from);
        }

        return;
}

EInternalErrorType CompetitionKnockoutQueueMgrActor::ExitQueue(const std::shared_ptr<MsgExitQueue>& msg, bool self)
{
        if (!msg)
                return E_IET_ParamError;

        auto& seq = _queueList.get<by_guid>();
        auto it = seq.find(msg->base_info().queue_guid());
        if (seq.end() == it)
                return E_IET_QueueNotFound;

        auto qInfo = *it;
        if (!qInfo || qInfo->_matched || !qInfo->Equal(msg->base_info()))
                return E_IET_QueueStateError;

        auto& playerList = qInfo->_playerList;
        playerList.erase(msg->player_guid());
        if (playerList.empty())
                seq.erase(it);

        msg->set_guid(qInfo->_guid);
        LogService::GetInstance()->Log<E_LSLMT_Content>(msg->player_guid(), "", E_LSLST_Queue, qInfo->_queueType, E_LSOT_ExitQueue, qInfo->_guid);
        return E_IET_Success;
}

void CompetitionKnockoutQueueMgrActor::StartCompetition(const stQueueInfoPtr& qInfo)
{
        _queueList.get<by_guid>().erase(qInfo->GetID());
        if (qInfo->_playerList.empty())
                return;

        auto competitionCfg = GetRegionMgrBase()->GetCompetitionCfg(qInfo->GetID());
        if (!competitionCfg)
                return;

        auto regionCfg = GetRegionMgrBase()->GetRegionCfg(competitionCfg->_regionType);
        if (!regionCfg)
                return;

        const int64_t competitionCnt = std::ceil((double)qInfo->_playerList.size() / competitionCfg->_maxFighterCnt);
        std::vector<stPlayerInfoPtr> playerList;
        playerList.reserve(competitionCnt * competitionCfg->_maxFighterCnt);
        for (auto& val : qInfo->_playerList)
                playerList.emplace_back(val.second);

        qInfo->_playerList.clear();
        qInfo->_robotList.clear();

        std::vector<int64_t> mapList;
        mapList.reserve(competitionCfg->_mapIDSelectorList.size());
        for (auto& rs : competitionCfg->_mapIDSelectorList)
                mapList.emplace_back(rs->Get()._v);

        std::vector<uint64_t> competitionGuidList;
        competitionGuidList.reserve(competitionCnt);
        auto thisPtr = shared_from_this();
        const auto playerCntPer = playerList.size() / competitionCnt;
        int64_t moreIdx = playerList.size() % competitionCnt;
        for (int64_t i=0; i<competitionCnt; ++i)
        {
                auto regionMgr = std::make_shared<CompetitionKnockoutRegionMgrActor>(regionCfg, qInfo->_queueType);
                regionMgr->_parentID = qInfo->_guid;
                regionMgr->_guid = LogService::GetInstance()->GenGuid();
                regionMgr->_param = qInfo->_param;
                regionMgr->_param_1 = qInfo->_param_1;
                regionMgr->_mapList = mapList;
                regionMgr->_roundCnt = 0;
                regionMgr->_qInfo = qInfo;
                regionMgr->Start();
                competitionGuidList.emplace_back(regionMgr->_guid);

                const auto tmpPlayerCnt = playerCntPer + (moreIdx--<=0 ? 0 : 1);

                const auto robotCnt = competitionCfg->_maxFighterCnt - playerCntPer;
                std::vector<std::shared_ptr<MailSyncPlayerInfo2Region>> robotList;
                robotList.reserve(robotCnt + 10);
                if (robotCnt > 0)
                {
                        std::vector<std::pair<int64_t, int64_t>> cntList;
                        cntList.reserve(competitionCfg->_robotAILevelList.size());
                        int64_t tmpSum = 0;
                        for (auto& val : competitionCfg->_robotAILevelList)
                        {
                                const auto tmpCnt = robotCnt * (val.first / 10000.0);
                                cntList.emplace_back(tmpSum + tmpCnt, val.second);
                                tmpSum += tmpCnt;
                        }

                        // TODO: 只请求一次
                        auto robotRet = RobotService::GetInstance()->ReqRobot(thisPtr, robotCnt, 1);
                        if (robotRet)
                        {
                                MailReqEnterRegion::MsgReqFighterInfo reqInfo;
                                int64_t distIdx = 0;
                                int64_t idx = 0;
                                auto robotAIInfo = cntList[distIdx];
                                for (auto& info : robotRet->robot_list())
                                {
                                        if (idx++ > robotAIInfo.first)
                                                robotAIInfo = GetValueOrLast(cntList, ++distIdx);
                                        auto rInfo = std::make_shared<MailSyncPlayerInfo2Region>();
                                        rInfo->CopyFrom(info);
                                        rInfo->set_param_1(robotAIInfo.second);
                                        robotList.emplace_back(rInfo);
                                }
                        }
                }
                regionMgr->StartRound(Random(playerList, tmpPlayerCnt), std::move(robotList));
        }

        std::string idListStr;
        for (auto id : competitionGuidList)
                idListStr += fmt::format("{},", id);
        if (!idListStr.empty())
                idListStr.pop_back();
        std::string str = fmt::format("{}\"id_list\":[{}]{}", "{", idListStr, "}");
        LogService::GetInstance()->Log<E_LSLMT_Content>(qInfo->_guid, str, E_LSLST_Competition, qInfo->_queueType, E_LSOT_CompetitionStart, qInfo->_guid);
}

// }}}

// vim: fenc=utf8:expandtab:ts=8
