#include "LoginGateSession.h"

void LoginGateSession::OnConnect()
{
	LOG_INFO("网关连上来了!!!");
	SuperType::OnConnect();
}

void LoginGateSession::OnClose(int32_t reasonType)
{
	LOG_INFO("网关断开连接!!!");
	SuperType::OnClose(reasonType);
}

void LoginGateSession::MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
{
        SuperType::MsgHandleServerInit(ses, buf, bufRef);
        LOG_INFO("初始化 GateServer sid[{}] 成功!!!", ses->GetSID());
}

// vim: fenc=utf8:expandtab:ts=8
