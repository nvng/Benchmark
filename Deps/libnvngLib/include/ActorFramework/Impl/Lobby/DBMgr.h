#pragma once

namespace nl::af::impl {

class PlayerBase;
typedef std::shared_ptr<PlayerBase> PlayerBasePtr;

class LobbyDBSession;
class DBMgr : public Singleton<DBMgr>
{
	DBMgr() { }
	~DBMgr() { }
	friend class Singleton<DBMgr>;

public :
	bool Init();
	bool LoadPlayer(const PlayerBasePtr& p, const std::shared_ptr<MsgClientLogin>& loginMsg);
	bool SavePlayer(const PlayerBasePtr& p, bool isDelete);
	FORCE_INLINE static uint64_t GenReqID() { static uint64_t id=-1; return ++id; }

public :
	bool AddSession(const std::shared_ptr<LobbyDBSession>& ses);
	std::shared_ptr<LobbyDBSession> RemoveSession(int64_t sid);
	FORCE_INLINE std::shared_ptr<LobbyDBSession> DistSession(int64_t id)
	{ return _dbSesArr[id & (_dbSesArr.size()-1)].lock(); }

private :
	friend class LobbyDBSession;
	std::vector<std::weak_ptr<LobbyDBSession>> _dbSesArr;
};

}; // end of namespace nl::af::impl;

// vim: fenc=utf8:expandtab:ts=8
