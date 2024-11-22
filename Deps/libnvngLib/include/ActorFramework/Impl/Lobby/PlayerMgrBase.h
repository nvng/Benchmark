#pragma once

#include "ActorFramework/SpecialActor.hpp"
#include "LobbyGateSession.h"

namespace nl::af::impl {

class PlayerBase;
typedef std::shared_ptr<PlayerBase> PlayerBasePtr;

SPECIAL_ACTOR_DEFINE_BEGIN(PlayerMgrActor, 0xeff);
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

struct stRefresh : public stActorMailBase
{
	std::vector<IActorWeakPtr> _actList;
	int64_t _subType = 0;
	int64_t _param = 0;
};

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

	virtual PlayerBasePtr CreatePlayer(uint64_t id, const std::string& nickName, const std::string& icon) { return nullptr; }
	
	FORCE_INLINE PlayerMgrActorPtr GetPlayerMgrActor(int64_t id) const
	{ return _playerMgrActorArr[id & _playerMgrActorArrSize]; }
private :
	friend class PlayerMgrActor;
	friend class PlayerBase;
	friend class LobbyGateSession;
        PlayerMgrActorPtr* _playerMgrActorArr = nullptr;
        const int64_t _playerMgrActorArrSize = (1 << 3) - 1;
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

}; // end of namespace nl::af::impl

// vim: fenc=utf8:expandtab:ts=8:noma
