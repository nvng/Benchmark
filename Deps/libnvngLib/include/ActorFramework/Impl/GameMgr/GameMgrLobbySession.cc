#include "GameMgrLobbySession.h"

#include "RegionMgr.h"

std::string GameMgrLobbySession::scPriorityTaskKey = "game_mgr_lobby_load";

GameMgrLobbySession::~GameMgrLobbySession()
{
        // LOG_INFO("GameMgrLobbySession 析构!!!");
}

void GameMgrLobbySession::InitCheckFinishTimer()
{
        SteadyTimer::StaticStart(std::chrono::seconds(1), []() {
                if (static_cast<int64_t>(RegionMgr::GetInstance()->_lobbySesList.Size()) >= ServerListCfgMgr::GetInstance()->GetSize<stLobbyServerInfo>())
                {
                        GetApp()->_startPriorityTaskList->Finish(scPriorityTaskKey);
                }
                else
                {
                        GameMgrLobbySession::InitCheckFinishTimer();
                }
        });
}

bool GameMgrLobbySession::InitOnce()
{
        if (!SuperType::InitOnce())
                return false;

        InitCheckFinishTimer();
        return true;
}

void GameMgrLobbySession::OnConnect()
{
	LOG_WARN("大厅连上来了!!!");
	SuperType::OnConnect();
}

void GameMgrLobbySession::OnClose(int32_t reasonType)
{
	LOG_WARN("大厅断开连接!!! reasonType:{}", reasonType);
	SuperType::OnClose(reasonType);
        RegionMgr::GetInstance()->_lobbySesList.Remove(GetSID(), this);
}

void GameMgrLobbySession::MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
{
        SuperType::MsgHandleServerInit(ses, buf, bufRef);
        LOG_WARN("初始化 GameMgrLobbySession sid[{}] 成功!!!", ses->GetSID());
        RegionMgr::GetInstance()->_lobbySesList.Add(ses->GetSID(), std::dynamic_pointer_cast<GameMgrLobbySession>(ses));
}

// vim: fenc=utf8:expandtab:ts=8
