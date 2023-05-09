#pragma once

#include "PlayerMgrBase.h"

class PlayerMgr : public nl::af::impl::PlayerMgrBase, public Singleton<PlayerMgr>
{
	PlayerMgr();
	~PlayerMgr() override;
	friend class Singleton<PlayerMgr>;

	typedef nl::af::impl::PlayerMgrBase SuperType;
public :
	bool Init() override;
	nl::af::impl::PlayerBasePtr CreatePlayer(uint64_t id, const std::string& nickName, const std::string& icon);

public :
	bool ReadLevelUpCfg();
};
