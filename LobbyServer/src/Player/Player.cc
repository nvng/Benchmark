#include "Player.h"

#include "PlayerMgr.h"
#include "Region/RegionMgr.h"

Player::Player(GUID_TYPE guid, const MsgClientLogin& msg)
	// : SuperType(guid, 1 << 15)
	: SuperType(guid)
{
        DLOG_INFO("Player::Player() 构造!!!");

        /*
        for (int64_t i=0; i<50; ++i)
                testList.emplace_back(new stTest());
        for (auto t : testList)
                t->Run();
                */

        SetAttr(E_PSAT_NickName, msg.nick_name());
        SetAttr(E_PSAT_Icon, msg.icon());
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

void Player::OnCreateAccount(const std::shared_ptr<MsgClientLogin>& msg)
{
        SuperType::OnCreateAccount(msg);

        /*
        for (int64_t i=0; i<1000; ++i)
                _bag.Add(RandInRange(1000*1000, 10 * 1000 * 1000), 9999, 128);
        */
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

ACTOR_MAIL_HANDLE(Player, 0x7f, 0x0, MsgClientLogin)
{
        ++GetApp()->_cnt;
        for (int64_t i=0; i<10; ++i)
                Send2Client(0x7f, 0xf, msg);
        Send2Client(0x7f, 0x0, msg);
        return nullptr;
}

ACTOR_MAIL_HANDLE(Player, 0x7f, 0x1, MsgClientLogin)
{
        ++GetApp()->_cnt;
        for (int64_t i=0; i<20; ++i)
                Send2Client(0x7f, 0xf, msg);
        Send2Client(0x7f, 0x1, msg);
        return nullptr;
}

ACTOR_MAIL_HANDLE(Player, 0x7f, 0x2, MsgClientLogin)
{
        ++GetApp()->_cnt;
        for (int64_t i=0; i<40; ++i)
                Send2Client(0x7f, 0xf, msg);
        Send2Client(0x7f, 0x2, msg);
        return nullptr;
}

ACTOR_MAIL_HANDLE(Player, 0x7f, 0x3, MsgClientLogin)
{
        ++GetApp()->_cnt;

        auto perSize = LobbyGateSession::SerializeAndCompressNeedSize(msg);
        stBufCache bufCache(160 * perSize * 2, sizeof(LobbyGateSession::MsgHeaderType), [this](const VoidPtr& bufRef, ISession::BuffType buf, std::size_t bufSize) {
                auto ses = _clientActor->GetSession();
                ses->Send(bufRef, buf, bufSize);
        });

        for (int64_t i=0; i<160; ++i)
        {
                auto sendBuf = bufCache.Prepare(perSize);
                auto [sendSize, _] = LobbyGateSession::Pack(sendBuf, msg, 0x7f, 0xf, LobbyGateSession::MsgHeaderType::E_RMT_Send, ActorCallGuidType(), GetID(), _clientActor->GetID());
                bufCache.Commit(sendSize);
        }

        auto sendBuf = bufCache.Prepare(perSize);
        auto[sendSize, _] = LobbyGateSession::Pack(sendBuf, msg, 0x7f, 0x3, LobbyGateSession::MsgHeaderType::E_RMT_Send, ActorCallGuidType(), GetID(), _clientActor->GetID());
        bufCache.Commit(sendSize);

        bufCache.Deal();
        return nullptr;
}

ACTOR_MAIL_HANDLE(Player, 0x7f, 0xc, MsgClientLogin)
{
        ++GetApp()->_cnt;
        Send2Client(0x7f, 0xc, msg);
        return nullptr;
}

ACTOR_MAIL_HANDLE(Player, 0x7f, 0xd)
{
        ++GetApp()->_cnt;
        Send2Client(0x7f, 0xd, nullptr);
        return nullptr;
}

ACTOR_MAIL_HANDLE(Player, 0x7f, 0xe, MsgClientLogin)
{
        ++GetApp()->_cnt;
        return nullptr;
}

ACTOR_MAIL_HANDLE(Player, 0x7f, 0xf)
{
        ++GetApp()->_cnt;
        return nullptr;
}

// vim: fenc=utf8:expandtab:ts=8
