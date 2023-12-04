#include "MySqlService.h"
#include <unordered_map>

template <>
bool MySqlService::Init()
{
        if (!SuperType::Init())
                return false;

#ifdef MYSQL_SERVICE_SERVER
        if (!MySqlMgr::GetInstance()->Init(ServerCfgMgr::GetInstance()->_mysqlCfg))
                return false;
#endif
        return true;
}

#ifdef MYSQL_SERVICE_SERVER

static std::atomic_uint64_t scMaxAllowedPackageSize = 32 * 1024 * 1024;

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

// }}}

SERVICE_NET_HANDLE(MySqlService::SessionType, E_MIMT_DB, E_MIDBST_ReqDBData, MailReqDBDataList)
{
        auto req = std::make_shared<DBReqWapper>(shared_from_this(), msgHead, msg);
        for (int64_t i=0; i<msg->list_size(); ++i)
        {
                auto item = msg->mutable_list(i);
                const auto taskType = item->task_type();
                if (E_MySql_TT_None == taskType || !EMySqlTaskType_IsValid(taskType))
                {
                        item->set_error_type(E_IET_DBDataTaskTypeError);
                        continue;
                }

                auto act = MySqlService::GetInstance()->GetMgrActor(item->guid());
                if (act)
                {
                        IMySqlTaskPtr task;
                        switch (taskType)
                        {
                        case E_MySql_TT_Version : task = std::make_shared<MySqlVersionTask>(); break;
                        case E_MySql_TT_Load :    task = std::make_shared<MySqlLoadTask>(); break;
                        case E_MySql_TT_Save :    task = std::make_shared<MySqlSaveTask>(); break;
                        default : break;
                        }

                        if (task)
                        {
                                if (E_MySql_TT_Save == taskType)
                                {
                                        task->_ref = msg;
                                        task->_data = std::move(item->data());
                                }
                                else
                                {
                                        task->_ref = req;
                                }

                                task->_pb = item;
                                task->_sid = GetSID();
                                task->_reqTime = GetClock().GetSteadyTime();

                                std::lock_guard l(act->_taskArrMutex[taskType]);
                                act->_taskArr[taskType][item->guid()] = task;
                        }
                        else
                        {
                                LOG_FATAL("");
                        }
                }
        }
}

bool MySqlActor::Init()
{
        if (!SuperType::Init())
                return false;

        auto thisPtr = shared_from_this();
        for (int64_t i=E_MySql_TT_None+1; i<EMySqlTaskType_ARRAYSIZE; ++i)
        {
                _execActorArr[i] = std::make_shared<MySqlExecActor>(thisPtr, _idx, static_cast<EMySqlTaskType>(i));
                _execActorArr[i]->Start();
        }

        std::string sqlStr = fmt::format("CREATE TABLE IF NOT EXISTS `data_{}` ( \
                             `id` bigint(0) NOT NULL, \
                             `v` bigint(0) NOT NULL, \
                             `data` longtext NOT NULL, \
                             PRIMARY KEY (`id`) USING BTREE \
                             ) ENGINE = InnoDB CHARACTER SET = utf8 COLLATE = utf8_general_ci ROW_FORMAT = Dynamic;",
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

        InitDealTaskTimer();
        InitDelVersionTimer();

        return true;
}

void MySqlActor::InitDealTaskTimer()
{
        // 必须使用 Timer。
        // 1，不能使用 boost::this_fiber::sleep_*，Actor 还需要接收其它邮件。
        // 2，不要使用 try_push，这样会降低性能，每个 task 都需要 try_push 一次。

        auto weakPtr = weak_from_this();
        _timer.Start(weakPtr, 0.1, [weakPtr]() {
                auto thisPtr = weakPtr.lock();
                if (thisPtr)
                {
                        thisPtr->DealTask();
                        thisPtr->InitDealTaskTimer();
                }
        });
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

void MySqlActor::DealTask()
{
        auto thisPtr = shared_from_this();
        while (true)
        {
                bool allEmpty = true;
                for (int64_t i=E_MySql_TT_None+1; i<EMySqlTaskType_ARRAYSIZE; ++i)
                {
                        std::unordered_map<uint64_t, IMySqlTaskPtr> tmpList;
                        {
                                std::lock_guard l(_taskArrMutex[i]);
                                tmpList = std::move(_taskArr[i]);
                                _taskArr[i].reserve(1024);
                        }

                        if (tmpList.empty())
                                continue;

                        allEmpty = false;
                        auto it = tmpList.begin();
                        for (; tmpList.end()!=it; ++it)
                        {
                                auto task = it->second;
                                if (!task->DealFromCache(thisPtr))
                                        _loadCacheTaskArr[i][task->_pb->guid()] = task;
                        }
                }

                if (allEmpty)
                        break;
        }
}

void MySqlActor::InitTerminateTimer()
{
        bool allEmpty = true;
        for (int64_t i=E_MySql_TT_None+1; i<EMySqlTaskType_ARRAYSIZE; ++i)
        {
                std::lock_guard l(_taskArrMutex[i]);
                if (!_taskArr[i].empty()
                    || !_loadCacheTaskArr[i].empty()
                    || !_loadTaskArr[i].empty())
                {
                        allEmpty = false;
                        break;
                }
        }

        if (allEmpty)
        {
                for (int64_t i=E_MySql_TT_None+1; i<EMySqlTaskType_ARRAYSIZE; ++i)
                        _execActorArr[i]->Terminate();

                // 必要要在其它协程等待以上所有 actor 退出后才能调用 SuperType::Terminate()。
                // SuperType::Terminate();
        }
        else
        {
                int64_t taskCnt = 0;
                int64_t loadCacheTaskCnt = 0;
                int64_t loadTaskCnt = 0;
                for (int64_t i=E_MySql_TT_None+1; i<EMySqlTaskType_ARRAYSIZE; ++i)
                {
                        std::lock_guard l(_taskArrMutex[i]);
                        taskCnt += _taskArr[i].size();
                        loadCacheTaskCnt += _loadCacheTaskArr[i].size();
                        loadTaskCnt += _loadTaskArr[i].size();
                }

                LOG_INFO("idx[{}] tc[{}] lctc[{}] ltc[{}]", _idx, taskCnt, loadCacheTaskCnt, loadTaskCnt);

                MySqlActorWeakPtr weakPtr = weak_from_this();
                _delVertionTimer.Start(weakPtr, 1.0, [weakPtr]() {
                        auto thisPtr = weakPtr.lock();
                        if (thisPtr)
                                thisPtr->InitTerminateTimer();
                });
        }
}

void MySqlActor::WaitForTerminate()
{
        for (int64_t i=E_MySql_TT_None+1; i<EMySqlTaskType_ARRAYSIZE; ++i)
                _execActorArr[i]->WaitForTerminate();

        SuperType::Terminate();
        SuperType::WaitForTerminate();
}

SPECIAL_ACTOR_MAIL_HANDLE(MySqlActor, 0x0, MailUInt)
{
        const auto taskType = msg->val();
        switch (taskType)
        {
        case E_MySql_TT_Version :
                for (auto& info : _versionInfoCacheList)
                        _versionInfoList.emplace(info);
                _versionInfoCacheList.clear();
                if (_versionInfoCacheList.size() > 1024)
                {
                        std::exchange(_versionInfoCacheList, {});
                        _versionInfoCacheList.reserve(1024);
                }
                break;
        default :
                break;
        }

        _loadTaskArr[taskType] = std::move(_loadCacheTaskArr[taskType]);
        _loadCacheTaskArr[taskType].reserve(1024);
        return msg;
}

bool MySqlExecActor::Init()
{
        if (!SuperType::Init())
                return false;

        InitExecTimer();
        return true;
}

void MySqlExecActor::InitExecTimer()
{
        Exec();

        auto weakPtr = weak_from_this();
        _timer.Start(weakPtr, 0.1, [weakPtr]() {
                auto thisPtr = weakPtr.lock();
                if (thisPtr)
                        thisPtr->InitExecTimer();
        });
}

void MySqlExecActor::Exec()
{
        // LOG_INFO("2222222222222222 _idx[{}] taskType[{}]", _idx, _taskType);
        auto act = _mysqlAct.lock();
        if (!act)
                return;

        auto thisPtr = shared_from_this();
        auto mail = std::make_shared<MailUInt>();
        mail->set_val(_taskType);
        while (true)
        {
                Call(MailUInt, act, scMySqlActorMailMainType, 0x0, mail);
                const auto& loadList = act->_loadTaskArr[_taskType];
                if (loadList.empty())
                        break;

                auto task = loadList.begin()->second;
                task->Exec(act, thisPtr, loadList);
        }
}

bool MySqlVersionTask::DealFromCache(const std::shared_ptr<MySqlActor>& act)
{
        auto& seq = act->_versionInfoList.get<by_id>();
        auto it = seq.find(_pb->guid());
        if (seq.end() != it)
        {
                auto info = *it;
                if (UINT32_MAX != info->_sid && _sid != info->_sid)
                {
                        LOG_INFO("玩家[{}] sid不匹配!!! sSID[{}] SID[{}]",
                                 info->_id, _sid, info->_sid);
                        _pb->set_error_type(E_IET_DBDataSIDError);
                }
                else
                {
                        info->_sid = _sid;
                        _pb->set_version(info->_version);
                        seq.erase(it);
                        info->_overTime = GetClock().GetSteadyTime() + 7 * 24 * 3600;
                        act->_versionInfoList.emplace(info);
                }
                return true;
        }
        else
        {
                return false;
        }
}

void MySqlVersionTask::Exec(const MySqlActorPtr& act
                            , const MySqlExecActorPtr& execAct
                            , const std::unordered_map<uint64_t, IMySqlTaskPtr>& taskList)
{
        GetApp()->_loadVersionCnt += taskList.size();

        auto now = GetClock().GetSteadyTime();
        auto it = taskList.begin();
        while (taskList.end() != it)
        {
                std::unordered_map<uint64_t, IMySqlTaskPtr> tmpList;

                std::string sqlStr = fmt::format("SELECT id,v FROM data_{} WHERE id IN(", execAct->_idx);
                sqlStr.reserve(MySqlService::scSqlStrInitSize);
                int64_t cnt = 0;
                for (; taskList.end() != it; ++it)
                {
                        auto task = it->second;
                        if (now - task->_reqTime <= IActor::scCallRemoteTimeOut)
                        {
                                tmpList.emplace(it->first, task);
                                sqlStr += fmt::format("{},", task->_pb->guid());
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
                execAct->PauseCostTime();
                auto result = MySqlMgr::GetInstance()->Exec(sqlStr);
                for (auto row : result->rows())
                {
                        auto id = row.at(0).as_int64();
                        auto version = row.at(1).as_int64();

                        auto it = tmpList.find(id);
                        if (tmpList.end() != it)
                        {
                                auto task = it->second;
                                tmpList.erase(it);

                                task->_pb->set_version(version);
                                auto info = std::make_shared<stVersionInfo>();
                                info->_sid = task->_sid;
                                info->_id = id;
                                info->_version = version;
                                info->_overTime = now + 7 * 24 * 3600;
                                act->_versionInfoCacheList.emplace_back(info);
                        }
                }
                execAct->ResumeCostTime();

                if (tmpList.empty())
                        continue;

                sqlStr.clear();
                sqlStr += fmt::format("INSERT INTO data_{}(id, v, data) VALUES", execAct->_idx);
                for (auto& val : tmpList)
                {
                        auto task = val.second;
                        sqlStr += fmt::format("({},0,''),", task->_pb->guid());

                        auto info = std::make_shared<stVersionInfo>();
                        info->_sid = task->_sid;
                        info->_id = task->_pb->guid();
                        info->_overTime = now + 7 * 24 * 3600;
                        act->_versionInfoCacheList.emplace_back(info);
                }
                sqlStr.pop_back();
                sqlStr += ";";
                GetApp()->_loadVersionSize += sqlStr.size();
                execAct->PauseCostTime();
                MySqlMgr::GetInstance()->Exec(sqlStr);
                execAct->ResumeCostTime();
        }
}

bool MySqlLoadTask::DealFromCache(const std::shared_ptr<MySqlActor>& act)
{
        const auto now = GetClock().GetSteadyTime();
        if (now - _reqTime > IActor::scCallRemoteTimeOut)
                return true;

        auto& seq = act->_versionInfoList.get<by_id>();
        auto it = seq.find(_pb->guid());
        if (seq.end() != it)
        {
                auto info = *it;
                if (_sid != info->_sid)
                {
                        LOG_INFO("玩家[{}] sid不匹配!!! sSID[{}] SID[{}]",
                                 _pb->guid(), _sid, info->_sid);
                        _pb->set_error_type(E_IET_DBDataSIDError);
                        return true;
                }

                // 先在本地缓存查找，再查找数据库，保证数据最新。
                auto it = act->_loadCacheTaskArr[_taskType].find(_pb->guid());
                if (act->_loadCacheTaskArr[_taskType].end() != it)
                {
                        auto task = it->second;
                        _pb->CopyFrom(*task->_pb);
                        _pb->set_error_type(E_IET_Success);
                        return true;
                }
                else
                {
                        it = act->_loadTaskArr[_taskType].find(_pb->guid());
                        if (act->_loadTaskArr[_taskType].end() != it) 
                        {
                                auto task = it->second;
                                _pb->CopyFrom(*task->_pb);
                                _pb->set_error_type(E_IET_Success);
                                return true;
                        }
                }
        }
        else
        {
                LOG_INFO("玩家[{}] LoadDBData 时 version 信息未找到!!!", _pb->guid());
                _pb->set_error_type(E_IET_DBDataSIDError);
                return true;
        }

        return false;
}

void MySqlLoadTask::Exec(const std::shared_ptr<MySqlActor>& act
                         , const std::shared_ptr<MySqlExecActor>& execAct
                         , const std::unordered_map<uint64_t, std::shared_ptr<IMySqlTask>>& reqList)
{
        auto it = reqList.begin();
        while (reqList.end() != it)
        {
                std::string sqlStr = fmt::format("SELECT id,data from data_{} WHERE id IN(", execAct->_idx);
                sqlStr.reserve(MySqlService::scSqlStrInitSize);

                auto now = GetClock().GetSteadyTime();
                int64_t cnt = 0;
                for (; reqList.end()!=it; ++it)
                {
                        auto task = it->second;
                        if (now - task->_reqTime <= IActor::scCallRemoteTimeOut)
                        {
                                sqlStr += fmt::format("{},", task->_pb->guid());
                                if (++cnt >= 1024)
                                {
                                        ++it;
                                        break;
                                }
                        }
                }
                sqlStr.pop_back();
                sqlStr += ");";

                execAct->PauseCostTime();
                auto result = MySqlMgr::GetInstance()->Exec(sqlStr);
                for (auto row : result->rows())
                {
                        auto id = row.at(0).as_int64();
                        auto it = reqList.find(id);
                        if (reqList.end() != it)
                        {
                                auto task = it->second;
                                _pb->set_data(row.at(1).as_string());
                                // task->_bufRef = result;
                                // task->_buf = row.at(1).as_string();
                        }
                }

                execAct->ResumeCostTime();

                GetApp()->_loadCnt += cnt;
                GetApp()->_loadSize += sqlStr.size();
        }
}

bool MySqlSaveTask::DealFromCache(const std::shared_ptr<MySqlActor>& act)
{
        // TODO: DBServer 重启后，会出现找不到 version 信息情况。
        auto& seq = act->_versionInfoList.get<by_id>();
        auto it = seq.find(_pb->guid());
        if (seq.end() != it)
        {
                auto info = *it;
                if (_sid != info->_sid)
                {
                        LOG_WARN("玩家[{}] sid不匹配!!! sSID[{}] SID[{}]",
                                 _pb->guid(), _sid, info->_sid);
                        _pb->set_error_type(E_IET_DBDataSIDError);
                        return true;
                }
                seq.erase(it);

                if (E_IET_DBDataDelete == _pb->error_type())
                {
                        // LOG_INFO("玩家[{}] 被删除!!!", msg->_pb->guid());
                        info->_sid = UINT32_MAX;
                }
                else
                {
                        // LOG_INFO("玩家[{}] 存储!!!", msg->_pb->guid());
                        info->_sid = _sid;
                }

                info->_version = _pb->version();
                info->_overTime = GetClock().GetSteadyTime() + 7 * 24 * 3600;
                act->_versionInfoList.emplace(info);
                return false;
        }
        else
        {
                LOG_WARN("玩家[{}] SaveDBData 时 version 信息未找到!!!", _pb->guid());
                _pb->set_error_type(E_IET_DBDataSIDError);
                return true;
        }
}

void MySqlSaveTask::Exec(const std::shared_ptr<MySqlActor>& act
                         , const std::shared_ptr<MySqlExecActor>& execAct
                         , const std::unordered_map<uint64_t, std::shared_ptr<IMySqlTask>>& reqList)
{
        auto it = reqList.begin();
        while (reqList.end() != it)
        {
                // DLOG_INFO("1111111111111111111111111 idx:{}", dbMgrActor->GetIdx());
                std::string sqlStr = fmt::format("UPDATE data_{} SET data=CASE id ", execAct->_idx);
                sqlStr.reserve(MySqlService::scSqlStrInitSize * 16);
                std::string versionSqlStr = "END,v=CASE id ";
                versionSqlStr.reserve(MySqlService::scSqlStrInitSize);
                std::string inStr = "END WHERE id IN(";
                inStr.reserve(MySqlService::scSqlStrInitSize);

                for (; reqList.end()!=it; ++it)
                {
                        auto pb = it->second->_pb;
                        sqlStr += fmt::format("WHEN {} THEN '{}' ", pb->guid(), it->second->_data);
                        versionSqlStr += fmt::format("WHEN {} THEN {} ", pb->guid(), pb->version());
                        inStr += fmt::format("{},", pb->guid());

                        if (sqlStr.size() + versionSqlStr.size() + inStr.size() > scMaxAllowedPackageSize * 0.9)
                        {
                                ++it;
                                break;
                        }
                }

                inStr.pop_back();
                inStr += ");";
                sqlStr += versionSqlStr;
                sqlStr += inStr;
                execAct->PauseCostTime();
                // auto oldTime = GetClock().GetTimeNow_Slow();
                MySqlMgr::GetInstance()->Exec(sqlStr);
                /*
                   auto endTime = GetClock().GetTimeNow_Slow();
                   LOG_INFO("1111111111111111111111111 end now[{}] diff[{}] size[{}] cnt[{}]",
                   endTime, endTime - oldTime, sqlStr.size() / (1024.0 * 1024.0), cnt);
                   */
                execAct->ResumeCostTime();
                GetApp()->_saveSize += sqlStr.size();
        }

        GetApp()->_saveCnt += reqList.size();
}

#endif

// vim: fenc=utf8:expandtab:ts=8
