#include "LobbyGMSession.h"

LobbyGMSession::~LobbyGMSession()
{
        // DLOG_INFO("LobbyGMSession 析构!!!");
}

void LobbyGMSession::OnConnect()
{
	LOG_INFO("GMServer 连上来了!!!");
	SuperType::OnConnect();
}

void LobbyGMSession::OnClose(int32_t reasonType)
{
	LOG_INFO("GMServer 断开连接!!!");
	SuperType::OnClose(reasonType);
}

void LobbyGMSession::MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
{
        SuperType::MsgHandleServerInit(ses, buf, bufRef);
        LOG_INFO("初始化 GMServer sid[{}] 成功!!!", ses->GetSID());
}

// vim: fenc=utf8:expandtab:ts=8
