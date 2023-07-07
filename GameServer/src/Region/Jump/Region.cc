#include "Region.h"

#include "RegionMgr.h"

namespace Jump {

REGION_GAME_COMMON_HANDLE(Region);

Region::Region(const std::shared_ptr<MailRegionCreateInfo>& cInfo,
               const std::shared_ptr<RegionCfg>& cfg,
               const GameMgrSession::ActorAgentTypePtr& agent)
        : SuperType(cInfo, cfg, agent)
{
}

Region::~Region()
{
}

bool Region::InitOnce()
{
        static bool ret = []() {
                Fighter::InitOnce();
                return true;
        }();
        return ret;
}

bool Region::Init()
{
        if (!SuperType::Init())
                return false;

        _regionStateMgr.Init();
        StateEventInfo evt(E_RSECT_None);
        _regionStateMgr.SetCurState(E_RST_None, shared_from_this(), evt, true);
        return true;
}

Region::FighterPtr Region::CreateFighter(const GameLobbySession::ActorAgentTypePtr& p,
                                         const GameGateSession::ActorAgentTypePtr& cli,
                                         const MailReqEnterRegion::MsgReqFighterInfo& info)
{
        return std::make_shared<PlayerFighter<Fighter>>(shared_from_this(), p, cli, info);
}

bool Region::CheckStart()
{
        // return GetFighterCnt() >= _cfg->player_limit_min();
        // return GetFighterCnt() >= GetAllFighterCnt();
        return GetFighterCnt() >= _cfg->player_limit_min();
}

void Region::Prepare()
{
        auto tmpList = _loadFighterList;
        for (auto& val : tmpList)
                RegisterFighter(val.second);
}

void Region::Play()
{
}

void Region::PlayEnd()
{
}

void Region::Conclude()
{
}

ACTOR_MAIL_HANDLE(Region, E_MCMT_Game, E_MCGST_Sync, MsgSync)
{
        // TimeCost t("E_MCGST_Sync");
        auto f = GetFighter(msg->player_guid());
        if (!f)
                return nullptr;

        switch (msg->state())
        {
        default :
                break;
        }

        Send2Client(E_MCMT_Game, E_MCGST_Sync, msg);
        return nullptr;
}

}; // end namespace Jump

// vim: fenc=utf8:expandtab:ts=8
