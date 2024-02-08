#pragma once

#include "ActorFramework/SpecialActor.hpp"
#include "LobbyGateSession.h"

namespace nl::af::impl {

SPECIAL_ACTOR_DEFINE_BEGIN(PlayerMgrActor);
public :
        PlayerMgrActor() : SuperType(SpecialActorMgr::GenActorID(), IActor::scMailQueueMaxSize) { }
SPECIAL_ACTOR_DEFINE_END(PlayerMgrActor);

struct stLoginInfo : public stActorMailBase
{
	std::weak_ptr<LobbyGateSession> _ses;
	LobbyGateSession::ActorAgentTypePtr _clientAgent;
	uint32_t _from = 0;
        std::shared_ptr<MsgClientLogin> _pb;
};

struct stDisconnectInfo : public stActorMailBase
{
        uint64_t _uniqueID = 0;
        uint64_t _to = 0;
};

// {{{ PlayerOfflineDataActor

namespace PlayerOfflineData {
struct by_id;
struct by_over_time;
};

typedef boost::multi_index::multi_index_container<
	std::shared_ptr<MsgOfflineOpt>,
	boost::multi_index::indexed_by <

        boost::multi_index::hashed_unique<
        boost::multi_index::tag<PlayerOfflineData::by_id>,
        BOOST_MULTI_INDEX_CONST_MEM_FUN(MsgOfflineOpt, int64_t, guid)
        >,

        boost::multi_index::ordered_non_unique<
        boost::multi_index::tag<PlayerOfflineData::by_over_time>,
        BOOST_MULTI_INDEX_CONST_MEM_FUN(MsgOfflineOpt, int64_t, over_time)
        >

	>
> PlayerOfflineDataListType;

struct stMailPlayerOfflineData : public stActorMailBase
{
        int64_t _guid = 0;
        int64_t _mt = 0;
        int64_t _st = 0;
        std::string _data;
};

SPECIAL_ACTOR_DEFINE_BEGIN(PlayerOfflineDataActor, E_MIMT_Offline);

public :
        PlayerOfflineDataActor() : SuperType(SpecialActorMgr::GenActorID(), IActor::scMailQueueMaxSize) { }
        bool Init() override;
        void Flush2DB(bool isDelete = false);
        void InitFlush2DBTimer();
        void DealGet();
        void InitGetTimer();

        FORCE_INLINE void AddOfflineData(int64_t guid, int64_t mt, int64_t st, const std::string& data)
        {
                auto mail = std::make_shared<stMailPlayerOfflineData>();
                mail->_guid = MySqlMgr::GenDataKey(E_MIMT_Offline, guid);
                mail->_mt = mt;
                mail->_st = st;
                mail->_data = std::move(data);
                SendPush(nullptr, E_MIOST_Append, mail);
        }

        FORCE_INLINE void AddOfflineData(int64_t guid, int64_t mt, int64_t st, google::protobuf::MessageLite& msg)
        { AddOfflineData(guid, mt, st, msg.SerializeAsString()); }

        template <typename _Ty>
        std::shared_ptr<MsgOfflineOpt> GetOfflineData(const std::shared_ptr<_Ty>& p)
        {
                auto mail = std::make_shared<stMailPlayerOfflineData>();
                mail->_guid = p->GetID();
                return Call(MsgOfflineOpt, p, shared_from_this(), E_MIMT_Offline, E_MIOST_Get, mail);
        }

        void Terminate() override;

        ::nl::util::SteadyTimer _flush2DBTimer;
        bool _inGetTimer = false;
        ::nl::util::SteadyTimer _getTimer;
        PlayerOfflineDataListType _dataList;
        std::unordered_map<uint64_t, IActorWeakPtr> _getList;

SPECIAL_ACTOR_DEFINE_END(PlayerOfflineDataActor);

// }}}

// {{{ PlayermgrBase

class PlayerBase;
class PlayerMgrBase;
extern PlayerMgrBase* GetPlayerMgrBase();

struct stPlayerLevelInfo
{
        int64_t _lv = 0;
        int64_t _exp = 0;
};
typedef std::shared_ptr<stPlayerLevelInfo> stPlayerLevelInfoPtr;

class PlayerMgrBase : public ServiceImpl
{
	typedef ServiceImpl SuperType;
public :
        PlayerMgrBase();
        ~PlayerMgrBase() override;

	bool Init() override;
        FORCE_INLINE static PlayerMgrBase* GetService() { return GetPlayerMgrBase(); }

        void OnDayChange();
        void OnDataReset(int64_t h);

private :
        void DealReset(EMsgInternalLocalSubType t, int64_t param);

public :
	void Terminate() override;
	void WaitForTerminate() override;

	virtual std::shared_ptr<PlayerBase> CreatePlayer(uint64_t id, const std::string& nickName, const std::string& icon) { return nullptr; }
	
	FORCE_INLINE PlayerMgrActorPtr GetPlayerMgrActor(int64_t id) const
	{ return _playerMgrActorArr[id & _playerMgrActorArrSize]; }
private :
	friend class PlayerMgrActor;
	friend class PlayerBase;
	friend class LobbyGateSession;
        PlayerMgrActorPtr* _playerMgrActorArr = nullptr;
        int64_t _playerMgrActorArrSize = (1 << 3) - 1;

public :
        FORCE_INLINE void AddPlayerOfflineData(uint64_t guid, int64_t mt, int64_t st, google::protobuf::MessageLite& msg)
        { auto act = GetPlayerOfflineDataActor(guid); if (act) act->AddOfflineData(guid, mt, st, msg); }
        template <typename _Ay>
        FORCE_INLINE std::shared_ptr<MsgOfflineOpt> GetPlayerOfflineData(const std::shared_ptr<_Ay>& p)
        {
                if (p)
                {
                        auto act = GetPlayerOfflineDataActor(p->GetID());
                        if (act)
                                return act->GetOfflineData(p);
                }
                return std::make_shared<MsgOfflineOpt>();
        }

        FORCE_INLINE PlayerOfflineDataActorPtr GetPlayerOfflineDataActor(uint64_t guid)
        { return _playerOfflineDataActorArr[guid & _playerMgrActorArrSize]; }
private :
        PlayerOfflineDataActorPtr* _playerOfflineDataActorArr = nullptr;

private :
	ThreadSafeUnorderedMap<uint64_t, std::shared_ptr<stLoginInfo>> _loginInfoList;

public :
	FORCE_INLINE stPlayerLevelInfoPtr GetLevelCfg(int64_t lv) const
	{ return 0<lv && lv<=_lvExpArrMax ? _lvExpArr[lv-1] : nullptr; }
	FORCE_INLINE int64_t GetExpByLV(int64_t lv) const
	{ return 0<lv && lv<=_lvExpArrMax ? _lvExpArr[lv-1]->_exp : INT64_MAX; }
protected :
	stPlayerLevelInfoPtr* _lvExpArr = nullptr;
	int32_t _lvExpArrMax = 0;
};

// }}}

}; // end of namespace nl::af::impl

// vim: fenc=utf8:expandtab:ts=8:noma
