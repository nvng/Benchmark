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
	PlayerBasePtr CreatePlayer(uint64_t id, const std::string& nickName, const std::string& icon);

public :
	bool ReadLevelUpCfg();
};
