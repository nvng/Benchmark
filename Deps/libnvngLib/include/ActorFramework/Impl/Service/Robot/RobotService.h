#pragma once

#if defined(ROBOT_SERVICE_SERVER) || defined (ROBOT_SERVICE_LOCAL)

#include "GameSeason.h"
#include "PlayerMgrBase.h"

struct stRobotAttrCfg
{
        int64_t _id = 0;
        std::vector<uint32_t> _iconList;
        std::vector<uint32_t> _heroList;
};
typedef std::shared_ptr<stRobotAttrCfg> stRobotAttrCfgPtr;

class Robot
{
public :
        uint32_t _cfgID = 0;
        uint32_t _id = 0;
        uint32_t _lv = 0;
        uint32_t _exp = 0;
        std::string _nickName;
        std::string _icon;

        PlayerGameSeasonInfo _gameSeasonInfo;

        void AddExp(int64_t exp)
        {
                if (exp <= 0)
                        return;

                _exp += exp;
                for (int i=0; i<1000; ++i)
                {
                        auto needExp = GetPlayerMgrBase()->GetExpByLV(_lv);
                        if (_exp >= needExp)
                                _lv += 1;
                        else
                                break;
                }
        }

        void Pack(auto& msg)
        {
                msg.set_cfg_id(_cfgID);
                msg.set_id(_id);
                msg.set_lv(_lv);
                msg.set_exp(_exp);
                msg.set_nick_name(_nickName);
                msg.set_icon(_icon);

                _gameSeasonInfo.Pack(*msg.mutable_game_season());
        }

        void UnPack(const auto& msg)
        {
                _cfgID = msg.cfg_id();
                _id = msg.id();
                _lv = msg.lv();
                _exp = msg.exp();
                _nickName = msg.nick_name();
                _icon = msg.icon();

                _gameSeasonInfo.UnPack(msg.game_season());
        }
        
        void Pack2Game(auto& msg)
        {
                msg.set_param(_cfgID);
                msg.set_player_guid(_id);
                msg.set_lv(_lv);
                msg.set_nick_name(_nickName);
                msg.set_icon(_icon);
        }
};
typedef std::shared_ptr<Robot> RobotPtr;

#endif

SPECIAL_ACTOR_DEFINE_BEGIN(RobotActor, E_MIMT_Robot);
#if defined(ROBOT_SERVICE_SERVER) || defined (ROBOT_SERVICE_LOCAL)
public :
        RobotActor() : SuperType(SpecialActorMgr::GenActorID(), IActor::scMailQueueMaxSize) { }
        bool Init() override;
        void InitSaveTimer();
#endif
SPECIAL_ACTOR_DEFINE_END(RobotActor);

// #define ROBOT_SERVICE_SERVER
// #define ROBOT_SERVICE_CLIENT
// #define ROBOT_SERVICE_LOCAL

#include "ActorFramework/ServiceExtra.hpp"

DECLARE_SERVICE_BASE_BEGIN(Robot, SessionDistributeMod, ServiceSession);

private :
        RobotServiceBase() : SuperType("RobotService") { }
        ~RobotServiceBase() override { }

public :
        bool Init() override
        {
                if (!SuperType::Init())
                        return false;

#if defined(ROBOT_SERVICE_SERVER) || defined (ROBOT_SERVICE_LOCAL)
                bool ret = ReadNameCfg()
                        && ReadAttrCfg();

                return ret;
#else
                return true;
#endif
        }

#if defined(ROBOT_SERVICE_SERVER) || defined (ROBOT_SERVICE_LOCAL)
        bool ReadNameCfg();
        std::string GenRandomName();
        bool ReadAttrCfg();
        RobotPtr CreateRobot();
        int64_t GenRandomHero(int64_t cfgID);
        RobotPtr DistRobot();
        RobotPtr GetRobot(int64_t id);
        void BackRobot(const RobotPtr& r);

        template <typename ... Args>
        bool StartLocal(int64_t actCnt, Args ... args)
        {
                actCnt = 1;
                LOG_FATAL_IF(!CHECK_2N(actCnt), "actCnt 必须设置为 2^N !!!", actCnt);
                for (int64_t i=0; i<actCnt; ++i)
                {
                        auto act = std::make_shared<typename SuperType::ActorType>(std::forward<Args>(args)...);
                        SuperType::_actorArr.emplace_back(act);
                        act->Start();
                }

                return true;
        }

private :
        friend class RobotActor;
        RobotActorPtr _robotActor;
        SpinLock _robotDistListMutex;
        Map<int64_t, RobotPtr> _robotList;
        std::vector<RobotPtr> _robotDistList;

private :
        std::vector<std::string> _robotFirstNameList;
        std::vector<std::string> _robotSecondNameList;
        std::vector<stRobotAttrCfgPtr> _robotAttrRandomList;
        Map<int64_t, stRobotAttrCfgPtr> _robotAttrList;
#endif

#ifdef ROBOT_SERVICE_CLIENT

        template <typename _Ty>
        std::shared_ptr<MailReqRobot> ReqRobot(const std::shared_ptr<_Ty>& act, int64_t cnt, int64_t rank)
        {
                auto robotActor = SuperType::GetActor(act, act->GetID());
                if (!robotActor)
                        return nullptr;

                auto mail = std::make_shared<MailReqRobot>();
                mail->set_cnt(cnt);
                mail->set_rank(rank);
                auto reqRet = Call(MailReqRobot, act, robotActor, E_MIMT_Robot, E_MIRST_Req, mail);
                if (!reqRet || E_IET_Success != reqRet->error_type())
                {
                        LOG_WARN("actor id[{}] 请求机器人时出错!!! error[{}]",
                                 act->GetID(), reqRet ? reqRet->error_type() : 0);
                        return nullptr;
                }

                return reqRet;
        }

        template <typename _Ty>
        bool BackRobot(const std::shared_ptr<_Ty>& act, const std::shared_ptr<MailBackRobot>& msg)
        {
                auto robotActor = SuperType::GetActor(act, act->GetID());
                if (!robotActor)
                        return false;

                auto reqRet = Call(MailBackRobot, act, robotActor, E_MIMT_Robot, E_MIRST_Back, msg);
                if (!reqRet || E_IET_Success != reqRet->error_type())
                {
                        LOG_WARN("");
                        return false;
                }

                return true;
        }

#endif

DECLARE_SERVICE_BASE_END(Robot);

#ifdef ROBOT_SERVICE_SERVER
typedef RobotServiceBase<E_ServiceType_Server, stLobbyServerInfo> RobotService;
#endif

#ifdef ROBOT_SERVICE_CLIENT
typedef RobotServiceBase<E_ServiceType_Client, stRobotMgrServerInfo> RobotService;
#endif

#ifdef ROBOT_SERVICE_LOCAL
typedef RobotServiceBase<E_ServiceType_Local, stServerInfoBase> RobotService;
#endif

// vim: fenc=utf8:expandtab:ts=8:noma
