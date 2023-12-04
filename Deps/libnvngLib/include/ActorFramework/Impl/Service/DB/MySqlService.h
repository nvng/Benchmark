#pragma once

#include "ActorFramework/SpecialActor.hpp"
#include "Tools/ServerList.hpp"
#include "msg_db.pb.h"

#ifdef MYSQL_SERVICE_SERVER

struct stVersionInfo
{
        uint64_t _id = 0;
        uint32_t _sid = UINT32_MAX;
        uint32_t _version = 0;
        uint64_t _overTime = 0;
};
typedef std::shared_ptr<stVersionInfo> stVersionInfoPtr;

struct by_id;
struct by_overtime;

typedef boost::multi_index::multi_index_container
<
	stVersionInfoPtr,
	boost::multi_index::indexed_by <

        boost::multi_index::hashed_unique<
        boost::multi_index::tag<by_id>,
        BOOST_MULTI_INDEX_MEMBER(stVersionInfo, uint64_t, _id)
        >,

        boost::multi_index::ordered_non_unique<
        boost::multi_index::tag<by_overtime>,
        BOOST_MULTI_INDEX_MEMBER(stVersionInfo, uint64_t, _overTime)
        >

	>
> VersionInfoListType;

class IMySqlTask;
class MySqlExecActor;

#endif

SPECIAL_ACTOR_DEFINE_BEGIN(MySqlActor, 0x1);

#ifdef MYSQL_SERVICE_SERVER

public :
        explicit MySqlActor(int64_t idx)
                : _idx(idx)
        {
                for (int64_t i=E_MySql_TT_None+1; i<EMySqlTaskType_ARRAYSIZE; ++i)
                {
                        _taskArr[i].reserve(1024);
                        _loadCacheTaskArr[i].reserve(1024);
                        _loadTaskArr[i].reserve(1024);
                }

                _versionInfoList.reserve(1024);
                _versionInfoCacheList.reserve(1024);
        }

        bool Init() override;
        void DealTask();
        FORCE_INLINE int64_t GetIdx() const { return _idx; }

        void InitDealTaskTimer();
        void InitDelVersionTimer();
        void InitTerminateTimer();
        void WaitForTerminate() override;

public :
        ::nl::util::SteadyTimer _timer;
        ::nl::util::SteadyTimer _delVertionTimer;

        int64_t _idx = 0;
        std::shared_ptr<MySqlExecActor> _execActorArr[EMySqlTaskType_ARRAYSIZE];
        SpinLock _taskArrMutex[EMySqlTaskType_ARRAYSIZE];
        std::unordered_map<uint64_t, std::shared_ptr<IMySqlTask>> _taskArr[EMySqlTaskType_ARRAYSIZE];
        std::unordered_map<uint64_t, std::shared_ptr<IMySqlTask>> _loadCacheTaskArr[EMySqlTaskType_ARRAYSIZE];
        std::unordered_map<uint64_t, std::shared_ptr<IMySqlTask>> _loadTaskArr[EMySqlTaskType_ARRAYSIZE];

        VersionInfoListType _versionInfoList;
        std::vector<stVersionInfoPtr> _versionInfoCacheList;

#endif

SPECIAL_ACTOR_DEFINE_END(MySqlActor);

#ifdef MYSQL_SERVICE_SERVER

SPECIAL_ACTOR_DEFINE_BEGIN(MySqlExecActor, 0x2);

public :
        MySqlExecActor(const std::shared_ptr<MySqlActor>& act, int64_t idx, EMySqlTaskType taskType)
                : _mysqlAct(act), _idx(idx), _taskType(taskType)
        {
        }

        bool Init() override;
        void Exec();
        void InitExecTimer();

        ::nl::util::SteadyTimer _timer;
        MySqlActorWeakPtr _mysqlAct;
        int64_t _idx = 0;
        EMySqlTaskType _taskType = static_cast<EMySqlTaskType>(9999);

SPECIAL_ACTOR_DEFINE_END(MySqlExecActor);

#endif

#ifdef MYSQL_SERVICE_SERVER
DECLARE_SERVICE_BASE_BEGIN(MySql, SessionDistributeNull, ServiceSession);
#endif

#ifdef MYSQL_SERVICE_CLIENT
DECLARE_SERVICE_BASE_BEGIN(MySql, SessionDistributeSID, ServiceSession);
#endif

public :
        bool Init();

private :
        MySqlServiceBase()
                : SuperType("MySqlService")
        {
#ifdef MYSQL_SERVICE_SERVER
                MySqlMgr::CreateInstance();
#endif
        }

#ifdef MYSQL_SERVICE_SERVER

private :
        ~MySqlServiceBase() override { MySqlMgr::DestroyInstance(); }

public :
        template <typename ... Args>
        bool StartLocal(int64_t actCnt, Args ... args)
        {
                actCnt = scMgrActorCnt + 1;
                LOG_FATAL_IF(!CHECK_2N(actCnt), "actCnt 必须设置为 2^N !!!", actCnt);
                for (int64_t i=0; i<actCnt; ++i)
                {
                        auto act = std::make_shared<typename SuperType::ActorType>(i);
                        SuperType::_actorArr.emplace_back(act);
                        act->Start();
                }

                return true;
        }

        FORCE_INLINE std::shared_ptr<MySqlActor> GetMgrActor(uint64_t id)
        { return SuperType::_actorArr[id & scMgrActorCnt].lock(); }
public :
        constexpr static int64_t scSqlStrInitSize = 1024 * 1024;
private :
        friend class MySqlActor;
        // constexpr static int64_t scMgrActorCnt = (1 << 6) - 1;
        constexpr static int64_t scMgrActorCnt = 0;

public :
        void Terminate() override;
        void WaitForTerminate() override;

#endif

#ifdef MYSQL_SERVICE_CLIENT

public :
        enum EMySqlServiceStatus
        {
                E_MySqlS_None = 0x0,
                E_MySqlS_Fail = 0x1,
                E_MySqlS_Success = 0x2,
                E_MySqlS_New = 0x3,
        };

#if 0
        template <typename _Ay, typename _Dy>
        EMySqlServiceStatus Load(const std::shared_ptr<_Ay>& act, uint64_t id, _Dy& dbInfo, std::string_view prefix)
        {
                auto ses = SuperType::DistSession(id);
                if (!ses)
                {
                        /*
                        int64_t cnt = 0;
                        (void)cnt;
                        for (auto& ws : _dbSesArr)
                        {
                                if (ws.lock())
                                        ++cnt;
                        }
                        PLAYER_LOG_WARN("act[{}] Load时，db ses 分配失败!!! sesCnt[{}]", act->GetID(), cnt);
                        */
                        return E_MySqlS_Fail;
                }

                auto agent = std::make_shared<typename SessionType::ActorAgentType>(GenReqID(), ses);
                agent->BindActor(act);
                ses->AddAgent(agent);

                auto msg = std::make_shared<MsgDBDataVersion>();
                msg->set_guid(id);
                auto versionRet = Call(MsgDBDataVersion, act, agent, E_MIMT_DB, E_MIDBST_DBDataVersion, msg);
                if (!versionRet)
                {
                        LOG_INFO("玩家[{}] 请求数据版本超时!!!", id);
                        return E_MySqlS_Fail;
                }

                auto parseDataFunc = [&dbInfo](std::string_view data) {
                        if (Compress::UnCompressAndParseAlloc<Compress::E_CT_Zstd>(dbInfo, data))
                                return E_MySqlS_Success;
                        else
                                return E_MySqlS_Fail;
                };

                // TODO: 不同类型 actor id 重复。
                auto loadFromMysqlFunc = [act, id, agent, &dbInfo, &versionRet, &parseDataFunc]() {
                        auto loadDBData = std::make_shared<MsgDBData>();
                        loadDBData->set_guid(id);
                        auto loadMailRet = act->CallInternal(agent, E_MIMT_DB, E_MIDBST_LoadDBData, loadDBData);
                        if (!loadMailRet)
                        {
                                LOG_WARN("玩家[{}] load from mysql 时，超时!!!", id);
                                return E_MySqlS_Fail;
                        }

                        MsgDBData loadRet;
                        auto [bufRef, buf, ret] = loadMailRet->ParseExtra(loadRet);
                        if (!ret)
                        {
                                LOG_WARN("玩家[{}] load from mysql 时，解析出错!!!", id);
                                return E_MySqlS_Fail;
                        }

                        if (E_IET_DBDataSIDError != loadRet.error_type())
                        {
                                if (buf.empty())
                                {
                                        return E_MySqlS_New;
                                }
                                else
                                {
                                        auto status = parseDataFunc(Base64Decode(buf));
                                        LOG_WARN_IF(versionRet->version() != dbInfo.version(), "玩家[{}] 从 MySql 获取数据 size:{}，版本不匹配!!! v:{} dbv:{}",
                                                    id, buf.length(), versionRet->version(), dbInfo.version());
                                        return status;
                                }
                        }
                        else
                        {
                                // p->KickOut(E_CET_StateError); // 无效，还没登录成功，_clientActor 为空。
                                return E_MySqlS_Fail;
                        }

                        return E_MySqlS_Fail;
                };

                std::string cmd = fmt::format("{}:{}", prefix, id);
                auto loadRet = act->RedisCmd("GET", cmd);
                if (!loadRet)
                        return E_MySqlS_Fail;

                if (loadRet->IsNil())
                {
                        if (0 == versionRet->version())
                        {
                                return E_MySqlS_New;
                        }
                        else
                        {
                                LOG_WARN("玩家[{}] 从 redis 获取数据为空!!! v:{}",
                                         id, versionRet->version());
                                return loadFromMysqlFunc();
                        }
                }
                else
                {
                        auto [data, err] = loadRet->GetStr();
                        if (!err)
                        {
                                auto status = parseDataFunc(data);
                                switch (status)
                                {
                                case E_MySqlS_Success :
                                        if (versionRet->version() > dbInfo.version())
                                        {
                                                LOG_WARN("玩家[{}] 从 redis 获取数据，版本不匹配!!! v:{} dbv:{}",
                                                         id, versionRet->version(), dbInfo.version());
                                                return loadFromMysqlFunc();
                                        }
                                        else
                                        {
                                                return status;
                                        }
                                        break;
                                default :
                                        break;
                                }
                        }
                }

                return E_MySqlS_Fail;
        }

        template <typename _Ay, typename _Dy>
        bool Save(const std::shared_ptr<_Ay>& act
                  , uint64_t id
                  , _Dy& dbInfo
                  , std::string_view prefix
                  , bool isDelete)
        {
                auto ses = SuperType::DistSession(id);
                if (!ses)
                {
                        /*
                        int64_t cnt = 0;
                        (void)cnt;
                        for (auto& ws : _dbSesArr)
                        {
                                if (ws.lock())
                                        ++cnt;
                        }
                        LOG_WARN("玩家[{}] 存储时，db ses 分配失败!!! sesCnt[{}]", act->GetID(), cnt);
                        */
                        return false;
                }

                auto agent = std::make_shared<typename SessionType::ActorAgentType>(GenReqID(), ses);
                agent->BindActor(act);
                ses->AddAgent(agent);

                auto [bufRef, bufSize] = Compress::SerializeAndCompress<Compress::E_CT_Zstd>(dbInfo);

                auto saveDBData = std::make_shared<MsgDBData>();
                if (isDelete)
                        saveDBData->set_error_type(E_IET_DBDataDelete);
                saveDBData->set_guid(id);
                saveDBData->set_version(dbInfo.version());
                auto [base64DataRef, base64DataSize] = Base64EncodeExtra(bufRef.get(), bufSize);
                // saveDBData->set_data(base64Data);
                /* 有问题。
                   saveDBData->set_allocated_data(&base64Data);
                   */

                auto saveRet = ParseMailData<MsgDBData>(act->CallInternal(agent, E_MIMT_DB, E_MIDBST_SaveDBData, saveDBData, base64DataRef, base64DataRef.get(), base64DataSize).get(), E_MIMT_DB, E_MIDBST_SaveDBData);
                if (!saveRet)
                {
                        LOG_WARN("玩家[{}] 存储到 DBServer 超时!!!", id);
                        return false;
                }

                std::string key = fmt::format("{}:{}", prefix, id);
                bredis::command_container_t cmdList = {
                        { "SET", key, std::string_view(bufRef.get(), bufSize) },
                        { "EXPIRE", key, EXPIRE_TIME_STR },
                };
                act->RedisCmd(std::move(cmdList));

                return true;
        }
#endif

        template <typename _Ay, typename _Dy>
        EMySqlServiceStatus Load(const std::shared_ptr<_Ay>& act, uint64_t id, _Dy& dbInfo, std::string_view prefix)
        {
                auto ses = SuperType::DistSession(id);
                if (!ses)
                {
                        /*
                        int64_t cnt = 0;
                        (void)cnt;
                        for (auto& ws : _dbSesArr)
                        {
                                if (ws.lock())
                                        ++cnt;
                        }
                        PLAYER_LOG_WARN("act[{}] Load时，db ses 分配失败!!! sesCnt[{}]", act->GetID(), cnt);
                        */
                        return E_MySqlS_Fail;
                }

                auto agent = std::make_shared<typename SessionType::ActorAgentType>(GenReqID(), ses);
                agent->BindActor(act);
                ses->AddAgent(agent);

                auto mail = std::make_shared<MailReqDBDataList>();
                auto item = mail->add_list();
                item->set_task_type(E_MySql_TT_Version);
                item->set_guid(id);
                auto versionRet = Call(MailReqDBDataList, act, agent, E_MIMT_DB, E_MIDBST_ReqDBData, mail);
                if (!versionRet || 1 != versionRet->list_size())
                {
                        LOG_INFO("玩家[{}] 请求数据版本超时!!!", id);
                        return E_MySqlS_Fail;
                }
                auto& versionItem = versionRet->list(0);

                auto parseDataFunc = [&dbInfo](std::string_view data) {
                        if (Compress::UnCompressAndParseAlloc<Compress::E_CT_Zstd>(dbInfo, data))
                                return E_MySqlS_Success;
                        else
                                return E_MySqlS_Fail;
                };

                // TODO: 不同类型 actor id 重复。
                auto loadFromMysqlFunc = [act, id, agent, &dbInfo, &versionRet, &parseDataFunc]() {
                        auto loadDBData = std::make_shared<MailReqDBDataList>();
                        auto item = loadDBData->add_list();
                        item->set_task_type(E_MySql_TT_Load);
                        item->set_guid(id);
                        auto loadRet = Call(MailReqDBDataList, act, agent, E_MIMT_DB, E_MIDBST_ReqDBData, loadDBData);
                        if (!loadRet && 1 != loadRet->list_size())
                        {
                                LOG_WARN("玩家[{}] load from mysql 时，超时!!!", id);
                                return E_MySqlS_Fail;
                        }

                        auto& loadItem = loadRet->list(0);
                        if (E_IET_DBDataSIDError != loadItem.error_type())
                        {
                                if (loadItem.data().empty())
                                {
                                        return E_MySqlS_New;
                                }
                                else
                                {
                                        auto status = parseDataFunc(Base64Decode(loadItem.data()));
                                        auto& versionItem = versionRet->list(0);
                                        LOG_WARN_IF(versionItem.version() != dbInfo.version(), "玩家[{}] 从 MySql 获取数据 size:{}，版本不匹配!!! v:{} dbv:{}",
                                                    id, loadItem.data().length(), versionItem.version(), dbInfo.version());
                                        return status;
                                }
                        }
                        else
                        {
                                // p->KickOut(E_CET_StateError); // 无效，还没登录成功，_clientActor 为空。
                                return E_MySqlS_Fail;
                        }

                        return E_MySqlS_Fail;
                };

                std::string cmd = fmt::format("{}:{}", prefix, id);
                auto loadRet = act->RedisCmd("GET", cmd);
                if (!loadRet)
                        return E_MySqlS_Fail;

                if (loadRet->IsNil())
                {
                        if (0 == versionItem.version())
                        {
                                return E_MySqlS_New;
                        }
                        else
                        {
                                LOG_WARN("玩家[{}] 从 redis 获取数据为空!!! v:{}",
                                         id, versionItem.version());
                                return loadFromMysqlFunc();
                        }
                }
                else
                {
                        auto [data, err] = loadRet->GetStr();
                        if (!err)
                        {
                                auto status = parseDataFunc(data);
                                switch (status)
                                {
                                case E_MySqlS_Success :
                                        if (versionItem.version() > dbInfo.version())
                                        {
                                                LOG_WARN("玩家[{}] 从 redis 获取数据，版本不匹配!!! v:{} dbv:{}",
                                                         id, versionItem.version(), dbInfo.version());
                                                return loadFromMysqlFunc();
                                        }
                                        else
                                        {
                                                return status;
                                        }
                                        break;
                                default :
                                        break;
                                }
                        }
                }

                return E_MySqlS_Fail;
        }

        template <typename _Ay, typename _Dy>
        bool Save(const std::shared_ptr<_Ay>& act, uint64_t id, _Dy& dbInfo, std::string_view prefix, bool isDelete)
        {
                auto ses = SuperType::DistSession(id);
                if (!ses)
                {
                        /*
                        int64_t cnt = 0;
                        (void)cnt;
                        for (auto& ws : _dbSesArr)
                        {
                                if (ws.lock())
                                        ++cnt;
                        }
                        LOG_WARN("玩家[{}] 存储时，db ses 分配失败!!! sesCnt[{}]", act->GetID(), cnt);
                        */
                        return false;
                }

                auto agent = std::make_shared<typename SessionType::ActorAgentType>(GenReqID(), ses);
                agent->BindActor(act);
                ses->AddAgent(agent);

                auto [bufRef, bufSize] = Compress::SerializeAndCompress<Compress::E_CT_Zstd>(dbInfo);

                auto saveDBData = std::make_shared<MailReqDBDataList>();
                auto item = saveDBData->add_list();
                item->set_task_type(E_MySql_TT_Save);
                if (isDelete)
                        item->set_error_type(E_IET_DBDataDelete);
                item->set_guid(id);
                item->set_version(dbInfo.version());
                auto [base64DataRef, base64DataSize] = Base64EncodeExtra(bufRef.get(), bufSize);
                item->set_data(base64DataRef.get(), base64DataSize);
                /* 有问题。
                   saveDBData->set_allocated_data(&base64Data);
                   */

                auto saveRet = Call(MailReqDBDataList, act, agent, E_MIMT_DB, E_MIDBST_ReqDBData, saveDBData);
                if (!saveRet)
                {
                        LOG_WARN("玩家[{}] 存储到 DBServer 超时!!!", id);
                        return false;
                }

                std::string key = fmt::format("{}:{}", prefix, id);
                bredis::command_container_t cmdList = {
                        { "SET", key, std::string_view(bufRef.get(), bufSize) },
                        { "EXPIRE", key, EXPIRE_TIME_STR },
                };
                act->RedisCmd(std::move(cmdList));

                return true;
        }

        template <typename _Ay, typename _Dy>
        EMySqlServiceStatus LoadBatch(const std::shared_ptr<_Ay>& act
                                      , std::unordered_map<uint64_t, std::pair<std::shared_ptr<_Dy>, EMySqlServiceStatus>>& dataList
                                      , std::string_view prefix)
        {
                // load version info。
                std::vector<std::pair<std::shared_ptr<typename SessionType::ActorAgentType>, std::shared_ptr<MailReqDBDataList>>> tmpList;
                tmpList.resize(ServerListCfgMgr::GetInstance()->GetSize<stDBServerInfo>());
                for (auto& val : dataList)
                {
                        auto id = val.first;
                        auto dbInfo = val.second.first;

                        const auto idx = id & (tmpList.size() - 1);
                        auto agentInfo = tmpList[idx];
                        if (!agentInfo.first)
                        {
                                auto ses = SuperType::DistSession(id);
                                if (!ses)
                                        return E_MySqlS_Fail;

                                auto agent = std::make_shared<typename SessionType::ActorAgentType>(GenReqID(), ses);
                                agent->BindActor(act);
                                ses->AddAgent(agent);

                                agentInfo = std::make_pair(agent, std::make_shared<MailReqDBDataList>());
                                tmpList[idx] = agentInfo;
                        }

                        auto item = agentInfo.second->add_list();
                        item->set_task_type(E_MySql_TT_Version);
                        item->set_guid(id);
                }

                std::unordered_map<uint64_t, uint64_t> versionList;
                versionList.reserve(dataList.size());
                for (auto& val : tmpList)
                {
                        auto agent = val.first;
                        auto mail = val.second;
                        auto versionRet = Call(MailReqDBDataList, act, agent, E_MIMT_DB, E_MIDBST_ReqDBData, mail);
                        if (!versionRet || mail->list_size() != versionRet->list_size())
                        {
                                LOG_INFO("玩家 请求数据版本超时!!!");
                                return E_MySqlS_Fail;
                        }

                        for (auto& item : versionRet->list())
                        {
                                auto it = dataList.find(item.guid());
                                if (dataList.end() != it)
                                        it->second.first->set_version(item.version());
                                else
                                        LOG_WARN("");
                        }
                }

                auto parseDataFunc = [](auto& dbInfo, std::string_view data) {
                        if (Compress::UnCompressAndParseAlloc<Compress::E_CT_Zstd>(dbInfo, data))
                                return E_MySqlS_Success;
                        else
                                return E_MySqlS_Fail;
                };

                // load from redis。
                std::vector<std::string> keyList;
                keyList.reserve(dataList.size());
                for (auto& val : dataList)
                        keyList.emplace_back(fmt::format("{}:{}", prefix, val.first));

                bredis::command_container_t cmdList;
                cmdList.reserve(dataList.size());
                int64_t idx = -1;
                for (auto& val : dataList)
                        cmdList.emplace_back(bredis::single_command_t{ "GET", keyList[++idx] });

                auto redisRet = act->RedisCmd(std::move(cmdList));
                if (!redisRet)
                        return E_MySqlS_Fail;

                std::vector<uint64_t> loadFromMysqlList;
                loadFromMysqlList.reserve(dataList.size());
                for (auto& val : dataList)
                {
                        auto id = val.first;
                        auto dbInfo = val.second.first;
                        EMySqlServiceStatus& status = val.second.second;
                        auto version = dbInfo->version();

                        if (redisRet->IsNil())
                        {
                                if (0 == version)
                                {
                                        status = E_MySqlS_New;
                                        val.second.second = E_MySqlS_New;
                                }
                                else
                                {
                                        LOG_WARN("玩家[{}] 从 redis 获取数据为空!!! v:{}", id, version);
                                        // return loadFromMysqlFunc();
                                        loadFromMysqlList.emplace_back(id);
                                }
                        }
                        else
                        {
                                auto [data, err] = redisRet->GetStr();
                                if (!err)
                                {
                                        status = parseDataFunc(*dbInfo, data);
                                        switch (status)
                                        {
                                        case E_MySqlS_Success :
                                                if (version > dbInfo->version())
                                                {
                                                        LOG_WARN("玩家[{}] 从 redis 获取数据，版本不匹配!!! v:{} dbv:{}",
                                                                 id, version, dbInfo->version());
                                                        // return loadFromMysqlFunc();

                                                        dbInfo->set_version(version);
                                                        loadFromMysqlList.emplace_back(id);
                                                }
                                                break;
                                        default :
                                                break;
                                        }
                                }
                        }
                }

                if (loadFromMysqlList.empty())
                        return;

                // redis 中未找到，version > 0，从 MySql 中取。
                for (auto& val : tmpList)
                        val.second->clear_list();

                for (auto id : loadFromMysqlList)
                {
                        const auto idx = id & (tmpList.size() - 1);
                        auto agentInfo = tmpList[idx];
                        auto item = agentInfo.second->add_list();
                        item->set_task_type(E_MySql_TT_Load);
                        item->set_guid(id);
                }

                for (auto& val : tmpList)
                {
                        auto agent = val.first;
                        auto mail = val.second;
                        auto loadRet = Call(MailReqDBDataList, act, agent, E_MIMT_DB, E_MIDBST_ReqDBData, mail);
                        if (!loadRet && mail->list_size() != loadRet->list_size())
                        {
                                LOG_WARN("玩家 load from mysql 时，超时!!!");
                                return E_MySqlS_Fail;
                        }

                        for (auto& item : loadRet->list())
                        {
                                auto it = dataList.find(item.guid());
                                if (dataList.end() == it)
                                {
                                        LOG_WARN("");
                                        continue;
                                }

                                auto id = it->first;
                                auto dbInfo = it->second.first;
                                auto& status = it->second.second;
                                if (E_IET_DBDataSIDError != item.error_type())
                                {
                                        if (item.data().empty())
                                        {
                                                status = E_MySqlS_New;
                                        }
                                        else
                                        {
                                                auto version = dbInfo->version();
                                                status = parseDataFunc(*dbInfo, Base64Decode(item.data()));
                                                LOG_WARN_IF(item.version() != dbInfo->version(), "玩家[{}] 从 MySql 获取数据 size:{}，版本不匹配!!! v:{} dbv:{}",
                                                            id, item.data().length(), version, dbInfo->version());
                                                return status;
                                        }
                                }
                                else
                                {
                                        // p->KickOut(E_CET_StateError); // 无效，还没登录成功，_clientActor 为空。
                                        status = E_MySqlS_Fail;
                                }
                        }
                }
        }

        template <typename _Ay, typename _Dy>
        bool SaveBatch(const std::shared_ptr<_Ay>& act
                       , const std::vector<std::tuple<uint64_t, std::shared_ptr<_Dy>, bool>>& dataList
                       , std::string_view prefix)
        {
                std::vector<std::tuple<std::string, std::shared_ptr<char[]>, std::size_t>> bufList;
                bufList.reserve(dataList.size());

                std::vector<std::pair<std::shared_ptr<typename SessionType::ActorAgentType>, std::shared_ptr<MailReqDBDataList>>> tmpList;
                tmpList.resize(ServerListCfgMgr::GetInstance()->GetSize<stDBServerInfo>());
                for (auto& v : dataList)
                {
                        auto id = std::get<0>(v);
                        auto dbInfo = std::get<1>(v);
                        auto isDelete = std::get<2>(v);

                        const auto idx = id & (tmpList.size() - 1);
                        auto agentInfo = tmpList[idx];
                        if (!agentInfo.first)
                        {
                                auto ses = SuperType::DistSession(id);
                                if (!ses)
                                        return false;

                                auto agent = std::make_shared<typename SessionType::ActorAgentType>(GenReqID(), ses);
                                agent->BindActor(act);
                                ses->AddAgent(agent);

                                agentInfo = std::make_pair(agent, std::make_shared<MailReqDBDataList>());
                                tmpList[idx] = agentInfo;
                        }

                        auto item = agentInfo.second->add_list();
                        item->set_task_type(E_MySql_TT_Save);
                        if (isDelete)
                                item->set_error_type(E_IET_DBDataDelete);
                        item->set_guid(id);
                        item->set_version(dbInfo->version());

                        auto [bufRef, bufSize] = Compress::SerializeAndCompress<Compress::E_CT_Zstd>(*dbInfo);
                        bufList.emplace_back(std::make_tuple(fmt::format("{}:{}", prefix, id), bufRef, bufSize));
                        auto [base64DataRef, base64DataSize] = Base64EncodeExtra(bufRef.get(), bufSize);
                        item->set_data(base64DataRef.get(), base64DataSize);
                }

                for (auto& val : tmpList)
                {
                        auto agent = val.first;
                        if (agent)
                        {
                                auto saveRet = Call(MailReqDBDataList, act, agent, E_MIMT_DB, E_MIDBST_ReqDBData, val.second);
                                if (!saveRet)
                                {
                                        LOG_WARN("批量存储到 DBServer sid[{}] 超时!!!", agent->GetSID());
                                        return false;
                                }
                        }
                }

                bredis::command_container_t cmdList;
                cmdList.reserve(bufList.size() * 2);
                for (auto& v : bufList)
                {
                        cmdList.emplace_back(bredis::single_command_t{ "SET", std::get<0>(v), std::string_view(std::get<1>(v).get(), std::get<2>(v)) });
                        cmdList.emplace_back(bredis::single_command_t{ "EXPIRE", std::get<0>(v), EXPIRE_TIME_STR });
                }
                act->RedisCmd(std::move(cmdList));

                return true;
        }

private :
	FORCE_INLINE static uint64_t GenReqID() { static uint64_t id=-1; return ++id; }

#endif

DECLARE_SERVICE_BASE_END(MySql);

#ifdef MYSQL_SERVICE_SERVER
typedef MySqlServiceBase<E_ServiceType_Server, stServerInfoBase> MySqlService;
#endif

#ifdef MYSQL_SERVICE_CLIENT
typedef MySqlServiceBase<E_ServiceType_Client, stDBServerInfo> MySqlService;
#endif

#ifdef MYSQL_SERVICE_SERVER

class DBReqWapper
{
public :
        DBReqWapper(const std::shared_ptr<MySqlService::SessionType>& ses
                    , const MySqlService::SessionType::MsgHeaderType& msgHead
                    , const std::shared_ptr<MailReqDBDataList>& msg)
                : _ses(ses), _msgHead(msgHead), _msg(msg)
        {
        }

        virtual ~DBReqWapper()
        {
                auto ses = _ses.lock();
                if (ses)
                {
                        ses->SendPB(_msg, E_MIMT_DB, E_MIDBST_ReqDBData,
                                    MySqlService::SessionType::MsgHeaderType::E_RMT_CallRet,
                                    _msgHead._guid, _msgHead._to, _msgHead._from);
                }
        }

        std::weak_ptr<MySqlService::SessionType> _ses;
        const MySqlService::SessionType::MsgHeaderType _msgHead;
        std::shared_ptr<MailReqDBDataList> _msg;
};

class MySqlActor;
class MySqlExecActor;
class IMySqlTask
{
public :
        IMySqlTask(EMySqlTaskType tt)
                : _taskType(tt)
        {
        }

        virtual bool DealFromCache(const std::shared_ptr<MySqlActor>&) { return false; }
        virtual void Exec(const std::shared_ptr<MySqlActor>& act
                          , const std::shared_ptr<MySqlExecActor>& execAct
                          , const std::unordered_map<uint64_t, std::shared_ptr<IMySqlTask>>& reqList) { }

        std::shared_ptr<void> _ref;
        MailReqDBData* _pb = nullptr;
        std::string _data;
        int64_t _sid = 0;
        const EMySqlTaskType _taskType = static_cast<EMySqlTaskType>(9999);
        time_t _reqTime = 0;
};
typedef std::shared_ptr<IMySqlTask> IMySqlTaskPtr;

class MySqlVersionTask : public IMySqlTask
{
public :
        MySqlVersionTask() : IMySqlTask(E_MySql_TT_Version) { }

        bool DealFromCache(const std::shared_ptr<MySqlActor>& act) override;
        void Exec(const std::shared_ptr<MySqlActor>& act
                  , const std::shared_ptr<MySqlExecActor>& execAct
                  , const std::unordered_map<uint64_t, std::shared_ptr<IMySqlTask>>& reqList) override;
};

class MySqlLoadTask : public IMySqlTask
{
public :
        MySqlLoadTask() : IMySqlTask(E_MySql_TT_Load) { }

        bool DealFromCache(const std::shared_ptr<MySqlActor>& act) override;
        void Exec(const std::shared_ptr<MySqlActor>& act
                  , const std::shared_ptr<MySqlExecActor>& execAct
                  , const std::unordered_map<uint64_t, std::shared_ptr<IMySqlTask>>& reqList) override;
};

class MySqlSaveTask : public IMySqlTask
{
public :
        MySqlSaveTask() : IMySqlTask(E_MySql_TT_Save) { }

        bool DealFromCache(const std::shared_ptr<MySqlActor>& act) override;
        void Exec(const std::shared_ptr<MySqlActor>& act
                  , const std::shared_ptr<MySqlExecActor>& execAct
                  , const std::unordered_map<uint64_t, std::shared_ptr<IMySqlTask>>& reqList) override;
};

#endif

// vim: fenc=utf8:expandtab:ts=8
