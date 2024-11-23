#pragma once

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

struct stReqDBDataVersion : public stActorMailBase
{
        std::shared_ptr<MsgDBDataVersion> _pb;
        int64_t _sid = 0;
        time_t _reqTime = 0;
        virtual ISessionPtr GetSession() = 0;
        virtual void CallRet() = 0;
};
typedef std::shared_ptr<stReqDBDataVersion> stReqDBDataVersionPtr;

template <typename _Sy>
struct stReqDBDataVersionImpl : public stReqDBDataVersion
{
        ISessionPtr GetSession() override { return _ses.lock(); }

        typename _Sy::MsgHeaderType _msgHead;
        std::weak_ptr<_Sy> _ses;
        void CallRet() override
        {
                auto ses = _ses.lock();
                if (ses)
                {
                        ses->SendPB(_pb, E_MIMT_DB, E_MIDBST_DBDataVersion,
                                    _Sy::MsgHeaderType::E_RMT_CallRet,
                                    _msgHead._guid, _msgHead._to, _msgHead._from);
                }
        }
};

struct stDBData : public stActorMailBase
{
        std::shared_ptr<MsgDBData> _pb;
        int64_t _sid = 0;
        time_t _reqTime = 0;
        std::shared_ptr<void> _bufRef;
        std::string_view _buf;

        virtual void CallRet() = 0;
};
typedef std::shared_ptr<stDBData> stDBDataPtr;

template <typename _Sy>
struct stDBDataImpl : public stDBData
{
        typename _Sy::MsgHeaderType _msgHead;
        std::weak_ptr<_Sy> _ses;

        void CallRet() override
        {
                auto ses = _ses.lock();
                if (ses)
                {
                        ses->SendPBExtra(_pb, _bufRef, _buf.data(), _buf.length(),
                                         _msgHead.MainType(), _msgHead.SubType(),
                                         _Sy::MsgHeaderType::E_RMT_CallRet,
                                         _msgHead._guid, _msgHead._to, _msgHead._from);
                }
        }
};

struct stDBDealList : public stActorMailBase
{
        std::unordered_map<int64_t, stDBDataPtr> _list;
        std::unordered_map<int64_t, stReqDBDataVersionPtr> _reqList;
        std::vector<stVersionInfoPtr> _versionList;
};

class MySqlActor;
SPECIAL_ACTOR_DEFINE_BEGIN(DBDataSaveActor, 0xefe);

public :
        DBDataSaveActor(int64_t idx) : _idx(idx) { }

        bool Init() override;
        void InitDealTimer();

        void DealSave();
        void DealLoad();
        void DealLoadVersion();

public :
        SteadyTimer _timer;
        std::weak_ptr<MySqlActor> _dbMgrActor;
        const int64_t _idx = -1;

SPECIAL_ACTOR_DEFINE_END(DBDataSaveActor);

#endif

SPECIAL_ACTOR_DEFINE_BEGIN(MySqlActor, 0xeff);

#ifdef MYSQL_SERVICE_SERVER

public :
        explicit MySqlActor(int64_t idx)
                : _saveActor(std::make_shared<DBDataSaveActor>(0))
                  , _loadActor(std::make_shared<DBDataSaveActor>(1))
                  , _loadVersionActor(std::make_shared<DBDataSaveActor>(2))
                  , _idx(idx)
        {
        }

        bool Init() override;

        FORCE_INLINE int64_t GetIdx() const { return _idx; }
        void InitDealTimer();
        void InitDelVersionTimer();
        void InitTerminateTimer();

        void LoadVersion();
        bool DealReqDBDataVersion();

        bool DealLoadDBData();
        bool DealSaveDBData();

        void WaitForTerminate() override;

public :
        SteadyTimer _timer;
        SteadyTimer _delVertionTimer;
        SteadyTimer _terminateTimer;

        DBDataSaveActorPtr _saveActor;
        DBDataSaveActorPtr _loadActor;
        DBDataSaveActorPtr _loadVersionActor;
        static std::atomic_uint64_t scMaxAllowedPackageSize;
	VersionInfoListType _versionInfoList;

        std::unordered_map<int64_t, stReqDBDataVersionPtr> _reqList;
        std::unordered_map<int64_t, stDBDataPtr> _saveList;
        decltype(_saveList) _saveCacheList;
        std::unordered_map<int64_t, stDBDataPtr> _loadList;

        SpinLock _reqVersionCacheListMutex;
        decltype(_reqList) _reqVersionCacheList;

        SpinLock _reqSaveCacheListMutex;
        std::map<std::pair<int64_t, int64_t>, stDBDataPtr> _reqSaveCacheList;

        SpinLock _loadCacheListMutex;
        decltype(_loadList) _loadCacheList;

private :
        const int64_t _idx = 0;

#endif

SPECIAL_ACTOR_DEFINE_END(MySqlActor);

DECLARE_SERVICE_BASE_BEGIN(MySql, SessionDistributeMod, ServiceSession);

public :
        bool Init();

#ifdef MYSQL_SERVICE_SERVER

private :
        MySqlServiceBase() { MySqlMgr::CreateInstance(); }
        ~MySqlServiceBase() override { MySqlMgr::DestroyInstance(); }

public :
        FORCE_INLINE std::shared_ptr<MySqlActor> GetMgrActor(uint64_t id)
        { return _dbMgrActorArr[id & scMgrActorCnt]; }
public :
        constexpr static int64_t scSqlStrInitSize = 1024 * 1024;
private :
        friend class MySqlActor;
        constexpr static int64_t scMgrActorCnt = (1 << 6) - 1;
        std::shared_ptr<MySqlActor> _dbMgrActorArr[scMgrActorCnt + 1];

public :
        void Terminate();
        void WaitForTerminate();

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

        template <typename _Ay, typename _Dy>
        EMySqlServiceStatus Load(const std::shared_ptr<_Ay>& act, _Dy& dbInfo, std::string_view prefix)
        {
                auto ses = SuperType::DistSession(act->GetID());
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
                msg->set_guid(act->GetID());
                auto versionRet = Call(MsgDBDataVersion, act, agent, E_MIMT_DB, E_MIDBST_DBDataVersion, msg);
                if (!versionRet)
                {
                        LOG_INFO("玩家[{}] 请求数据版本超时!!!", act->GetID());
                        return E_MySqlS_Fail;
                }

                auto parseDataFunc = [&dbInfo](std::string_view data) {
                        if (Compress::UnCompressAndParseAlloc<Compress::E_CT_Zstd>(dbInfo, data))
                                return E_MySqlS_Success;
                        else
                                return E_MySqlS_Fail;
                };

                // TODO: 不同类型 actor id 重复。
                auto loadFromMysqlFunc = [act, agent, &dbInfo, &versionRet, &parseDataFunc]() {
                        auto loadDBData = std::make_shared<MsgDBData>();
                        loadDBData->set_guid(act->GetID());
                        auto loadMailRet = act->CallInternal(agent, E_MIMT_DB, E_MIDBST_LoadDBData, loadDBData);
                        if (!loadMailRet)
                        {
                                LOG_WARN("玩家[{}] load from mysql 时，超时!!!", act->GetID());
                                return E_MySqlS_Fail;
                        }

                        MsgDBData loadRet;
                        auto [bufRef, buf, ret] = loadMailRet->ParseExtra(loadRet);
                        if (!ret)
                        {
                                LOG_WARN("玩家[{}] load from mysql 时，解析出错!!!", act->GetID());
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
                                        auto status = parseDataFunc(buf);
                                        LOG_WARN_IF(versionRet->version() != dbInfo.version(), "玩家[{}] 从 MySql 获取数据 size:{}，版本不匹配!!! v:{} dbv:{}",
                                                    act->GetID(), buf.length(), versionRet->version(), dbInfo.version());
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

                std::string cmd = fmt::format("{}:{}", prefix, act->GetID());
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
                                         act->GetID(), versionRet->version());
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
                                                         act->GetID(), versionRet->version(), dbInfo.version());
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
        bool Save(const std::shared_ptr<_Ay>& act, _Dy& dbInfo, std::string_view prefix, bool isDelete)
        {
                auto ses = SuperType::DistSession(act->GetID());
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
                saveDBData->set_guid(act->GetID());
                saveDBData->set_version(dbInfo.version());
                auto [base64DataRef, base64DataSize] = Base64EncodeExtra(bufRef.get(), bufSize);
                // saveDBData->set_data(base64Data);
                /* 有问题。
                   saveDBData->set_allocated_data(&base64Data);
                   */

                auto saveRet = ParseMailData<MsgDBData>(act->CallInternal(agent, E_MIMT_DB, E_MIDBST_SaveDBData, saveDBData, base64DataRef, base64DataRef.get(), base64DataSize).get(), E_MIMT_DB, E_MIDBST_SaveDBData);
                if (!saveRet)
                {
                        LOG_WARN("玩家[{}] 存储到 DBServer 超时!!!", act->GetID());
                        return false;
                }

                std::string key = fmt::format("{}:{}", prefix, act->GetID());
                act->RedisCmd("SET", key, std::string_view(bufRef.get(), bufSize));
                act->RedisCmd("EXPIRE", key, fmt::format_int(7*24*3600).c_str());

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

// vim: fenc=utf8:expandtab:ts=8
