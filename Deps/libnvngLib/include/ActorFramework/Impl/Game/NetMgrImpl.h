#pragma once

class GameGateSession;
class GameLobbySession;
class NetMgrImpl : public Singleton<NetMgrImpl>
{
        NetMgrImpl() : _gateSesList("NetMgr_gateSesList"), _lobbySesList("NetMgr_lobbySesList") { }
        ~NetMgrImpl() { }
        friend class Singleton<NetMgrImpl>;

public :
        bool Init() { return true; }

public :
        inline std::shared_ptr<GameGateSession> GetGateSession(uint64_t sid)
        { return _gateSesList.Get(sid).lock(); }

        inline std::shared_ptr<GameLobbySession> GetLobbySession(uint64_t sid)
        { return _lobbySesList.Get(sid).lock(); }

        void Terminate() {}
        void WaitForTerminate() {}

public :
        ThreadSafeUnorderedMap<uint64_t, std::weak_ptr<GameGateSession>> _gateSesList;
        ThreadSafeUnorderedMap<uint64_t, std::weak_ptr<GameLobbySession>> _lobbySesList;
};

// vim: fenc=utf8:expandtab:ts=8
