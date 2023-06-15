#pragma once

class ClientGateSession;

class Player;
typedef std::shared_ptr<Player> PlayerPtr;

class PlayerMgr : public ServiceImpl, public Singleton<PlayerMgr>
{
	PlayerMgr();
	~PlayerMgr() override;
	friend class Singleton<PlayerMgr>;

	typedef ServiceImpl SuperType;
public :
	bool Init() override;

        FORCE_INLINE static IService* GetService() { return PlayerMgr::GetInstance(); }
	// FORCE_INLINE PlayerPtr GetPlayer(uint64_t guid) { return std::dynamic_pointer_cast<Player>(GetActor(guid)); }

	std::vector<uint64_t> _idList;
public :
	FORCE_INLINE std::shared_ptr<ClientGateSession> DistSes() { return _sesList[_idx++ % _sesList.size()];  }
private :
	std::atomic_int64_t _idx = 0;
	std::vector<std::shared_ptr<ClientGateSession>> _sesList;
};

// vim: fenc=utf8:expandtab:ts=8
