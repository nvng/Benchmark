#include "DBLobbySession.h"

void DBLobbySession::OnConnect()
{
        LOG_INFO("大厅连接上来!!!");
	SuperType::OnConnect();
}

void DBLobbySession::OnClose(int32_t reasonType)
{
        LOG_INFO("大厅断开连接!!! reasonType:{}", reasonType);
	SuperType::OnClose(reasonType);
}

void DBLobbySession::MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
{
        SuperType::MsgHandleServerInit(ses, buf, bufRef);
        LOG_INFO("初始化大厅 sid[{}] 成功!!!", ses->GetSID());
}

// vim: fenc=utf8:expandtab:ts=8
