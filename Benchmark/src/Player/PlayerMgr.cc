#include "PlayerMgr.h"

#include "Player.h"
#include "Net/NetProcMgr.h"
#include "Net/ClientGateSession.h"

PlayerMgr::PlayerMgr()
{
}

PlayerMgr::~PlayerMgr()
{
}

bool PlayerMgr::Init()
{
        if (!SuperType::Init())
                return false;

        int64_t perCnt = 1000;
        nl::af::impl::ServerListCfgMgr::GetInstance()->_gateServerList.Foreach([this, perCnt](const nl::af::impl::stGateServerInfoPtr& gateInfo) {
                for (int i=0; i<perCnt; ++i)
                {
                        static int64_t idx = 0;
                        _idList.emplace_back(GetApp()->GetSID() * 1000 * 1000 + ++idx);
                }
        });

        for (auto id : _idList)
                LOG_INFO("1111111111111 id:{}", id);

        nl::af::impl::ServerListCfgMgr::GetInstance()->_gateServerList.Foreach([this, perCnt](const nl::af::impl::stGateServerInfoPtr& gateInfo) {
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
        
	auto p = std::make_shared<Player>(msg->player_info().player_guid());
        if (p->Init())
        {
                p->_ses = shared_from_this();
                _player = p;
                p->SendPush(nullptr, E_MCMT_ClientCommon, E_MCCCST_Login, msg);
                p->Start();
        }
}

// vim: fenc=utf8:expandtab:ts=8
