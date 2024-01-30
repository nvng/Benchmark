#include "GameGateSession.h"

#include "NetMgrImpl.h"

GameGateSession::~GameGateSession()
{
        // LOG_INFO("GameGateSession 析构!!!");
}

void GameGateSession::OnConnect()
{
	LOG_WARN("网关连上来了!!!");
	SuperType::OnConnect();
}

void GameGateSession::OnClose(int32_t reasonType)
{
	LOG_WARN("网关断开连接!!!");
        NetMgrImpl::GetInstance()->_gateSesList.Remove(GetSID(), this);
	SuperType::OnClose(reasonType);
}

void GameGateSession::MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
{
        SuperType::MsgHandleServerInit(ses, buf, bufRef);
        LOG_WARN("初始化网关 sid[{}] 成功!!!", ses->GetSID());
        NetMgrImpl::GetInstance()->_gateSesList.Add(ses->GetSID(), std::dynamic_pointer_cast<GameGateSession>(ses));
}

// vim: fenc=utf8:expandtab:ts=8
