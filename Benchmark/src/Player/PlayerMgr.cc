#include "PlayerMgr.h"

#include "Net/ISession.hpp"
#include "Player.h"
#include "Net/ClientGateSession.h"
#include "boost/fiber/fiber.hpp"

PlayerMgr::PlayerMgr()
        : SuperType("PlayerMgr")
{
}

PlayerMgr::~PlayerMgr()
{
}

bool PlayerMgr::Init()
{
        if (!SuperType::Init())
                return false;

        // int64_t perCnt = 5000;
        int64_t perCnt = 80000;
        ServerListCfgMgr::GetInstance()->Foreach<stGateServerInfo>([this, perCnt](const stGateServerInfoPtr& gateInfo) {
                for (int i=0; i<perCnt; ++i)
                {
#if 1
                        static int64_t idx = 0;
                        _idList.emplace_back(GetApp()->GetSID() * 1000 * 1000 + ++idx);
#else
                        static int64_t base = 10 * 1000 * 1000;
                        _idList.emplace_back(RandInRange(base, base + 40 * 1000)); // 不停地断线重连接以及异地登录。
#endif
                }
        });

        ServerListCfgMgr::GetInstance()->Foreach<stGateServerInfo>([perCnt](const stGateServerInfoPtr& gateInfo) {
                LOG_INFO("gggggggggate ip:{} port:{}", gateInfo->_ip, gateInfo->_client_port);
                for (int i=0; i<perCnt; ++i)
                {
                        NetMgrBase<ClientGateSession::Tag>::GetInstance()->Connect("127.0.0.1", gateInfo->_client_port, [](auto&& s) {
                                return std::make_shared<ClientGateSession>(std::move(s));
                        });
                }
        });

        return true;
}

NET_MSG_HANDLE(ClientGateSession, E_MCMT_ClientCommon, E_MCCCST_Login, MsgClientLoginRet)
{
	// DLOG_INFO("玩家登录成功返回!!!");
        const auto& playerInfo = msg->player_info();
        auto p = std::dynamic_pointer_cast<Player>(PlayerMgr::GetInstance()->GetActor(playerInfo.player_guid()));
        if (!p)
        {
                auto p = std::make_shared<Player>(playerInfo.player_guid());
                p->SendPush(nullptr, E_MCMT_ClientCommon, E_MCCCST_Login, msg);
                p->_ses = shared_from_this();
                _player = p;
                p->Start();

#if 0
                std::shared_ptr<MsgClientLogin> msg;
                // auto msg = std::make_shared<MsgClientLogin>();
                // msg->set_nick_name("abcdef123456abcdef123456abcdef123456abcdef123456abcdef123456abcdef123456abcdef123456abcdef123456");
                p->SendPB(0x7f, 0, msg);
                /*
                for (int64_t i=0; i<200; ++i)
                        p->SendPB(0x7f, 1, msg);
                        */
#endif
        }
        else
        {
                p->SendPush(nullptr, E_MCMT_ClientCommon, E_MCCCST_Login, msg);
                p->_ses = shared_from_this();
                _player = p;
        }
}

// vim: fenc=utf8:expandtab:ts=8
