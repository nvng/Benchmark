#include "Player.h"

#include "PlayerMgr.h"

Player::Player(uint64_t guid)
	: ActorImpl(guid)
{
}

Player::~Player()
{
}

bool Player::Init()
{
	SuperType::Init();

        // ChangeStateMgr<PlayerGameStateMgr>(E_PGST_Game);
	return true;
}

uint64_t Player::GenPlayerGuid()
{
	// TODO:后面根据规则生成
	// return RandInRange(10000000, 10000000*10-1);
        return 10 * 1000 * 1000 + RandInRange(0, 40000);
        // return 99999999;
}

void Player::UnPack(const MsgPlayerInfo& msg)
{
	_msgPlayerInfo.CopyFrom(msg);
        for (auto& info : _msgPlayerInfo.goods_list())
                _goodsList[info.id()] = info.num();
}

void Player::SendPB(uint16_t mainType, uint16_t subType, google::protobuf::MessageLite* pb/*=nullptr*/)
{
        if (_ses)
                _ses->SendPB(pb, mainType, subType);
}

bool Player::UseGMGoods()
{
        return true;
}

void Player::DealPlayerChange(const MsgPlayerChange& msg)
{
        if (msg.change_goods_list_size() > 0)
        {
                for (auto& info : msg.change_goods_list())
                        _goodsList[info.id()] = info.num();
        }
}

ACTOR_MAIL_HANDLE(Player, E_MCMT_Client, E_MCCST_Login, MsgClientLoginRet)
{
        UnPack(msg->player_info());
        return nullptr;
}

ACTOR_MAIL_HANDLE(Player, E_MCMT_Client, E_MCCST_Kickout, MsgClientKickout)
{
        return nullptr;
}

ACTOR_MAIL_HANDLE(Player, E_MCMT_Client, E_MCCST_DayChange)
{
	// LOG_INFO("玩家收到 day change 消息!!!");
        return nullptr;
}

ACTOR_MAIL_HANDLE(Player, E_MCMT_Client, E_MCCST_DataResetNoneZero)
{
	LOG_INFO("玩家收到 data reset 消息!!!");
        return nullptr;
}

ACTOR_MAIL_HANDLE(Player, E_MCMT_GameCommon, E_MCGCST_SwitchRegion, MsgSwitchRegion)
{
        // LOG_INFO("玩家[{}] 收到 SwitchRegion type[{}]", GetID(), msg->region_type());
        SendPB(E_MCMT_GameCommon, E_MCGCST_LoadFinish);
        SendPB(0x7f, 0, nullptr);

        /*
        StateEventInfo evt(E_MCGCST_SwitchRegion);
        evt._data = msg;
        OnEvent(evt);
        */

        return nullptr;
}

// vim: fenc=utf8:expandtab:ts=8
