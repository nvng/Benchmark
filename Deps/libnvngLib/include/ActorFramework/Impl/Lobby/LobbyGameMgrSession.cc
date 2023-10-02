#include "LobbyGameMgrSession.h"

#include "RegionMgrBase.h"

namespace nl::af::impl {

const std::string LobbyGameMgrSession::_sPriorityTaskKey = "connect_gamemgr";

LobbyGameMgrSession::~LobbyGameMgrSession()
{
        // LOG_INFO("LobbyGameMgrSession 析构!!!");
}

void LobbyGameMgrSession::OnConnect()
{
	LOG_WARN("房间管理服连接成功!!!");
	SuperType::OnConnect();
}

void LobbyGameMgrSession::OnClose(int32_t reasonType)
{
	LOG_WARN("房间管理服断开连接!!!");
        GetRegionMgrBase()->_gameMgrSes.reset();
        GetRegionMgrBase()->_regionRelationList.Clear();

	SuperType::OnClose(reasonType);
}

void LobbyGameMgrSession::MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
{
        SuperType::MsgHandleServerInit(ses, buf, bufRef);
        LOG_WARN("初始化房间管理服 sid[{}] 成功!!!", ses->GetSID());
        auto thisPtr = std::dynamic_pointer_cast<LobbyGameMgrSession>(ses);
        GetRegionMgrBase()->_gameMgrSes = thisPtr;
        GetAppBase()->_startPriorityTaskList->Finish(_sPriorityTaskKey);
}

}; // end of namespace nl::af::impl

// vim: fenc=utf8:expandtab:ts=8
