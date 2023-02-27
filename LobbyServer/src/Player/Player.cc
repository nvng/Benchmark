#include "Player.h"

#include "PlayerMgr.h"
#include "Region/RegionMgr.h"

Player::Player(uint64_t guid, const std::string& nickName)
	: SuperType(guid, nickName)
{
        DLOG_INFO("Player::Player() 构造!!!");
}

Player::~Player()
{
        DLOG_INFO("Player::~Player() 析构!!!");
}

bool Player::Init()
{
	if (!SuperType::Init())
                return false;

	return true;
}

void Player::Online()
{
        DLOG_WARN("玩家[{}] online!!!", GetID());
        SuperType::Online();

        CheckDataReset(GlobalSetup_CH::GetInstance()->_dataResetNonZero);

        // Note: 第一个返回的消息必须是 E_MCMT_Client, E_MCST_Login
        auto msg = std::make_shared<MsgClientLoginRet>();
        Pack2Client(*(msg->mutable_player_info()));
        Send2Client(E_MCMT_ClientCommon, E_MCCCST_Login, msg);
}

ACTOR_MAIL_HANDLE(Player, E_MCMT_Client, E_MCCST_GM, MsgLobbyGM)
{
        LOG_INFO("gm cmd:{}", msg->cmd());
        if (msg->cmd()[0] != '@')
                return nullptr;

        for (auto& str : Tokenizer(msg->cmd(), "@"))
        {
                LOG_INFO("cmd str:{}", str);
                auto tmpList = Tokenizer(str, " ");
                if (tmpList.empty())
                        continue;

                LOG_INFO("cmd tmpList[0]:{}", tmpList[0]);
        }

        return nullptr;
}

// vim: fenc=utf8:expandtab:ts=8
