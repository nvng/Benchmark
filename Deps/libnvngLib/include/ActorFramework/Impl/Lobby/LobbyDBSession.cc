#include "LobbyDBSession.h"

#if 0

#include "DBMgr.h"

namespace nl::af::impl {

std::string LobbyDBSession::scPriorityTaskKey = "connect_db_server";

LobbyDBSession::~LobbyDBSession()
{
        // LOG_INFO("LobbyDBSession 析构!!!");
}

void LobbyDBSession::InitCheckFinishTimer()
{
        SteadyTimer::StaticStart(1, []() {
                int64_t cnt = 0;
                for (auto& ws : DBMgr::GetInstance()->_dbSesArr)
                {
                        auto s = ws.lock();
                        if (s)
                                ++cnt;
                }

                if (cnt >= ServerListCfgMgr::GetInstance()->GetSize<stDBServerInfo>())
                {
                        GetApp()->_startPriorityTaskList->Finish(scPriorityTaskKey);
                }
                else
                {
                        LobbyDBSession::InitCheckFinishTimer();
                }
        });
}

bool LobbyDBSession::InitOnce()
{
        if (!SuperType::InitOnce())
                return false;

        InitCheckFinishTimer();
        return true;
}

void LobbyDBSession::OnConnect()
{
	LOG_INFO("DBServer 连上来了!!!");
	SuperType::OnConnect();
}

void LobbyDBSession::OnClose(int32_t reasonType)
{
	LOG_INFO("DBServer 断开连接!!!");
        DBMgr::GetInstance()->RemoveSession(GetSID());
	SuperType::OnClose(reasonType);
}

void LobbyDBSession::MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
{
        SuperType::MsgHandleServerInit(ses, buf, bufRef);
        LOG_INFO("初始化 DBServer sid[{}] 成功!!!", ses->GetSID());
        DBMgr::GetInstance()->AddSession(std::dynamic_pointer_cast<LobbyDBSession>(ses));
}

}; // end of namespace nl::af::impl;

#endif

// vim: fenc=utf8:expandtab:ts=8
