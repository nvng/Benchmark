#include "PlayerState.h"

#include "Player.h"

// namespace Jump {

#define DEFINE_STATE_ACTOR_MAIL_HANDLE(mt, st, body) \
        ACTOR_MAIL_HANDLE(Player, mt, st, body) { \
                OnEvent(mt, st, msg); \
                return nullptr; \
        }

PlayerStateMgr::StateBaseType*
PlayerStateMgr::CreateStateByType(int stateType)
{
	switch (stateType)
	{
        case E_PST_None : return new PlayerNoneState(); break;
	case E_PST_Lobby : return new PlayerLobbyState(); break;
	case E_PST_Queue : return new PlayerQueueState(); break;
	case E_PST_Game : return new PlayerGameState(); break;
	default :
		break;
	}

	return nullptr;
}

void PlayerNoneState::OnEvent(const PlayerPtr& player, StateEventInfo& evt)
{
        switch (evt._eventType)
        {
        case ClientGateSession::MsgHeaderType::MsgTypeMerge<E_MCMT_GameCommon, E_MCGCST_SwitchRegion>() :
                {
                        auto msg = std::dynamic_pointer_cast<MsgSwitchRegion>(evt._data);
                        switch (msg->region_type())
                        {
                        case E_RT_MainCity :
                                player->ChangeState(E_PST_Lobby, evt);
                                break;
                        default :
                                player->ChangeState(E_PST_Game, evt);
                                break;
                        }
                }
                break;
        default :
                LOG_INFO("玩家[{}] 在 E_PST_None 状态收到消息 mt[{:#x}] st[{:#x}]",
                         player->GetID(),
                         ClientGateSession::MsgHeaderType::MsgMainType(evt._eventType),
                         ClientGateSession::MsgHeaderType::MsgSubType(evt._eventType));
                break;
        }
}

void PlayerLobbyState::Enter(const PlayerPtr& player, StateEventInfo& evt)
{
        MsgReqQueue sendMsg;
        auto baseInfo = sendMsg.mutable_base_info();
        baseInfo->set_region_type(E_RT_PVE);
        baseInfo->set_queue_type(E_QT_Normal);
        player->SendPB(E_MCMT_QueueCommon, E_MCQCST_ReqQueue, &sendMsg);
}

void PlayerLobbyState::Exit(const PlayerPtr& player, StateEventInfo& evt)
{
        _exitQueueTimerGuid = INVALID_TIMER_GUID;
}

void PlayerLobbyState::OnEvent(const PlayerPtr& player, StateEventInfo& evt)
{
        switch (evt._eventType)
        {
        case ClientGateSession::MsgHeaderType::MsgTypeMerge<E_MCMT_GameCommon, E_MCGCST_SwitchRegion>() :
                {
                        auto msg = std::dynamic_pointer_cast<MsgSwitchRegion>(evt._data);
                        switch (msg->region_type())
                        {
                        case E_RT_MainCity :
                                // player->ChangeState(E_PST_Lobby, evt);
                                break;
                        default :
                                player->ChangeState(E_PST_Game, evt);
                                break;
                        }
                }
                break;
        case ClientGateSession::MsgHeaderType::MsgTypeMerge<E_MCMT_QueueCommon, E_MCQCST_ReqQueue>() :
                {
                        auto msg = std::dynamic_pointer_cast<MsgReqQueue>(evt._data);
                        switch (msg->error_type())
                        {
                        case E_CET_Success :
                                {
                                        player->_queueInfo.mutable_base_info()->CopyFrom(msg->base_info());
                                        PlayerWeakPtr weakPlayer = player;
                                        _exitQueueTimerGuid = player->StartTimerWithRelativeTimeOnce(RandInRange(0.0, 5.0), [weakPlayer](TimerGuidType id) {
                                                auto p = weakPlayer.lock();
                                                if (p)
                                                {
                                                        p->SendPB(E_MCMT_QueueCommon, E_MCQCST_ExitQueue);
                                                }
                                        });
                                }
                                break;
                        default :
                                LOG_INFO("玩家[{}] 在 E_PST_Lobby 收到消息 E_MCMT_QueueCommon E_MCQCST_ReqQueue 时，error[{}]",
                                         player->GetID(), msg->error_type());
                                break;
                        }
                }
                break;
        case ClientGateSession::MsgHeaderType::MsgTypeMerge<E_MCMT_QueueCommon, E_MCQCST_ExitQueue>() :
                {
                        auto msg = std::dynamic_pointer_cast<MsgExitQueue>(evt._data);
                        switch (msg->error_type())
                        {
                        case E_CET_Success :
                                {
                                        player->_queueInfo.mutable_base_info()->CopyFrom(msg->base_info());
                                        PlayerWeakPtr weakPlayer = player;
                                        player->StartTimerWithRelativeTimeOnce(RandInRange(0.0, 5.0), [weakPlayer](TimerGuidType id) {
                                                auto p = weakPlayer.lock();
                                                if (p)
                                                {
                                                        MsgReqQueue sendMsg;
                                                        auto baseInfo = sendMsg.mutable_base_info();
                                                        baseInfo->set_region_type(E_RT_PVE);
                                                        baseInfo->set_queue_type(E_QT_Normal);
                                                        p->SendPB(E_MCMT_QueueCommon, E_MCQCST_ReqQueue, &sendMsg);
                                                }
                                        });
                                }
                                break;
                        default :
                                LOG_INFO("玩家[{}] 在 E_PST_Lobby 收到消息 E_MCMT_QueueCommon E_MCQCST_ReqQueue 时，error[{}]",
                                         player->GetID(), msg->error_type());
                                break;
                        }
                }
                break;
        default :
                LOG_INFO("玩家[{}] 在 E_PST_Lobby 状态收到消息 mt[{:#x}] st[{:#x}]",
                         player->GetID(),
                         ClientGateSession::MsgHeaderType::MsgMainType(evt._eventType),
                         ClientGateSession::MsgHeaderType::MsgSubType(evt._eventType));
                break;
        }
}

DEFINE_STATE_ACTOR_MAIL_HANDLE(E_MCMT_QueueCommon, E_MCQCST_ReqQueue, MsgReqQueue);
DEFINE_STATE_ACTOR_MAIL_HANDLE(E_MCMT_QueueCommon, E_MCQCST_ExitQueue, MsgExitQueue);

void PlayerQueueState::OnEvent(const PlayerPtr& player, StateEventInfo& evt)
{
        switch (evt._eventType)
        {
        default :
                LOG_INFO("玩家[{}] 在 E_PST_Queue 状态收到消息 mt[{:#x}] st[{:#x}]",
                         player->GetID(),
                         ClientGateSession::MsgHeaderType::MsgMainType(evt._eventType),
                         ClientGateSession::MsgHeaderType::MsgSubType(evt._eventType));
                break;
        }
}

void PlayerGameState::Enter(const PlayerPtr& player, StateEventInfo& evt)
{
}

void PlayerGameState::OnEvent(const PlayerPtr& player, StateEventInfo& evt)
{
	switch (evt._eventType)
	{
        case ClientGateSession::MsgHeaderType::MsgTypeMerge<E_MCMT_GameCommon, E_MCGCST_SwitchRegion>() :
		{
			auto msg = std::reinterpret_pointer_cast<MsgSwitchRegion>(evt._data);
			if (msg->region_type() == E_RT_MainCity)
			{
				if (!player->UseGMGoods())
				{
					// StateEventInfo evt(E_PSET_ReqQueue);
					// player->OnEvent(evt);
				}
			}
		}
		break;
	// case E_PSET_ReqQueue :
	// 	{
	// 		if (player->_queueInfo.base_info().region_type() == E_RT_None)
	// 		{
	// 			MsgReqQueue sendMsg;
        //                         auto baseInfo = sendMsg.mutable_base_info();
        //                         baseInfo->set_region_type(E_RT_PVP);
	// 			player->SendPB(E_MCMT_QueueCommon, E_MCQCST_ReqQueue, &sendMsg);
	// 		}
	// 	}
	// 	break;
	default :
                LOG_INFO("玩家[{}] 在 E_PST_Game 状态收到消息 mt[{:#x}] st[{:#x}]",
                         player->GetID(),
                         ClientGateSession::MsgHeaderType::MsgMainType(evt._eventType),
                         ClientGateSession::MsgHeaderType::MsgSubType(evt._eventType));
		break;
	}
}

// } // end namespace Jump

// vim: fenc=utf8:expandtab:ts=8:sw=8
