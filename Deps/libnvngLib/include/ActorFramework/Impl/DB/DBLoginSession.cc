#include "DBLoginSession.h"

void DBLoginSession::OnConnect()
{
        LOG_INFO("登录服连接上来!!!");
	SuperType::OnConnect();
}

void DBLoginSession::OnClose(int32_t reasonType)
{
        LOG_INFO("登录服断开连接!!! reasonType:{}", reasonType);
	SuperType::OnClose(reasonType);
}

void DBLoginSession::MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
{
        SuperType::MsgHandleServerInit(ses, buf, bufRef);
        LOG_INFO("初始化登录服 sid[{}] 成功!!!", ses->GetSID());
}

// vim: fenc=utf8:expandtab:ts=8
