#pragma once

class Player;
typedef std::shared_ptr<Player> PlayerPtr;
#include "msg_jump.pb.h"

// namespace Jump {

struct StateEventInfo
{
        explicit StateEventInfo(int64_t eventType)
                : _eventType(eventType)
        {
        }

        int64_t   _eventType = 0;
        double    _dlParam = 0.0;
        bool      _bParam = false;
        int64_t   _param = 0;
        std::string_view _strParam;
        ActorMailDataPtr _data;
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

enum EPlayerStateType
{
        E_PST_None = 0,
        E_PST_Lobby = 1,
        E_PST_Queue = 2, // 排队。
        E_PST_Game = 3,
        E_PST_Count,
};

class PlayerStateMgr : public PlayerStateMgrBase
{
public :
	PlayerStateMgr() : PlayerStateMgrBase(E_PST_Count) {}
	StateBaseType* CreateStateByType(int stateType) override;
};

class PlayerNoneState : public PlayerStateBase
{
public :
        PlayerNoneState() : PlayerStateBase(E_PST_None) {}
        void OnEvent(const PlayerPtr& player, StateEventInfo& evt) override;
};

class PlayerLobbyState : public PlayerStateBase
{
public :
        PlayerLobbyState() : PlayerStateBase(E_PST_Lobby) {}
        void Enter(const PlayerPtr& player, StateEventInfo& evt) override;
        void Exit(const PlayerPtr& player, StateEventInfo& evt) override;
        void OnEvent(const PlayerPtr& player, StateEventInfo& evt) override;

public :
        TimerGuidType _reqQueueTimerGuid = INVALID_TIMER_GUID;
        TimerGuidType _exitQueueTimerGuid = INVALID_TIMER_GUID;
};

class PlayerQueueState : public PlayerStateBase
{
public :
        PlayerQueueState() : PlayerStateBase(E_PST_Queue) {}
        void OnEvent(const PlayerPtr& player, StateEventInfo& evt) override;
};

class PlayerGameState : public PlayerStateBase
{
public :
        PlayerGameState() : PlayerStateBase(E_PST_Game) {}
        void Enter(const PlayerPtr& player, StateEventInfo& evt) override;
        void OnEvent(const PlayerPtr& player, StateEventInfo& evt) override;

        std::shared_ptr<Jump::MsgSync> _sendMsg;
};

// } // end namespace Jump

// vim: fenc=utf8:expandtab:ts=8
