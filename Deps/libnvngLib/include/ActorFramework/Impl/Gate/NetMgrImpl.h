#pragma once

#include "GateLobbySession.h"
#include "GateLoginSession.h"
#include "GateGameSession.h"

#include "Tools/ServerList.hpp"

namespace nl::af::impl {

enum ESessionType
{
	E_GST_None = 0,
	E_GST_Client = 1,
	E_GST_Login = 2,
	E_GST_Lobby = 3,
	E_GST_Game = 4,
};

struct stSesDistData
{
	stSesDistData() {}
	explicit stSesDistData(const std::shared_ptr<ISession>& ses)
		: _ses(ses)
	{
	}

	std::string GetKey() const
	{
		auto ses = _ses.lock();
		if (ses)
		{
			return fmt::format("{}", ses->GetID());
		}
		else
		{
			LOG_ERROR("GetKey ses nullptr!!!");
			return std::string();
		}
	}

	ISessionWeakPtr _ses;

	DISABLE_COPY_AND_ASSIGN(stSesDistData);
};

class NetMgrImpl : public Singleton<NetMgrImpl>
{
	NetMgrImpl() : _sesList("NetMgrImpl") { }
	~NetMgrImpl() { }
	friend class Singleton<NetMgrImpl>;

public :
	bool Init()
	{
		_lobbySesList.resize(ServerListCfgMgr::GetInstance()->GetSize<stLobbyServerInfo>());
		_gameSesList.resize(ServerListCfgMgr::GetInstance()->GetSize<stGameServerInfo>());
		return true;
	}

	bool AddSession(ESessionType sesType, const std::shared_ptr<ISession>& ses)
	{
		if (!ses || E_GST_None == sesType)
			return false;

		if (!_sesList.Add(ses->GetID(), ses))
			return false;

		switch (sesType)
		{
		case E_GST_Login :
			{
				auto s = std::dynamic_pointer_cast<GateLoginSession>(ses);
				s->_hashKey = _LoginConsistencyHash.AddNode(1, ses);
			}
			break;
		case E_GST_Lobby :
			{
                                auto s = std::dynamic_pointer_cast<GateLobbySession>(ses);
                                const int64_t idx = ServerListCfgMgr::GetInstance()->CalSidIdx<stLobbyServerInfo>(s->GetSID());
                                if (INVALID_SERVER_IDX != idx)
                                        _lobbySesList[idx] = s;
                                else
                                        LOG_WARN("AddSession 时，sid[{}] 获取idx[{}]出错!!!", s->GetSID(), idx);
			}
			break;
		case E_GST_Game :
			{
                                auto s = std::dynamic_pointer_cast<GateGameSession>(ses);
                                const int64_t idx = ServerListCfgMgr::GetInstance()->CalSidIdx<stGameServerInfo>(s->GetSID());
                                if (INVALID_SERVER_IDX != idx)
                                        _gameSesList[idx] = s;
                                else
                                        LOG_WARN("AddSession 时，sid[{}] 获取idx[{}]出错!!!", s->GetSID(), idx);
			}
			break;
		default:
			break;
		}
		return true;
	}

	FORCE_INLINE ISessionPtr GetSession(uint64_t id) { return _sesList.Get(id).lock(); }

	bool RemoveSession(ESessionType sesType, ISession* ses)
	{
		if (nullptr == ses)
			return false;

		auto ret = _sesList.Remove(ses->GetID(), ses).lock();
		if (!ret)
			return false;

		switch (sesType)
		{
		case E_GST_Login :
			{
				auto s = dynamic_cast<GateLoginSession*>(ses);
				_LoginConsistencyHash.RemoveNode(s->_hashKey);
			}
			break;
		case E_GST_Lobby :
			{
				auto s = dynamic_cast<GateLobbySession*>(ses);
				const int64_t idx = ServerListCfgMgr::GetInstance()->CalSidIdx<stLobbyServerInfo>(s->GetSID());
                                if (INVALID_SERVER_IDX != idx)
                                        _lobbySesList[idx].reset();
                                else
                                        LOG_WARN("RemoveSession 时，sid[{}] 获取idx[{}]出错!!!", s->GetSID(), idx);
			}
			break;
		case E_GST_Game :
			{
				auto s = dynamic_cast<GateGameSession*>(ses);
				const int64_t idx = ServerListCfgMgr::GetInstance()->CalSidIdx<stGameServerInfo>(s->GetSID());
                                if (INVALID_SERVER_IDX != idx)
                                        _gameSesList[idx].reset();
                                else
                                        LOG_WARN("RemoveSession 时，sid[{}] 获取idx[{}]出错!!!", s->GetSID(), idx);
			}
			break;
		default:
			break;
		}

		return true;
	}

	FORCE_INLINE std::size_t GetSessionCnt() { return _sesList.Size(); }
	void ForeachLobby(const auto& cb)
	{
		for (auto& w : _lobbySesList)
		{
			auto ses = w.lock();
			if (ses)
				cb(ses);
		}
	}

	void ForeachGame(const auto& cb)
	{
		for (auto& w : _gameSesList)
		{
			auto ses = w.lock();
			if (ses)
				cb(ses);
		}
	}

	std::shared_ptr<GateLobbySession> DistLobbySession(const uint64_t id)
	{
		// 尽量与其它地方分配方式错开。
		const auto idx = id >> 8;
		return _lobbySesList[idx & (_lobbySesList.size()-1)].lock();
	}

	std::shared_ptr<GateLoginSession> DistLoginSession(uint64_t id)
	{
		const stSesDistData& data = _LoginConsistencyHash.GetNodeByKey(id);
		return std::dynamic_pointer_cast<GateLoginSession>(data._ses.lock());
	}

	FORCE_INLINE std::shared_ptr<GateGameSession> DistGameSession(uint64_t sid, uint64_t id)
	{
		const int64_t idx = ServerListCfgMgr::GetInstance()->CalSidIdx<stGameServerInfo>(sid);
                if (INVALID_SERVER_IDX == idx)
                        return nullptr;
		return _gameSesList[idx].lock();
	}

	template <typename _Fy>
	FORCE_INLINE void Foreach(const _Fy& cb)
	{
		_sesList.Foreach([cb](const auto& s) {
                        auto ses = s.lock();
                        if (ses)
                                cb(ses);
		});
	}

private :
	ThreadSafeUnorderedMap<uint64_t, ISessionWeakPtr> _sesList;

	ConsistencyHash<stSesDistData> _LoginConsistencyHash;

	std::vector<std::weak_ptr<GateLobbySession>> _lobbySesList;
	std::vector<std::weak_ptr<GateGameSession>> _gameSesList;
};

}; // end of namespace nl::af::impl
