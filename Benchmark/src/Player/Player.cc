#include "Player.h"

#include "PlayerMgr.h"
#include "msg_client.pb.h"

Player::Player(uint64_t guid)
	: ActorImpl(guid, 128)
{
}

Player::~Player()
{
}

bool Player::Init()
{
	SuperType::Init();


        _stateMgr.Init();
        StateEventInfo evt(ActorMail::MsgTypeMerge(E_MCMT_Internal, E_MCIST_HeartBeat));
        _stateMgr.SetCurState(E_PST_None, shared_from_this(), evt, true);
	return true;
}

uint64_t Player::GenPlayerGuid()
{
	// TODO:后面根据规则生成
	return RandInRange(10000000, 10000000*10-1);
        // return 10 * 1000 * 1000 + RandInRange(0, 40000);
        // return 99999999;
}

void Player::UnPack(const MsgPlayerInfo& msg)
{
	_msgPlayerInfo.CopyFrom(msg);
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
}

void Player::OnDisconnect()
{
        // OnEvent(E_MCMT_ClientCommon, E_MCCCST_Disconnect, nullptr);
}

ACTOR_MAIL_HANDLE(Player, E_MCMT_ClientCommon, E_MCCCST_Login, MsgClientLoginRet)
{
        UnPack(msg->player_info());
        return nullptr;
}

ACTOR_MAIL_HANDLE(Player, E_MCMT_ClientCommon, E_MCCCST_Kickout, MsgClientKickout)
{
        return nullptr;
}

ACTOR_MAIL_HANDLE(Player, E_MCMT_ClientCommon, E_MCCCST_DayChange)
{
	// LOG_INFO("玩家收到 day change 消息!!!");
        return nullptr;
}

ACTOR_MAIL_HANDLE(Player, E_MCMT_ClientCommon, E_MCCCST_DataResetNoneZero)
{
	// LOG_INFO("玩家收到 data reset 消息!!!");
        return nullptr;
}

ACTOR_MAIL_HANDLE(Player, E_MCMT_GameCommon, E_MCGCST_SwitchRegion, MsgSwitchRegion)
{
        // LOG_INFO("玩家[{}] 收到 SwitchRegion type[{}]", GetID(), msg->region_type());
        OnEvent(E_MCMT_GameCommon, E_MCGCST_SwitchRegion, msg);

        // 必须要切完再发，LoadFinish 设计意义在于等待客户端加载完成。
        MsgLoadFinish sendMsg;
        sendMsg.set_region_type(msg->region_type());
        SendPB(E_MCMT_GameCommon, E_MCGCST_LoadFinish, &sendMsg);

        return nullptr;
}

// vim: fenc=utf8:expandtab:ts=8
