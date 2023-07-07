#include "Fighter.h"

#include "Region.h"

namespace Jump {

Fighter::Fighter(uint64_t id,
                 const RegionPtr& region,
                 const MailReqEnterRegion::MsgReqFighterInfo& info)
        : SuperType(id, region, info)
{
}

Fighter::~Fighter()
{
}

void Fighter::InitOnce()
{
}

bool Fighter::InitFromLobby(const SyncMessageType& playerInfo)
{
        auto region = GetRegion();
        if (!region)
                return false;

        // REGION_DLOG_WARN(GetRegionGuid(), "场景[{}] 收到玩家[{}] 大厅过来的初始化信息!!!", region->GetID(), GetID());
        if (!SuperType::InitFromLobby(playerInfo))
        {
                REGION_LOG_WARN(GetRegionGuid(), "玩家[{}] 向 WarsOfChessRegion 同步信息时，SuperType Init error!!!", GetID());
                return false;
        }

        MailSyncPlayerInfo2RegionExtra data;
        playerInfo.extra().UnpackTo(&data);
        _heroID = data.hero_id();
        _rankLV = data.rank_lv();
        _rankStar = data.rank_star();

        return true;
}

void Fighter::PackFighterInfoBrief(MsgFighterInfo& msg)
{
        SuperType::PackFighterInfoBrief(msg);
}

void Fighter::PackFighterInfo(MsgFighterInfo& msg)
{
        PackFighterInfoBrief(msg);
        msg.mutable_pos_info()->CopyFrom(_posInfo);
        msg.set_end_time(_endTime);
        msg.set_shield_end_time(_shieldEndTime);
        msg.set_speed_pos_cnt(_speedPosCnt);
        msg.set_camp(GetCamp());
        msg.set_hero_id(_heroID);
        msg.set_continue_center_cnt(_continueCenterCnt);
        msg.set_add_coins(_addCoins);
        for (auto id : _battleItemList)
                msg.add_battle_item_list(id);
}

void Fighter::PackConclude(const RegionPtr& region)
{
        PackFighterInfoBrief(*_concludeInfo.mutable_fighter_info());
        _concludeInfo.set_score(_posInfo.score());
        _concludeInfo.set_di_kang_gan_rao_cnt(_diKangGanRaoCnt);
        _concludeInfo.set_add_coins(_addCoins);

        if (0 != _endTime)
        {
                _concludeInfo.set_is_finish(true);
        }

        for (auto& val : _battleItemUseList)
        {
                auto item = _concludeInfo.add_battle_item_use_list();
                item->set_id(val.first);
                item->set_cnt(val.second);
        }

        for (auto& val : _battleItemHitList)
        {
                auto item = _concludeInfo.add_battle_item_hit_list();
                item->set_id(val.first);
                item->set_cnt(val.second);
        }
}

}; // end namespace Jump

// vim: fenc=utf8:expandtab:ts=8
