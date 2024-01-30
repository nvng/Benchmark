#pragma once

#include "msg_db.pb.h"
#include "DBLobbySession.h"

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

struct stGenGuidItem
{
        uint64_t _idx = 0;
        uint64_t _cur = 0;

        void Pack(auto& msg)
        {
                msg.set_idx(_idx);
                msg.set_cur(_cur);
        }

        void UnPack(const auto& msg)
        {
                _idx = msg.idx();
                _cur = msg.cur();
        }
};
typedef std::shared_ptr<stGenGuidItem> stGenGuidItemPtr;

SPECIAL_ACTOR_DEFINE_BEGIN(DBGenGuidActor, 0xefd);

public :
        DBGenGuidActor(int64_t idx, uint64_t minGuid, uint64_t maxGuid)
                : _idx(idx)
                  , _minGuid(minGuid)
                  , _maxGuid(maxGuid)
                  , _step((maxGuid - minGuid + 1) / scInitArrSize)
        {
                for (int64_t i=0; i<scInitArrSize; ++i)
                {
                        auto item = std::make_shared<stGenGuidItem>();
                        item->_idx = i;
                        item->_cur = std::min(_minGuid + i*_step, _maxGuid);
                        if (item->_cur < _maxGuid)
                                _itemList.emplace_back(item);
                }
        }

        const int64_t _idx = 0;
        const uint64_t _minGuid = 0;
        const uint64_t _maxGuid = 0;
        const uint64_t _step = 0;
        constexpr static int64_t scInitArrSize = 10000;
        std::vector<stGenGuidItemPtr> _itemList;

SPECIAL_ACTOR_DEFINE_END(DBGenGuidActor);

class DBMgrActor;
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
        std::weak_ptr<DBMgrActor> _dbMgrActor;
        const int64_t _idx = -1;

SPECIAL_ACTOR_DEFINE_END(DBDataSaveActor);

SPECIAL_ACTOR_DEFINE_BEGIN(DBMgrActor, 0xeff);

public :
        explicit DBMgrActor(int64_t idx)
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

SPECIAL_ACTOR_DEFINE_END(DBMgrActor);

class DBMgr : public Singleton<DBMgr>
{
        DBMgr();
        ~DBMgr();
        friend class Singleton<DBMgr>;

public :
        bool Init();

public :
        FORCE_INLINE std::shared_ptr<DBMgrActor> GetMgrActor(uint64_t id)
        { return _dbMgrActorArr[id & scMgrActorCnt]; }

        DBGenGuidActorPtr _genGuidActor;
public :
        constexpr static int64_t scSqlStrInitSize = 1024 * 1024;
private :
        friend class DBMgrActor;
        constexpr static int64_t scMgrActorCnt = (1 << 6) - 1;
        std::shared_ptr<DBMgrActor> _dbMgrActorArr[scMgrActorCnt + 1];

public :
        void Terminate();
        void WaitForTerminate();
};

// vim: fenc=utf8:expandtab:ts=8
