#include "GMLobbySession.h"

void GMLobbySession::OnConnect()
{
	LOG_INFO("大厅连上来了!!!");
	SuperType::OnConnect();

        GetApp()->_lobbySesList.Add(GetID(), shared_from_this());
}

void GMLobbySession::OnClose(int32_t reasonType)
{
	LOG_INFO("大厅断开连接!!!");
	SuperType::OnClose(reasonType);

        GetApp()->_lobbySesList.Remove(GetID(), this);
}

void GMLobbySession::MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
{
        SuperType::MsgHandleServerInit(ses, buf, bufRef);
        LOG_INFO("初始化 LobbyServer sid[{}] 成功!!!", ses->GetSID());

        auto mail = std::make_shared<stMailSyncActivity>();
        mail->_ses = std::dynamic_pointer_cast<GMLobbySession>(ses);
        GetApp()->_activityActor->SendPush(0xe, mail);
}

// vim: fenc=utf8:expandtab:ts=8
