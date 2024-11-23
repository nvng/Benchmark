#include "MySqlService.h"

#ifdef MYSQL_SERVICE_SERVER

// {{{ DBDataSaveActor

bool DBDataSaveActor::Init()
{
        if (!SuperType::Init())
                return false;

        InitDealTimer();
        return true;
}

void DBDataSaveActor::InitDealTimer()
{
        switch (_idx)
        {
        case 0 : DealSave(); break;
        case 1 : DealLoad(); break;
        case 2 : DealLoadVersion(); break;
        default : break;
        }

        DBDataSaveActorWeakPtr weakPtr = shared_from_this();
        _timer.Start(weakPtr, 0.05, [weakPtr]() {
                auto thisPtr = weakPtr.lock();
                if (thisPtr)
                        thisPtr->InitDealTimer();
        });
}

void DBDataSaveActor::DealSave()
{
        auto dbMgrActor = _dbMgrActor.lock();
        if (!dbMgrActor)
                return;

        while (true)
        {
                Call(MailResult, dbMgrActor, scMySqlActorMailMainType, 0x2, nullptr);
                if (dbMgrActor->_saveCacheList.empty())
                        break;

                auto it = dbMgrActor->_saveCacheList.begin();
                while (dbMgrActor->_saveCacheList.end() != it)
                {
                        // DLOG_INFO("1111111111111111111111111 idx:{}", dbMgrActor->GetIdx());
                        std::string sqlStr = fmt::format("UPDATE data_{} SET data=CASE id ", dbMgrActor->GetIdx());
                        sqlStr.reserve(MySqlService::scSqlStrInitSize * 16);
                        std::string versionSqlStr = "END,v=CASE id ";
                        versionSqlStr.reserve(MySqlService::scSqlStrInitSize);
                        std::string inStr = "END WHERE id IN(";
                        inStr.reserve(MySqlService::scSqlStrInitSize);

                        int64_t cnt = 0;
                        for (; dbMgrActor->_saveCacheList.end()!=it; ++it)
                        {
                                ++cnt;
                                auto info = it->second;
                                sqlStr += fmt::format("WHEN {} THEN '{}' ", info->_pb->guid(), info->_buf);
                                versionSqlStr += fmt::format("WHEN {} THEN {} ", info->_pb->guid(), info->_pb->version());
                                inStr += fmt::format("{},", info->_pb->guid());

                                if (sqlStr.size() + versionSqlStr.size() + inStr.size() > MySqlActor::scMaxAllowedPackageSize * 0.9)
                                {
                                        ++it;
                                        break;
                                }
                        }

                        inStr.pop_back();
                        inStr += ");";
                        sqlStr += versionSqlStr;
                        sqlStr += inStr;
                        PauseCostTime();
                        // auto oldTime = GetClock().GetTimeNow_Slow();
                        MySqlMgr::GetInstance()->Exec(sqlStr);
                        /*
                           auto endTime = GetClock().GetTimeNow_Slow();
                           LOG_INFO("1111111111111111111111111 end now[{}] diff[{}] size[{}] cnt[{}]",
                           endTime, endTime - oldTime, sqlStr.size() / (1024.0 * 1024.0), cnt);
                           */
                        ResumeCostTime();
                        GetApp()->_saveCnt += cnt;
                        GetApp()->_saveSize += sqlStr.size();
                }
        }
}

void DBDataSaveActor::DealLoad()
{
        auto dbMgrActor = _dbMgrActor.lock();
        if (!dbMgrActor)
                return;

        while (true)
        {
                auto ret = Call(stDBDealList, dbMgrActor, scMySqlActorMailMainType, 0x1, nullptr);
                if (ret->_list.empty())
                        break;

                auto it = ret->_list.begin();
                while (ret->_list.end() != it)
                {
                        std::string sqlStr = fmt::format("SELECT id,data from data_{} WHERE id IN(", dbMgrActor->GetIdx());
                        sqlStr.reserve(MySqlService::scSqlStrInitSize);

                        auto now = GetClock().GetSteadyTime();
                        int64_t cnt = 0;
                        for (; ret->_list.end()!=it; ++it)
                        {
                                auto info = it->second;
                                if (now - info->_reqTime <= IActor::scCallRemoteTimeOut)
                                {
                                        sqlStr += fmt::format("{},", info->_pb->guid());
                                        if (++cnt >= 1024)
                                        {
                                                ++it;
                                                break;
                                        }
                                }
                        }
                        sqlStr.pop_back();
                        sqlStr += ");";

                        PauseCostTime();
                        auto result = MySqlMgr::GetInstance()->Exec(sqlStr);
                        for (auto row : result->rows())
                        {
                                auto id = row.at(0).as_int64();
                                auto it = ret->_list.find(id);
                                if (ret->_list.end() != it)
                                {
                                        // it->second->_pb->set_data(row.at(1).as_string());
                                        it->second->_bufRef = result;
                                        it->second->_buf = row.at(1).as_string();
                                        it->second->CallRet();
                                }
                        }

                        ResumeCostTime();

                        GetApp()->_loadCnt += cnt;
                        GetApp()->_loadSize += sqlStr.size();
                }
        }
}

void DBDataSaveActor::DealLoadVersion()
{
        auto dbMgrActor = _dbMgrActor.lock();
        if (!dbMgrActor)
                return;

        auto msg = std::make_shared<stDBDealList>();
        while (true)
        {
                Call(stDBDealList, dbMgrActor, scMySqlActorMailMainType, 0x0, msg);
                const auto& reqList = msg->_reqList;
                if (reqList.empty())
                        break;

                GetApp()->_loadVersionCnt += reqList.size();

                auto now = GetClock().GetSteadyTime();
                auto it = reqList.begin();
                while (reqList.end() != it)
                {
                        std::unordered_map<int64_t, stReqDBDataVersionPtr> tmpList;

                        std::string sqlStr = fmt::format("SELECT id,v FROM data_{} WHERE id IN(", dbMgrActor->GetIdx());
                        sqlStr.reserve(MySqlService::scSqlStrInitSize);
                        int64_t cnt = 0;
                        for (; reqList.end() != it; ++it)
                        {
                                auto reqInfo = it->second;
                                if (now - reqInfo->_reqTime <= IActor::scCallRemoteTimeOut)
                                {
                                        tmpList.emplace(it->first, reqInfo);
                                        sqlStr += fmt::format("{},", reqInfo->_pb->guid());
                                        if (++cnt >= 1024 * 10)
                                        {
                                                ++it;
                                                break;
                                        }
                                }
                        }

                        if (cnt <= 0)
                                continue;

                        sqlStr.pop_back();
                        sqlStr += ");";
                        GetApp()->_loadVersionSize += sqlStr.size();

                        auto now = GetClock().GetSteadyTime();
                        PauseCostTime();
                        auto result = MySqlMgr::GetInstance()->Exec(sqlStr);
                        for (auto row : result->rows())
                        {
                                auto id = row.at(0).as_int64();
                                auto version = row.at(1).as_int64();
                                auto it = tmpList.find(id);
                                if (tmpList.end() != it)
                                {
                                        auto reqInfo = it->second;
                                        auto info = std::make_shared<stVersionInfo>();
                                        info->_sid = reqInfo->_sid;
                                        reqInfo->_pb->set_version(version);
                                        tmpList.erase(it);

                                        info->_id = id;
                                        info->_version = version;
                                        info->_overTime = now + 7 * 24 * 3600;
                                        msg->_versionList.emplace_back(info);
                                }
                        }
                        ResumeCostTime();

                        if (tmpList.empty())
                                continue;

                        sqlStr.clear();
                        sqlStr += fmt::format("INSERT INTO data_{}(id, v, data) VALUES", dbMgrActor->GetIdx());
                        for (auto& val : tmpList)
                        {
                                auto reqInfo = val.second;
                                sqlStr += fmt::format("({},0,''),", reqInfo->_pb->guid());

                                auto info = std::make_shared<stVersionInfo>();
                                info->_sid = reqInfo->_sid;
                                info->_id = reqInfo->_pb->guid();
                                info->_overTime = now + 7 * 24 * 3600;
                                msg->_versionList.emplace_back(info);
                        }
                        sqlStr.pop_back();
                        sqlStr += ";";
                        GetApp()->_loadVersionSize += sqlStr.size();
                        PauseCostTime();
                        MySqlMgr::GetInstance()->Exec(sqlStr);
                        ResumeCostTime();
                }
        }
}

// }}}

// {{{ MySqlActor
std::atomic_uint64_t MySqlActor::scMaxAllowedPackageSize = 0;

bool MySqlActor::Init()
{
        if (!SuperType::Init())
                return false;

        _saveActor->_dbMgrActor = shared_from_this();
        _saveActor->Start();

        _loadActor->_dbMgrActor = shared_from_this();
        _loadActor->Start();

        _loadVersionActor->_dbMgrActor = shared_from_this();
        _loadVersionActor->Start();

        std::string sqlStr = fmt::format("CREATE TABLE IF NOT EXISTS `data_{}` ( \
                             `id` bigint(0) NOT NULL, \
                             `v` bigint(0) NOT NULL, \
                             `data` longtext NOT NULL, \
                             PRIMARY KEY (`id`) USING BTREE \
                             ) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci ROW_FORMAT = Dynamic;",
                             GetIdx());
        PauseCostTime();
        MySqlMgr::GetInstance()->Exec(sqlStr);
        ResumeCostTime();

        if (0 == GetIdx())
        {
                sqlStr = "SHOW VARIABLES LIKE 'max_allowed_packet'";
                auto result = MySqlMgr::GetInstance()->Exec(sqlStr);
                scMaxAllowedPackageSize = std::atoll(result->rows()[0][1].as_string().data());
        }

        InitDealTimer();
        InitDelVersionTimer();

        return true;
}

void MySqlActor::InitDealTimer()
{
        DealReqDBDataVersion();
        DealLoadDBData();
        DealSaveDBData();

        MySqlActorWeakPtr weakPtr = shared_from_this();
        _timer.Start(weakPtr, 0.05, [weakPtr]() {
                auto thisPtr = weakPtr.lock();
                if (thisPtr)
                        thisPtr->InitDealTimer();
        });
}

bool MySqlActor::DealReqDBDataVersion()
{
        decltype(_reqVersionCacheList) tmpList;
        {
                std::lock_guard l(_reqVersionCacheListMutex);
                tmpList = std::move(_reqVersionCacheList);
                _reqVersionCacheList.reserve(1024);
        }

        if (tmpList.empty())
                return false;

        const auto now = GetClock().GetSteadyTime();
        for (auto& val : tmpList)
        {
                auto msg = val.second;
                auto ses = msg->GetSession();
                if (!ses || now - msg->_reqTime > IActor::scCallRemoteTimeOut)
                        continue;

                auto pb = msg->_pb;
                auto& seq = _versionInfoList.get<by_id>();
                auto it = seq.find(pb->guid());
                if (seq.end() != it)
                {
                        auto info = *it;
                        if (UINT32_MAX != info->_sid && ses->GetSID() != info->_sid)
                        {
                                LOG_INFO("玩家[{}] sid不匹配!!! sSID[{}] SID[{}]",
                                         info->_id, ses->GetSID(), info->_sid);
                                pb->set_error_type(E_IET_DBDataSIDError);
                        }
                        else
                        {
                                info->_sid = ses->GetSID();
                                pb->set_version(info->_version);
                                seq.erase(it);
                                info->_overTime = now + 7 * 24 * 3600;
                                _versionInfoList.emplace(info);
                        }

                        msg->CallRet();
                }
                else
                {
                        _reqList.emplace(msg->_pb->guid(), msg);
                }
        }

        return true;
}

SPECIAL_ACTOR_MAIL_HANDLE(MySqlActor, 0x0, stDBDealList)
{
        // 先存储版本信息，再返回。
        for (auto& info : msg->_versionList)
                _versionInfoList.emplace(info);
        msg->_versionList.clear();

        for (auto& val : msg->_reqList)
                val.second->CallRet();

        msg->_reqList = std::move(_reqList);
        return msg;
}


bool MySqlActor::DealSaveDBData()
{
        decltype(_reqSaveCacheList) tmpList;
        {
                std::lock_guard l(_reqSaveCacheListMutex);
                tmpList = std::move(_reqSaveCacheList);
        }

        if (tmpList.empty())
                return false;

        for (auto& val : tmpList)
        {
                auto msg = val.second;
                do
                {
                        // TODO: DBServer 重启后，会出现找不到 version 信息情况。
                        auto& seq = _versionInfoList.get<by_id>();
                        auto it = seq.find(msg->_pb->guid());
                        if (seq.end() != it)
                        {
                                auto info = *it;
                                if (msg->_sid != info->_sid)
                                {
                                        LOG_WARN("玩家[{}] sid不匹配!!! sSID[{}] SID[{}]",
                                                 msg->_pb->guid(), msg->_sid, info->_sid);
                                        break;
                                }
                                seq.erase(it);

                                if (E_IET_DBDataDelete == msg->_pb->error_type())
                                {
                                        // LOG_INFO("玩家[{}] 被删除!!!", msg->_pb->guid());
                                        info->_sid = UINT32_MAX;
                                }
                                else
                                {
                                        // LOG_INFO("玩家[{}] 存储!!!", msg->_pb->guid());
                                        info->_sid = msg->_sid;
                                }

                                info->_version = msg->_pb->version();
                                info->_overTime = GetClock().GetSteadyTime() + 7 * 24 * 3600;
                                _versionInfoList.emplace(info);

                                _saveList[msg->_pb->guid()] = msg;
                        }
                        else
                        {
                                LOG_WARN("玩家[{}] SaveDBData 时 version 信息未找到!!!", msg->_pb->guid());
                                break;
                        }
                } while (0);
        }

        return true;
}

SPECIAL_ACTOR_MAIL_HANDLE(MySqlActor, 0x2)
{
        _saveCacheList = std::move(_saveList);
        static auto ret = std::make_shared<MailResult>();
        return ret;
}

bool MySqlActor::DealLoadDBData()
{
        decltype(_loadCacheList) tmpList;
        {
                std::lock_guard l(_loadCacheListMutex);
                tmpList = std::move(_loadCacheList);
                _loadCacheList.reserve(1024);
        }

        if (tmpList.empty())
                return false;

        const auto now = GetClock().GetSteadyTime();
        for (auto& val : tmpList)
        {
                auto msg = val.second;
                if (now - msg->_reqTime > IActor::scCallRemoteTimeOut)
                        continue;

                EInternalErrorType errorType = E_IET_None;
                do
                {
                        auto& seq = _versionInfoList.get<by_id>();
                        auto it = seq.find(msg->_pb->guid());
                        if (seq.end() != it)
                        {
                                auto info = *it;
                                if (msg->_sid != info->_sid)
                                {
                                        LOG_INFO("玩家[{}] sid不匹配!!! sSID[{}] SID[{}]",
                                                 msg->_pb->guid(), msg->_sid, info->_sid);
                                        errorType = E_IET_DBDataSIDError;
                                        break;
                                }

                                // 先在本地缓存查找，再查找数据库，保证数据最新。
                                auto it = _saveList.find(msg->_pb->guid());
                                if (_saveList.end() != it)
                                {
                                        msg->_pb = it->second->_pb;
                                        msg->_bufRef = it->second->_bufRef;
                                        msg->_buf = it->second->_buf;
                                        errorType = E_IET_Success;
                                        break;
                                }
                                else
                                {
                                        it = _saveCacheList.find(msg->_pb->guid());
                                        if (_saveCacheList.end() != it) 
                                        {
                                                msg->_pb = it->second->_pb;
                                                msg->_bufRef = it->second->_bufRef;
                                                msg->_buf = it->second->_buf;
                                                errorType = E_IET_Success;
                                                break;
                                        }
                                }
                        }
                        else
                        {
                                LOG_INFO("玩家[{}] LoadDBData 时 version 信息未找到!!!", msg->_pb->guid());
                                errorType = E_IET_DBDataSIDError;
                                break;
                        }

                        _loadList[msg->_pb->guid()] = msg;
                        errorType = E_IET_None;
                } while (0);

                if (E_IET_None != errorType)
                {
                        msg->_pb->set_error_type(errorType);
                        msg->CallRet();
                }
        }

        return true;
}

SPECIAL_ACTOR_MAIL_HANDLE(MySqlActor, 0x1)
{
        auto ret = std::make_shared<stDBDealList>();
        ret->_list = std::move(_loadList);
        return ret;
}

void MySqlActor::InitDelVersionTimer()
{
        auto& seq = _versionInfoList.get<by_overtime>();
        auto ie = seq.upper_bound(GetClock().GetSteadyTime());
        seq.erase(seq.begin(), ie);

        MySqlActorWeakPtr weakPtr = shared_from_this();
        _delVertionTimer.Start(weakPtr, 60 * 60, [weakPtr]() {
                auto thisPtr = weakPtr.lock();
                if (thisPtr)
                        thisPtr->InitDelVersionTimer();
        });
}

void MySqlActor::InitTerminateTimer()
{
        std::lock_guard l(_reqSaveCacheListMutex);
        if (_reqSaveCacheList.empty()
            && _saveList.empty()
            && _saveCacheList.empty())
        {
                _saveActor->Terminate();
                _loadActor->Terminate();
                _loadVersionActor->Terminate();

                // 必要要在其它协程等待以上三个 actor 退出后才能调用 SuperType::Terminate()。
                // SuperType::Terminate();
        }
        else
        {
                LOG_INFO("idx[{}] rsc[{}] s[{}] sc[{}]",
                         _idx, _reqSaveCacheList.size(), _saveList.size(), _saveCacheList.size());

                MySqlActorWeakPtr weakPtr = shared_from_this();
                _delVertionTimer.Start(weakPtr, 1, [weakPtr]() {
                        auto thisPtr = weakPtr.lock();
                        if (thisPtr)
                                thisPtr->InitTerminateTimer();
                });
        }
}

void MySqlActor::WaitForTerminate()
{
        _saveActor->WaitForTerminate();
        _loadActor->WaitForTerminate();
        _loadVersionActor->WaitForTerminate();

        SuperType::Terminate();
        SuperType::WaitForTerminate();
}

// }}}

// {{{ MySqlService

template <>
bool MySqlService::Init()
{
        if (!MySqlMgr::GetInstance()->Init(ServerCfgMgr::GetInstance()->_mysqlCfg))
                return false;
        return true;
}

template <>
void MySqlService::Terminate()
{
        for (int64_t i=0; i<scMgrActorCnt+1; ++i)
        {
                auto act = SuperType::_actorArr[i].lock();
                if (act)
                {
                        MySqlActorWeakPtr weakPtr = act;
                        act->SendPush([weakPtr]() {
                                auto act = weakPtr.lock();
                                if (act)
                                        act->InitTerminateTimer();
                        });
                }
        }
}

template <>
void MySqlService::WaitForTerminate()
{
        for (int64_t i=0; i<scMgrActorCnt+1; ++i)
        {
                auto act = SuperType::_actorArr[i].lock();
                if (act)
                        act->WaitForTerminate();
        }
}

SERVICE_NET_HANDLE(MySqlService::SessionType, E_MIMT_DB, E_MIDBST_DBDataVersion, MsgDBDataVersion)
{
        auto actor = MySqlService::GetInstance()->GetMgrActor(msg->guid());
        if (actor)
        { 
                auto info = std::make_shared<stReqDBDataVersionImpl<MySqlService::SessionType>>();
                info->_ses = shared_from_this();
                info->_pb = msg;
                info->_msgHead = msgHead;
                info->_sid = GetSID();
                info->_reqTime = GetClock().GetSteadyTime();

                std::lock_guard l(actor->_reqVersionCacheListMutex);
                actor->_reqVersionCacheList[msg->guid()] = info;
        }
        else
        {
                LOG_WARN("玩家[{}] 分配 MySqlActor 失败!!!", msg->guid());
        }
}

SERVICE_NET_HANDLE(MySqlService::SessionType, E_MIMT_DB, E_MIDBST_SaveDBData, MsgDBData, buf, bufRef)
{
        auto actor = MySqlService::GetInstance()->GetMgrActor(msg->guid());
        if (actor)
        {
                auto info = std::make_shared<stDBDataImpl<MySqlService::SessionType>>();
                info->_ses = shared_from_this();
                info->_pb = msg;
                info->_bufRef = bufRef;
                info->_buf = std::string_view((const char*)buf, bufSize);
                info->_msgHead = msgHead;
                info->_sid = GetSID();
                info->_reqTime = GetClock().GetSteadyTime();

                {
                        std::lock_guard l(actor->_reqSaveCacheListMutex);
                        actor->_reqSaveCacheList[std::make_pair(msg->guid(), info->_sid)] = info;
                }

                msg->set_error_type(E_IET_Success);
                SendPB(msg,
                       E_MIMT_DB,
                       E_MIDBST_SaveDBData,
                       MySqlService::SessionType::MsgHeaderType::E_RMT_CallRet,
                       msgHead._guid,
                       msgHead._to,
                       msgHead._from);
        } else {
                LOG_WARN("玩家[{}] 分配 MySqlActor 失败!!!", msg->guid());
        }
}

SERVICE_NET_HANDLE(MySqlService::SessionType, E_MIMT_DB, E_MIDBST_LoadDBData, MsgDBData)
{
        DLOG_WARN("玩家[{}] 向 MySql 请求玩家数据!!!", msg->guid());
        auto actor = MySqlService::GetInstance()->GetMgrActor(msg->guid());
        if (actor)
        {
                auto info = std::make_shared<stDBDataImpl<MySqlService::SessionType>>();
                info->_ses = shared_from_this();
                info->_pb = msg;
                info->_msgHead = msgHead;
                info->_sid = GetSID();
                info->_reqTime = GetClock().GetSteadyTime();

                std::lock_guard l(actor->_loadCacheListMutex);
                actor->_loadCacheList[msg->guid()] = info;
        }
        else
        {
                LOG_WARN("玩家[{}] 分配 MySqlActor 失败!!!", msg->guid());
        }
}

#endif

// }}}

// vim: fenc=utf8:expandtab:ts=8
