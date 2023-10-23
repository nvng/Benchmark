#include "GameLobbySession.h"

#include "NetMgrImpl.h"
#include "RegionMgr.h"

std::string GameLobbySession::scPriorityTaskKey = "game_lobby_load";

GameLobbySession::~GameLobbySession()
{
        // LOG_INFO("GameLobbySession 析构!!!");
}

void GameLobbySession::InitCheckFinishTimer()
{
        SteadyTimer::StaticStart(1, []() {
                if (static_cast<int64_t>(NetMgrImpl::GetInstance()->_lobbySesList.Size()) >= ServerListCfgMgr::GetInstance()->GetSize<stLobbyServerInfo>())
                {
                        GetApp()->_startPriorityTaskList->Finish(scPriorityTaskKey);
                }
                else
                {
                        GameLobbySession::InitCheckFinishTimer();
                }
        });
}

bool GameLobbySession::InitOnce()
{
        if (!SuperType::InitOnce())
                return false;

        InitCheckFinishTimer();
        return true;
}

void GameLobbySession::OnConnect()
{
	LOG_WARN("大厅连上来了!!!");
	SuperType::OnConnect();
}

void GameLobbySession::OnClose(int32_t reasonType)
{
	LOG_WARN("大厅断开连接!!!");
        NetMgrImpl::GetInstance()->_lobbySesList.Remove(GetSID(), this);
	SuperType::OnClose(reasonType);
}

void GameLobbySession::MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
{
        SuperType::MsgHandleServerInit(ses, buf, bufRef);
        LOG_WARN("初始化大厅 sid[{}] 成功!!!", ses->GetSID());
        NetMgrImpl::GetInstance()->_lobbySesList.Add(ses->GetSID(), std::dynamic_pointer_cast<GameLobbySession>(ses));
}

// vim: fenc=utf8:expandtab:ts=8
