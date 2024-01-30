#include "GameMgrSession.h"

#include "RegionMgr.h"

std::string GameMgrSession::scPriorityTaskKey = "game_mgr_load";

GameMgrSession::~GameMgrSession()
{
        // LOG_INFO("GameMgrSession 析构!!!");
}

void GameMgrSession::OnConnect()
{
	LOG_WARN("房间管理服连上来了!!!");
	SuperType::OnConnect();
}

void GameMgrSession::OnClose(int32_t reasonType)
{
	LOG_WARN("房间管理服断开连接!!!");
	SuperType::OnClose(reasonType);
}

void GameMgrSession::MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
{
        SuperType::MsgHandleServerInit(ses, buf, bufRef);
        LOG_WARN("初始化房间管理服 sid[{}] 成功!!!", ses->GetSID());

        GetApp()->_startPriorityTaskList->Finish(scPriorityTaskKey);
}

// vim: fenc=utf8:expandtab:ts=8
