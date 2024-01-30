#include "GateGameSession.h"

#include "NetMgrImpl.h"
#include "Player.h"
#include "GateClientSession.h"

namespace nl::af::impl {

const std::string GateGameSession::scPriorityTaskKey = "gate_game_load";
bool GateGameSession::InitOnce()
{
        if (!SuperType::InitOnce())
                return false;

        SteadyTimer::StaticStart(std::chrono::seconds(1), []() {
                int64_t cnt = 0;
                NetMgrImpl::GetInstance()->ForeachGame([&cnt](const auto& ses) {
                        ++cnt;
                });

                if (cnt >= ServerListCfgMgr::GetInstance()->GetSize<stGameServerInfo>())
                {
                        GetAppBase()->_startPriorityTaskList->Finish(scPriorityTaskKey);
                }
        });

        return true;
}

void GateGameSession::OnConnect()
{
        LOG_WARN("game server 连接成功!!!");
	SuperType::OnConnect();
}

void GateGameSession::OnClose(int32_t reasonType)
{
        /*
         * 此处禁止做自动绑定，因玩家与 game server 绑定关系只由 lobby 确定，
         * 因此，此处如断开连接，直接向客户端发送 GameDisconnec，则客户端通过
         * lobby 进行重连接。再者，一般情况下，此处断开连接，则 lobby 对应 sid
         * game server 断开可能性极大，在 lobby 处已经做自动重连。
         * 到时若只需客户端处理，而不进行自动重连。需在 lobby 上更改。
         */

        LOG_WARN("game server 断开连接!!!");
        _playerList.Foreach([](const auto& weakPlayer) {
                auto p = weakPlayer.lock();
                if (p)
                        p->OnGameClose();
        });
	NetMgrImpl::GetInstance()->RemoveSession(E_GST_Game, shared_from_this());
	SuperType::OnClose(reasonType);
}

void GateGameSession::MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
{
        SuperType::MsgHandleServerInit(ses, buf, bufRef);
        NetMgrImpl::GetInstance()->AddSession(E_GST_Game, ses);
        LOG_WARN("初始化房间服 sid[{}] 成功!!!", ses->GetSID());
}

void GateGameSession::OnRecv(typename SuperType::BuffTypePtr::element_type* buf, const typename SuperType::BuffTypePtr& bufRef)
{
        ++GetApp()->_gameRecvCnt;
        auto msgHead = *reinterpret_cast<MsgHeaderType*>(buf);
        DLOG_INFO("GateGameSession recv mainType:{:#x} subType:{:#x} size:{} sesCnt:{}",
                  msgHead.MainType(), msgHead.SubType(), msgHead._size, NetMgrImpl::GetInstance()->GetSessionCnt());

        auto player = GetPlayer(msgHead._to);
        if (player)
        {
                player->Send2Client<MsgHeaderType>(bufRef, buf, msgHead._size,
                                                   msgHead.CompressType(), msgHead.MainType(), msgHead.SubType());
        }
        else
        {
                DLOG_WARN("网关收到 game server 消息 mt[{:#x}] st[{:#x}]，但玩家[{}] 没找到!!!",
                          msgHead.MainType(), msgHead.SubType(), msgHead._to);
        }
}

}; // end of namespace nl::af::impl

// vim: fenc=utf8:expandtab:ts=8
