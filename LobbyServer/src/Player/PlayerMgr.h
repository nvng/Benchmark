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
        std::shared_ptr<PlayerBase> CreatePlayer(GUID_TYPE id, const MsgClientLogin& msg) override;

public :
	bool ReadLevelUpCfg();
};
