#pragma once

#include "PlayerMgrBase.h"

class PlayerMgr : public PlayerMgrBase, public Singleton<PlayerMgr>
{
	PlayerMgr();
	~PlayerMgr() override;
	friend class Singleton<PlayerMgr>;

	typedef PlayerMgrBase SuperType;
public :
	bool Init() override;
        std::shared_ptr<PlayerBase> CreatePlayer(GUID_TYPE id, const std::string& nickName, const std::string& icon) override;

public :
	bool ReadLevelUpCfg();

public :
        static constexpr int64_t scPlayerOfflineDataActorArrSize = 1;
        PlayerOfflineDataActorPtr _playerOfflineDataActorArr[scPlayerOfflineDataActorArrSize];
};
