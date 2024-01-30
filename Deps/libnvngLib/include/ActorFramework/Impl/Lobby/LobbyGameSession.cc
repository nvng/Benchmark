#include "LobbyGameSession.h"

#include "RegionMgrBase.h"

namespace nl::af::impl {

std::string LobbyGameSession::scPriorityTaskKey = "lobby_game_load";

LobbyGameSession::~LobbyGameSession()
{
        // LOG_INFO("LobbyGameSession 析构!!!");
}

void LobbyGameSession::InitCheckFinishTimer()
{
        SteadyTimer::StaticStart(std::chrono::seconds(1), []() {
                if (static_cast<int64_t>(GetRegionMgrBase()->_gameSesList.Size()) >= ServerListCfgMgr::GetInstance()->GetSize<stGameServerInfo>())
                {
                        GetApp()->_startPriorityTaskList->Finish(scPriorityTaskKey);
                }
                else
                {
                        LobbyGameSession::InitCheckFinishTimer();
                }
        });
}

bool LobbyGameSession::InitOnce()
{
        if (!SuperType::InitOnce())
                return false;

        InitCheckFinishTimer();
        return true;
}

void LobbyGameSession::OnConnect()
{
	LOG_WARN("房间服连接成功!!!");
	SuperType::OnConnect();
}

void LobbyGameSession::OnClose(int32_t reasonType)
{
	LOG_WARN("房间服断开连接!!!");
        GetRegionMgrBase()->_gameSesList.Remove(GetSID(), this);
	SuperType::OnClose(reasonType);
}

void LobbyGameSession::MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
{
        SuperType::MsgHandleServerInit(ses, buf, bufRef);
        LOG_WARN("初始化房间服 sid[{}] 成功!!!", ses->GetSID());
        GetRegionMgrBase()->_gameSesList.Add(ses->GetSID(), std::dynamic_pointer_cast<LobbyGameSession>(ses));
}

}; // end of namespace nl::af::impl

// vim: fenc=utf8:expandtab:ts=8
