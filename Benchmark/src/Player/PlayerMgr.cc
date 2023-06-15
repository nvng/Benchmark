#include "PlayerMgr.h"

#include "Player.h"
#include "Net/NetProcMgr.h"
#include "Net/ClientGateSession.h"

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

        int64_t perCnt = 5000;
        ServerListCfgMgr::GetInstance()->Foreach<stGateServerInfo>([this, perCnt](const stGateServerInfoPtr& gateInfo) {
                for (int i=0; i<perCnt; ++i)
                {
#if 0
                        static int64_t idx = 0;
                        _idList.emplace_back(GetApp()->GetSID() * 1000 * 1000 + ++idx);
#else
                        static int64_t base = 10 * 1000 * 1000;
                        _idList.emplace_back(RandInRange(base, base + 40 * 1000)); // 不停地断线重连接以及异地登录。
#endif
                }
        });

        ServerListCfgMgr::GetInstance()->Foreach<stGateServerInfo>([this, perCnt](const stGateServerInfoPtr& gateInfo) {
                for (int i=0; i<perCnt; ++i)
                {
                        auto proc = NetProcMgr::GetInstance()->Dist(i);
                        proc->Connect(gateInfo->_ip, gateInfo->_client_port, false, []() { return CreateSession<ClientGateSession>(); });
                }
        });

        return true;
}

NET_MSG_HANDLE(ClientGateSession, E_MCMT_ClientCommon, E_MCCCST_Login, MsgClientLoginRet)
{
	// LOG_INFO("玩家登录成功返回!!!");
        
        const auto& playerInfo = msg->player_info();
        auto p = std::dynamic_pointer_cast<Player>(PlayerMgr::GetInstance()->GetActor(playerInfo.player_guid()));
        if (!p)
        {
                auto p = std::make_shared<Player>(playerInfo.player_guid());
                if (p->Init())
                {
                        p->SendPush(nullptr, E_MCMT_ClientCommon, E_MCCCST_Login, msg);
                        p->_ses = shared_from_this();
                        _player = p;
                        p->Start();
                }
        }
        else
        {
                p->SendPush(nullptr, E_MCMT_ClientCommon, E_MCCCST_Login, msg);
                p->_ses = shared_from_this();
                _player = p;
        }
}

// vim: fenc=utf8:expandtab:ts=8
