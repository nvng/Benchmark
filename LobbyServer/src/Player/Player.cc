#include "Player.h"

#include "PlayerMgr.h"
#include "Region/RegionMgr.h"

Player::Player(uint64_t guid, const std::string& nickName, const std::string& icon)
	: SuperType(guid, nickName, icon)
{
        DLOG_INFO("Player::Player() 构造!!!");

        /*
        for (int64_t i=0; i<50; ++i)
                testList.emplace_back(new stTest());
        for (auto t : testList)
                t->Run();
                */
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

void InitTimer(const PlayerPtr& player)
{
        player->StartTimerWithRelativeTimeOnce(10.0, [player](TimerGuidType id) {
                auto msg = std::make_shared<MsgClientLogin>();
                PlayerMgr::GetInstance()->BroadCast(player, 0x7f, 0xf, msg);
                InitTimer(player);
                LOG_INFO("11111111111111111111111111111111111111111111111111111111111111111111111111111");
        });
}

void Player::Online()
{
        ++GetApp()->_cnt;
        DLOG_WARN("玩家[{}] online!!!", GetID());
        SuperType::Online();

        CheckDataReset(GlobalSetup_CH::GetInstance()->_dataResetNonZero);

        // Note: 第一个返回的消息必须是 E_MCMT_Client, E_MCST_Login
        auto msg = std::make_shared<MsgClientLoginRet>();
        Pack2Client(*(msg->mutable_player_info()));
        Send2Client(E_MCMT_ClientCommon, E_MCCCST_Login, msg);

        static bool _ = [this]() {
                // InitTimer(shared_from_this());
                return true;
        }();
        (void)_;
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

ACTOR_MAIL_HANDLE(Player, 0x7f, 0xf, MsgClientLogin)
{
        ++GetApp()->_cnt;
        return nullptr;
}

// vim: fenc=utf8:expandtab:ts=8
