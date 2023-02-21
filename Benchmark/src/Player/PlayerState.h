#pragma once

#if 0
class Player;
typedef std::shared_ptr<Player> PlayerPtr;

namespace JCC {

enum EPlayerStateEventType
{
        E_PSET_None = 256,
        E_PSET_ReqQueue,
};

struct StateEventInfo
{
        explicit StateEventInfo(int eventType)
                : _eventType(eventType)
        {
        }

        int       _eventType = 0;
        double    _dlParam = 0.0;
        bool      _bParam = false;
        int64_t   _param = 0;
        std::string_view _strParam;
        std::shared_ptr<void> _data;
        EClientErrorType _errorType = E_CET_Success;
};

class PlayerStateMgrBase : public FSMBase<const PlayerPtr, StateEventInfo>
{
public:
	PlayerStateMgrBase(int stateListSize) : FSMBase(stateListSize) {}
	~PlayerStateMgrBase() override {}
};

class PlayerStateBase : public StateBase<const PlayerPtr, StateEventInfo>
{
public :
        PlayerStateBase(int stateType) : StateBase(stateType) {}
};

enum EPlayerGameStateType
{
	E_PGST_None = 0,
        E_PGST_Queue = 1, // 排队。
        E_PGST_Game = 2,
	E_PGST_Count,
};

class PlayerGameStateMgr : public PlayerStateMgrBase
{
public :
	PlayerGameStateMgr() : PlayerStateMgrBase(E_PGST_Count) {}
	StateBaseType* CreateStateByType(int stateType) override;
};

class PlayerGameQueueState : public PlayerStateBase
{
public :
        PlayerGameQueueState() : PlayerStateBase(E_PGST_Game) {}
	void OnEvent(const PlayerPtr& player, StateEventInfo& evt) override;
};

class PlayerGameGameState : public PlayerStateBase
{
public :
        PlayerGameGameState() : PlayerStateBase(E_PGST_Game) {}
	void Enter(const PlayerPtr& player, StateEventInfo& evt) override;
	void OnEvent(const PlayerPtr& player, StateEventInfo& evt) override;
};

} // end namespace JCC
#endif

// vim: fenc=utf8:expandtab:ts=8
