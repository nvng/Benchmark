#include "LobbyGateSession.h"

#include "Player/PlayerMgr.h"

LobbyGateSession::LobbyGateSession()
{
        LOG_INFO("LobbyGateSession 构造!!!");
}

LobbyGateSession::~LobbyGateSession()
{
        LOG_INFO("LobbyGateSession 析构!!!");
}

void LobbyGateSession::OnConnect()
{
	LOG_INFO("网关连上来了!!!");
	SuperType::OnConnect();

        GetApp()->_gateSesList.Add(GetGuid(), shared_from_this());
}

void LobbyGateSession::OnClose(int32_t reasonType)
{
	LOG_INFO("网关断开连接!!!");
	SuperType::OnClose(reasonType);

        GetApp()->_gateSesList.Remove(GetGuid());
}

// vim: fenc=utf8:expandtab:ts=8
