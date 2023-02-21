#include "PlayerState.h"

#include "Player.h"

#if 0
namespace JCC {

PlayerGameStateMgr::StateBaseType*
PlayerGameStateMgr::CreateStateByType(int stateType)
{
	switch (stateType)
	{
	case E_PGST_Queue : return new PlayerGameQueueState(); break;
	case E_PGST_Game : return new PlayerGameGameState(); break;
	default :
		break;
	}

	return nullptr;
}

void PlayerGameQueueState::OnEvent(const PlayerPtr& player, StateEventInfo& evt)
{
	switch (evt._eventType)
	{
	default :
	  break;
	}
}

void PlayerGameGameState::Enter(const PlayerPtr& player, StateEventInfo& evt)
{
}

void PlayerGameGameState::OnEvent(const PlayerPtr& player, StateEventInfo& evt)
{
	switch (evt._eventType)
	{
	case E_MCGCST_SwitchRegion :
		{
			auto msg = std::reinterpret_pointer_cast<MsgSwitchRegion>(evt._data);
			if (msg->region_type() == E_RT_MainCity)
			{
				if (!player->UseGMGoods())
				{
					StateEventInfo evt(E_PSET_ReqQueue);
					player->OnEvent(evt);
				}
			}
		}
		break;
	case E_PSET_ReqQueue :
		{
			if (player->_queueInfo.region_type() == E_RT_None)
			{
				MsgReqQueue sendMsg;
				sendMsg.set_region_type(E_RT_PVP);
				player->SendPB(E_MCMT_Game, E_MCGCST_ReqQueue);
			}
		}
		break;
	default :
		break;
	}
}

} // end namespace JCC
#endif

// vim: fenc=utf8:expandtab:ts=8:sw=8
