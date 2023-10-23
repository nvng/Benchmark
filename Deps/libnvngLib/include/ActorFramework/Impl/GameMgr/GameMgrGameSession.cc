#include "GameMgrGameSession.h"

#include "RegionMgr.h"

std::string GameMgrGameSession::scPriorityTaskKey = "game_server_load";

GameMgrGameSession::~GameMgrGameSession()
{
        // LOG_INFO("GameMgrGameSession 析构!!!");
}

void GameMgrGameSession::InitCheckFinishTimer()
{
        SteadyTimer::StaticStart(1, []() {
                std::lock_guard l(RegionMgr::GetInstance()->_gameSesArrMutex);
                if (static_cast<int64_t>(RegionMgr::GetInstance()->_gameSesArr.size()) >= ServerListCfgMgr::GetInstance()->GetSize<stGameServerInfo>())
                {
                        GetApp()->_startPriorityTaskList->Finish(scPriorityTaskKey);
                }
                else
                {
                        GameMgrGameSession::InitCheckFinishTimer();
                }
        });
}

bool GameMgrGameSession::InitOnce()
{
        if (!SuperType::InitOnce())
                return false;

        InitCheckFinishTimer();
        return true;
}

void GameMgrGameSession::OnConnect()
{
	LOG_WARN("GameServer 连上来了!!!");
	SuperType::OnConnect();
}

void GameMgrGameSession::OnClose(int32_t reasonType)
{
	LOG_WARN("GameServer 断开连接!!!");
	SuperType::OnClose(reasonType);
        RegionMgr::GetInstance()->UnRegistGameSes(shared_from_this());
}

void GameMgrGameSession::MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
{
        SuperType::MsgHandleServerInit(ses, buf, bufRef);
        LOG_WARN("初始化 GameServer sid[{}] 成功!!!", ses->GetSID());

        auto thisPtr = std::dynamic_pointer_cast<GameMgrGameSession>(ses);
        RegionMgr::GetInstance()->RegistGameSes(thisPtr);
}

// vim: fenc=utf8:expandtab:ts=8
