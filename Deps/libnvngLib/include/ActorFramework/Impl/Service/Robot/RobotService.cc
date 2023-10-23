#include "RobotService.h"

#if defined (ROBOT_SERVICE_SERVER) || defined (ROBOT_SERVICE_LOCAL)

template <>
bool RobotService::ReadNameCfg()
{
        auto fileName = ServerCfgMgr::GetInstance()->GetConfigAbsolute("AiName.txt");
        std::stringstream ss;
        if (!ReadFileToSS(ss, fileName))
                return false;

        _robotFirstNameList.clear();
        _robotSecondNameList.clear();
        int64_t idx = 0;
        std::string tmpStr;
        while (ReadTo(ss, "#"))
        {
                if (++idx <= 3)
                        continue;

                ss >> tmpStr
                        >> tmpStr;
                _robotFirstNameList.emplace_back(tmpStr);
                ss >> tmpStr;
                _robotSecondNameList.emplace_back(tmpStr);

                LOG_FATAL_IF(!CheckConfigEnd(ss, fileName),
                             "文件[{}] 读取错位，idx[{}] 没检测到结束符 <end> !!!",
                             fileName, idx);
        }

        LOG_FATAL_IF(_robotFirstNameList.empty() || _robotSecondNameList.empty(), "");
        return true;
}

template <>
bool RobotService::ReadAttrCfg()
{
        auto fileName = ServerCfgMgr::GetInstance()->GetConfigAbsolute("AiPlayer.txt");
        std::stringstream ss;
        if (!ReadFileToSS(ss, fileName))
                return false;

        _robotAttrRandomList.clear();
        _robotAttrList.Clear();
        int64_t idx = 0;
        std::string tmpStr;
        while (ReadTo(ss, "#"))
        {
                if (++idx <= 3)
                        continue;

                auto cfg = std::make_shared<stRobotAttrCfg>();
                ss >> cfg->_id
                        >> tmpStr
                        >> cfg->_iconList
                        >> cfg->_heroList
                        >> tmpStr;

                LOG_FATAL_IF(cfg->_iconList.empty() || cfg->_heroList.empty(), "");
                _robotAttrRandomList.emplace_back(cfg);

                LOG_FATAL_IF(!_robotAttrList.Add(cfg->_id, cfg), "");

                LOG_FATAL_IF(!CheckConfigEnd(ss, fileName),
                             "文件[{}] 读取错位，idx[{}] 没检测到结束符 <end> !!!",
                             fileName, idx);
        }
        LOG_FATAL_IF(_robotAttrRandomList.empty(), "");
        return true;
}

template <>
std::string RobotService::GenRandomName()
{
        // std::string ret;
        // ret.reserve(16);
        // ret += _robotFirstNameList[RandInRange(0, _robotFirstNameList.size())];
        // ret += _robotSecondNameList[RandInRange(0, _robotSecondNameList.size())];
        // return Base64Encode(ret);
        return Base64Encode(_robotFirstNameList[RandInRange(0, _robotFirstNameList.size())]);
}

template <>
RobotPtr RobotService::CreateRobot()
{
        RobotPtr ret;

        auto cfg = _robotAttrRandomList[RandInRange(0, _robotAttrRandomList.size())];
        if (!cfg)
                return ret;

        ret = std::make_shared<Robot>();
        ret->_cfgID = cfg->_id;
        ret->_id = RandInRange(1000 * 1000 * 10, 1000 * 1000 * 20);
        ret->_lv = 1;
        ret->_nickName = GenRandomName();
        std::string icon = fmt::format("{}", cfg->_iconList[RandInRange(0, cfg->_iconList.size())]);
        ret->_icon = Base64Encode(icon);
        // LOG_INFO("777777777777 reticon:{} icon:{} len:{} ret:{}", ret->_icon, icon, ret->_icon.length(), icon.length());
        return ret;
}

template <>
int64_t RobotService::GenRandomHero(int64_t cfgID)
{
        auto cfg = _robotAttrList.Get(cfgID);
        if (!cfg)
                return 0;

        return cfg->_heroList[RandInRange(0, cfg->_heroList.size())];
}

template <>
RobotPtr RobotService::DistRobot()
{
        std::lock_guard l(_robotDistListMutex);
        if (!_robotDistList.empty())
        {
                const auto idx = RandInRange(0, _robotDistList.size());
                auto ret = _robotDistList[idx];
                _robotDistList[idx].swap(_robotDistList[_robotDistList.size()-1]);
                _robotDistList.pop_back();
                return ret;
        }
        else
        {
                return CreateRobot();
        }
}

template <>
RobotPtr RobotService::GetRobot(int64_t id)
{
        std::lock_guard l(_robotDistListMutex);
        return _robotList.Get(id);
}

template <>
void RobotService::BackRobot(const RobotPtr& r)
{
        std::lock_guard l(_robotDistListMutex);
        _robotDistList.emplace_back(r);
}

void RobotActor::InitSaveTimer()
{
        RobotActorWeakPtr weakAct = shared_from_this();
        SteadyTimer::StaticStart(5 * 60, [weakAct]() {
                auto act = weakAct.lock();
                if (!act)
                        return;

                DBRobotList dbInfo;
                RobotService::GetInstance()->_robotList.Foreach([&dbInfo](const auto& r) {
                        r->Pack(*dbInfo.add_robot_list());
                });
                auto [bufRef, bufSize] = Compress::SerializeAndCompress<Compress::E_CT_Zstd>(dbInfo);
                act->RedisCmd("SET", "robot", std::string_view{ bufRef.get(), bufSize });
                act->InitSaveTimer();
        });
}

bool RobotActor::Init()
{
        if (!SuperType::Init())
                return false;

        std::lock_guard l(RobotService::GetInstance()->_robotDistListMutex);
        RobotService::GetInstance()->_robotList.Clear();
        RobotService::GetInstance()->_robotDistList.clear();
        auto loadRet = RedisCmd("GET", "robot");
        if (loadRet && !loadRet->IsNil())
        {
                auto [data, err] = loadRet->GetStr();
                if (!err)
                {
                        DBRobotList dbInfo;
                        Compress::UnCompressAndParseAlloc<Compress::E_CT_Zstd>(dbInfo, data);
                        for (auto& info : dbInfo.robot_list())
                        {
                                auto robot = std::make_shared<Robot>();
                                robot->UnPack(info);
                                RobotService::GetInstance()->_robotList.Add(robot->_id, robot);
                                RobotService::GetInstance()->_robotDistList.emplace_back(robot);
                        }
                }

        }
        else
        {
                for (int64_t i=0;  i<1000; ++i)
                {
                        auto robot = RobotService::GetInstance()->CreateRobot();
                        RobotService::GetInstance()->_robotList.Add(robot->_id, robot);
                        RobotService::GetInstance()->_robotDistList.emplace_back(robot);
                }
        }

        InitSaveTimer();

        return true;
}

#endif

#ifdef ROBOT_SERVICE_SERVER

SERVICE_NET_HANDLE(RobotService::SessionType, E_MIMT_Robot, E_MIRST_Req, MailReqRobot)
{
        do
        {
                auto cfg = GameSeasonMgr::GetInstance()->GetSegmentLevelInfoByLV(std::min<int64_t>(msg->rank(), 32));
                if (!cfg || cfg->_aiList.empty())
                {
                        msg->set_error_type(E_IET_Fail);
                        break;
                }

                for (int64_t i=0; i<msg->cnt(); ++i)
                {
                        auto r = RobotService::GetInstance()->DistRobot();
                        if (r)
                        {
                                auto info = msg->add_robot_list();
                                r->Pack2Game(*info);

                                Jump::MailSyncPlayerInfo2RegionExtra data;
                                data.set_hero_id(RobotService::GetInstance()->GenRandomHero(r->_cfgID));
                                data.set_ai_id(cfg->_aiList[i % cfg->_aiList.size()]);
                                info->mutable_extra()->PackFrom(data);
                        }
                }

                msg->set_error_type(E_IET_Success);
        } while (0);

        SendPB(msg, E_MIMT_Robot, E_MIRST_Req, MsgHeaderType::E_RMT_CallRet, msgHead._guid, msgHead._to, msgHead._from);
}

SERVICE_NET_HANDLE(RobotService::SessionType, E_MIMT_Robot, E_MIRST_Back, MailBackRobot)
{
        for (auto id : msg->robot_list())
        {
                auto robot = RobotService::GetInstance()->GetRobot(id);
                if (robot)
                        RobotService::GetInstance()->BackRobot(robot);
        }

        msg->set_error_type(E_IET_Success);
        SendPB(msg, E_MIMT_Robot, E_MIRST_Back, MsgHeaderType::E_RMT_CallRet, msgHead._guid, msgHead._to, msgHead._from);
}

#endif

// vim: fenc=utf8:expandtab:ts=8:noma
