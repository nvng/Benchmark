#include "LogService.h"

bool LogActor::Init()
{
        if (!SuperType::Init())
                return false;

        InitSaveTimer();

#ifdef LOG_SERVICE_CLIENT
        ResetCache();
#endif

        return true;
}

void LogActor::InitSaveTimer()
{
        LogActorWeakPtr weakThis = shared_from_this();
        ::nl::util::SteadyTimer::StaticStart(0.1, [weakThis]() {
                auto thisPtr = weakThis.lock();
                if (thisPtr)
                {
                        thisPtr->DealTimeout();
                        thisPtr->InitSaveTimer();
                }
        });
}

#ifdef LOG_SERVICE_SERVER

void LogActor::DealTimeout()
{
        for (int64_t i=0; i<ELogServiceLogMainType_ARRAYSIZE; ++i)
        {
                do
                {
                        auto reg = _sqlPrefixArr[i];
                        if (!reg)
                                break;

                        std::string sqlStr;
                        sqlStr.reserve(1024 * 1024);
                        fmt::format_to(std::back_inserter(sqlStr), fmt::runtime(reg->sql_prefix()), _idx);

                        auto oldSize = sqlStr.size();
                        for (auto& msg : _logList)
                                sqlStr += msg->data_list(i);
                        if (oldSize != sqlStr.size())
                        {
                                sqlStr.pop_back();
                                sqlStr += ";";
                                MySqlMgr::GetInstance()->Exec(sqlStr);
                        }
                } while (0);
        }
        _logList.clear();
}

SPECIAL_ACTOR_MAIL_HANDLE(LogActor, E_MILOGST_Register, MsgLogServiceRegister)
{
        for (auto& item : msg->item_list())
        {
                if (!ELogServiceLogMainType_IsValid(item.type()))
                        continue;

                if (!_sqlPrefixArr[item.type()])
                {
                        auto tmp = std::make_shared<MsgLogServiceRegisterItem>();
                        tmp->CopyFrom(item);
                        _sqlPrefixArr[tmp->type()] = tmp;
                        std::string sqlStr;
                        sqlStr.reserve(1024 * 1024);
                        fmt::format_to(std::back_inserter(sqlStr), fmt::runtime(tmp->sql_table_create()), _idx);
                        MySqlMgr::GetInstance()->Exec(sqlStr);
                }
                else
                {
                        LOG_ERROR("tttttt");
                }
        }
        return nullptr;
}

SPECIAL_ACTOR_MAIL_HANDLE(LogActor, E_MILOGST_Log, MsgLogServiceLog)
{
        if (!ELogServiceLogMainType_IsValid(msg->type()))
                return nullptr;

        _logList.emplace_back(msg);
        return nullptr;
}

SERVICE_NET_HANDLE(LogService::SessionType, E_MIMT_Log, E_MILOGST_Register, MsgLogServiceRegister)
{
        LogService::GetInstance()->ForeachActor([msg](const auto& weakAct) {
                auto act = weakAct.lock();
                if (act)
                        act->SendPush(nullptr, E_MILOGST_Register, msg);
        });
}

SERVICE_NET_HANDLE(LogService::SessionType, E_MIMT_Log, E_MILOGST_Log, MsgLogServiceLog)
{
        auto act = LogService::GetInstance()->GetActor(msg->idx());
        if (act)
                act->SendPush(nullptr, E_MILOGST_Register, msg);
}

#endif

#ifdef LOG_SERVICE_CLIENT

std::shared_ptr<MsgLogServiceLog> LogActor::ResetCache()
{
        bool hasData = false;
        std::lock_guard l(_cacheMutex);
        if (_cache)
        {
                for (int64_t i=0; i<_cache->data_list_size(); ++i)
                {
                        if (!_cache->data_list(i).empty())
                        {
                                hasData = true;
                                break;
                        }
                }

                if (!hasData)
                        return nullptr;
        }

        auto msg = std::make_shared<MsgLogServiceLog>();
        msg->set_idx(_idx);
        for (int64_t i=0; i<ELogServiceLogMainType_ARRAYSIZE; ++i)
                msg->add_data_list()->reserve(1024 * 1024);
        _cache.swap(msg);
        return msg;
}

void LogActor::DealTimeout()
{
        std::shared_ptr<MsgLogServiceLog> msg = ResetCache();
        auto ses = LogService::GetInstance()->DistSession(_sesIdx);
        if (msg && ses)
                ses->SendPB(msg, E_MIMT_Log, E_MILOGST_Log, LogService::SessionType::MsgHeaderType::E_RMT_Send, 0, 0, 0);
}

template <>
bool LogService::AddSession(const ::nl::net::ISessionPtr& s)
{
        auto ses = std::dynamic_pointer_cast<SessionType>(s);
        if (!ses)
                return false;

        auto msg = std::make_shared<MsgLogServiceRegister>();

        auto item = msg->add_item_list();
        item->set_type(E_LSLMT_Content);
        item->set_sql_table_create(LogServiceTableCreate<E_LSLMT_Content>());
        item->set_sql_prefix(LogServiceSqlPrefix<E_LSLMT_Content>());

        ses->SendPB(msg, E_MIMT_Log, E_MILOGST_Register, SessionType::MsgHeaderType::E_RMT_Send, 0, 0, 0);

        return SuperType::AddSession(ses);
}

SERVICE_NET_HANDLE(LogService::SessionType, E_MIMT_Log, E_MILOGST_Register, MsgLogServiceRegister)
{
}

#endif

template <>
bool LogService::Init()
{
        if (!SuperType::Init())
                return false;

#ifdef LOG_SERVICE_CLIENT
        SnowflakeLogGuid::Init(GetApp()->GetSID());
#endif
        return true;
}

