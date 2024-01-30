#include "LobbyGateSession.h"

namespace nl::af::impl {

std::string LobbyGateSession::scPriorityTaskKey = "lobby_gate_load";

void LobbyGateSession::OnConnect()
{
	SuperType::OnConnect();
        auto ep = _socket.remote_endpoint();
	LOG_WARN("网关连上来了!!! ip[{}] port[{}]", ep.address().to_string(), ep.port());

        GetApp()->_gateSesList.Add(GetID(), shared_from_this());
}

void LobbyGateSession::OnClose(int32_t reasonType)
{
	LOG_WARN("网关断开连接!!!");
	SuperType::OnClose(reasonType);

        GetApp()->_gateSesList.Remove(GetID(), this);
}

void LobbyGateSession::OnRecv(ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
{
        auto msgHead = *reinterpret_cast<MsgHeaderType*>(buf);
        switch (msgHead._type)
        {
                // 因涉及到统一管理，以及唯一性判断（即来自不同网关和来自相同网关的不同消息）,
                // E_MCCCST_Login 和 E_MCCCST_Disconnect 必须通过 PlayerMgrActor 进行处理。
        case MsgHeaderType::MsgTypeMerge<E_MCMT_ClientCommon, E_MCCCST_Login>() :
        case MsgHeaderType::MsgTypeMerge<E_MCMT_ClientCommon, E_MCCCST_Disconnect>() :
                SuperType::SuperType::OnRecv(buf, bufRef);
                break;
        default:
                SuperType::OnRecv(buf, bufRef);
                break;
        }
}

void LobbyGateSession::MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
{
        SuperType::MsgHandleServerInit(ses, buf, bufRef);
        LOG_INFO("初始化网关 sid[{}] 成功!!!", ses->GetSID());
}

};

// vim: fenc=utf8:expandtab:ts=8:noma
