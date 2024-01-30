#pragma once

#include "GateClientSession.h"

namespace nl::af::impl {

class GateLobbySession;
class GateGameSession;

#ifdef ____BENCHMARK____
enum EGatePlayerFlagType
{
        E_GPFT_Delete = 1 << 0,
};
#endif

typedef std::shared_ptr<::google::protobuf::MessageLite> MessageLitePtr;

class Player
{
public :
	Player(uint64_t id, const std::shared_ptr<GateClientSession>& clientSes);
	~Player();

	void SetLobbySes(const std::shared_ptr<GateLobbySession>& ses) { _lobbySes = ses; }
	void ResetLobbySes() { _lobbySes.reset(); }

	void OnLobbyClose();
        void Send2Lobby(const ISession::BuffTypePtr& bufRef,
                        ISession::BuffTypePtr::element_type* buf,
                        std::size_t bufSize,
                        Compress::ECompressType ct,
                        uint64_t mainType,
                        uint64_t subType);
	void Send2Lobby(const MessageLitePtr& msg, uint64_t mainType, uint64_t subType);

	void OnGameClose();
	void Send2Game(const ISession::BuffTypePtr& bufRef,
                       ISession::BuffTypePtr::element_type* buf,
                       std::size_t bufSize,
                       Compress::ECompressType ct,
                       uint64_t mainType,
                       uint64_t subType);

	void OnClientClose();
	void Send2Client(const MessageLitePtr& msg, uint64_t mainType, uint64_t subType);

        template <typename _Hy>
	void Send2Client(const ISession::BuffTypePtr& bufRef,
                         ISession::BuffTypePtr::element_type* buf,
                         std::size_t bufSize,
                         Compress::ECompressType ct,
                         uint64_t mt,
                         uint64_t st)
        {
                auto ses = _clientSes.lock();
                if (ses)
                        ses->SendBuf<_Hy>(bufRef, buf, bufSize, ct, mt, st, 0);
        }

	void ClearSessionRelation();

public :
	std::weak_ptr<GateClientSession> _clientSes;
	std::weak_ptr<GateLobbySession> _lobbySes;
	std::weak_ptr<GateGameSession> _gameSes;
	uint64_t _regionGuid = 0;
	static std::atomic_int64_t _playerFlag;
#ifdef ____BENCHMARK____
        std::atomic_uint64_t _internalFlag = 0;
#endif

public :
	FORCE_INLINE uint64_t GetID() const { return _id; }
	FORCE_INLINE int64_t GetUniqueID() const { return _uniqueID; }

private :
	const int64_t _uniqueID = 0;
	const uint64_t _id = 0;
};

typedef std::shared_ptr<Player> PlayerPtr;
typedef std::weak_ptr<Player> PlayerWeakPtr;

class PlayerMgr : public Singleton<PlayerMgr>
{
	PlayerMgr() : _playerList("PlayerMgr"), _playerLoginList("PlayerMgr") {}
	~PlayerMgr() {}
	friend class Singleton<PlayerMgr>;
public :
	bool Init() { return true; }
	static FORCE_INLINE uint64_t GenUniqueID() { static std::atomic_uint32_t _g = 0; return ++_g; }

public :
	FORCE_INLINE bool AddPlayer(const PlayerPtr& p) { if (!p) return false; return _playerList.Add(p->GetID(), p); }
	FORCE_INLINE PlayerPtr GetPlayer(int64_t uniqueID, uint64_t id)
	{
		auto tp = _playerList.Get(id);
                LOG_WARN_IF(tp && tp->GetUniqueID() != uniqueID,
                            "获取玩家[{}]时，GetUniqueID[{}] uniqueID[{}]",
                            id, tp->GetUniqueID(), uniqueID);
		return tp && tp->GetUniqueID() == uniqueID ? tp : nullptr;
	}
	FORCE_INLINE PlayerPtr RemovePlayer(uint64_t id, void* ptr)
	{
		auto tp = _playerList.Remove(id, ptr);
		if (tp)
			tp->ClearSessionRelation();
		return tp;
	}
	template <typename _Fy>
	FORCE_INLINE void Foreach(const _Fy& cb) { _playerList.Foreach(cb); }
	FORCE_INLINE int64_t GetPlayerCnt() { return _playerList.Size(); }

	FORCE_INLINE bool AddLoginPlayer(const PlayerPtr& p) { if (!p) return false; return _playerLoginList.Add(p->GetID(), p); }
	FORCE_INLINE PlayerPtr GetLoginPlayer(int64_t uniqueID, uint64_t id)
	{
		auto tp = _playerLoginList.Get(id);
		return tp && tp->GetUniqueID() == uniqueID ? tp : nullptr;
	}

	FORCE_INLINE PlayerPtr RemoveLoginPlayer(uint64_t id, void* ptr)
	{ return _playerLoginList.Remove(id, ptr); }
	FORCE_INLINE int64_t GetLoginPlayerCnt() { return _playerLoginList.Size(); }

private :
	ThreadSafeUnorderedMap<uint64_t, PlayerPtr> _playerList;
	ThreadSafeUnorderedMap<uint64_t, PlayerPtr> _playerLoginList;
};

}; // end of namespace nl::af::impl
