#include "LoginDBSession.h"

std::string LoginDBSession::scPriorityTaskKey = "connect_db_server";

LoginDBSession::~LoginDBSession()
{
        // LOG_INFO("LoginDBSession 析构!!!");
}

void LoginDBSession::InitCheckFinishTimer()
{
        SteadyTimer::StaticStart(1, []() {
                int64_t cnt = 0;
                for (auto& ws : GetApp()->_dbSesArr)
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
                        LoginDBSession::InitCheckFinishTimer();
                }
        });
}

bool LoginDBSession::InitOnce()
{
        if (!SuperType::InitOnce())
                return false;

        InitCheckFinishTimer();
        return true;
}

void LoginDBSession::OnConnect()
{
	LOG_INFO("DBServer 连上来了!!!");
	SuperType::OnConnect();
}

void LoginDBSession::OnClose(int32_t reasonType)
{
	LOG_INFO("DBServer 断开连接!!!");
        GetApp()->RemoveSession(GetSID());
	SuperType::OnClose(reasonType);
}

void LoginDBSession::MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
{
        SuperType::MsgHandleServerInit(ses, buf, bufRef);
        LOG_INFO("初始化 DBServer sid[{}] 成功!!!", ses->GetSID());
        GetApp()->AddSession(std::dynamic_pointer_cast<LoginDBSession>(ses));
}

// vim: fenc=utf8:expandtab:ts=8
