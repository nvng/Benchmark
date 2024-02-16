#include "PlayerState.h"

#include "Player.h"
#include "msg_client.pb.h"
#include "msg_common.pb.h"
#include "msg_jump.pb.h"
#include "msg_shop.pb.h"
#include <cstdint>
#include <memory>

// namespace Jump {

#define DEFINE_STATE_ACTOR_MAIL_HANDLE(mt, st, body) \
ACTOR_MAIL_HANDLE(Player, mt, st, body) { \
        OnEvent(mt, st, msg); \
        return nullptr; \
}

#define DEFINE_STATE_ACTOR_MAIL_HANDLE_EMPTY(mt, st) \
ACTOR_MAIL_HANDLE(Player, mt, st) { \
        OnEvent(mt, st, nullptr); \
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
        default : break;
        }

        return nullptr;
}

void PlayerNoneState::OnEvent(const PlayerPtr& player, StateEventInfo& evt)
{
        player->CheckThreadSafe();
        // LOG_INFO("玩家[{}] 收到登录消息 type[{:#x}]", player->GetID(), evt._eventType);
        switch (evt._eventType)
        {
                case Player::ActorMailType::MsgTypeMerge<E_MCMT_GameCommon, E_MCGCST_SwitchRegion>() :
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
                                 Player::ActorMailType::MsgMainType(evt._eventType),
                                 Player::ActorMailType::MsgSubType(evt._eventType));
                        break;
        }
}

void PlayerLobbyState::Enter(const PlayerPtr& player, StateEventInfo& evt)
{
        player->CheckThreadSafe();
        // LOG_INFO("玩家[{}] 进入大厅状态!!!", player->GetID());
        if (false)
        {
                auto sendMsg = std::make_shared<MsgReqQueue>();
                auto baseInfo = sendMsg->mutable_base_info();
                baseInfo->set_region_type(E_RT_PVE);
                baseInfo->set_queue_type(E_QT_Normal);
                player->SendPB(E_MCMT_QueueCommon, E_MCQCST_ReqQueue, sendMsg);
        }

        /*
        for (int64_t i=0; i<10; ++i)
        {
                auto sendMsg = std::make_shared<MsgShopRefresh>();
                sendMsg->set_id(INT64_MAX);
                sendMsg->set_param(INT64_MAX);
                player->SendPB(E_MCMT_Shop, E_MCSST_Refresh, sendMsg);
        }
        */
}

void PlayerLobbyState::Exit(const PlayerPtr& player, StateEventInfo& evt)
{
        player->CheckThreadSafe();
        _reqQueueTimerGuid = INVALID_TIMER_GUID;
        _exitQueueTimerGuid = INVALID_TIMER_GUID;
}

void PlayerLobbyState::OnEvent(const PlayerPtr& player, StateEventInfo& evt)
{
        player->CheckThreadSafe();
        // LOG_INFO("玩家[{}] 收到大厅消息 type[{:#x}]", player->GetID(), evt._eventType);

        switch (evt._eventType)
        {
        case Player::ActorMailType::MsgTypeMerge<E_MCMT_GameCommon, E_MCGCST_SwitchRegion>() :
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
        case Player::ActorMailType::MsgTypeMerge<E_MCMT_QueueCommon, E_MCQCST_ReqQueue>() :
                {
                        auto msg = std::dynamic_pointer_cast<MsgReqQueue>(evt._data);
                        switch (msg->error_type())
                        {
                        case E_CET_Success :
                                {
#if 0
                                        player->_msgPlayerInfo.mutable_queue_info()->mutable_base_info()->CopyFrom(msg->base_info());
                                        PlayerWeakPtr weakPlayer = player;
                                        _reqQueueTimerGuid = player->StartTimerWithRelativeTimeOnce(RandInRange(0.0, 0.0), [weakPlayer, this](TimerGuidType id) {
                                                auto p = weakPlayer.lock();
                                                if (p && _reqQueueTimerGuid == id)
                                                {
                                                        p->SendPB(E_MCMT_QueueCommon, E_MCQCST_ExitQueue);
                                                        /*
                                                           static auto sendMsg = []() {
                                                           auto ret = std::make_shared<MsgClientLogin>();
                                                        // ret->set_nick_name("a");
                                                        return ret;
                                                        }();
                                                        for (int64_t i=0; i<70; ++i)
                                                        p->SendPB(0x7f, 1, sendMsg.get());
                                                        */
                                                }
                                        });
#endif
                                }
                                break;
                        case E_CET_QueueError :
                        case E_CET_Fail :
                                player->ChangeState(E_PST_Lobby, evt);
                                break;
                        default :
                                LOG_WARN("玩家[{}] 在 E_PST_Lobby 收到消息 E_MCMT_QueueCommon E_MCQCST_ReqQueue 时，error[{:#x}] rt[{}] qt[{}]",
                                         player->GetID(), msg->error_type(), msg->base_info().region_type(), msg->base_info().queue_type());
                                break;
                        }
                }
                break;
        case Player::ActorMailType::MsgTypeMerge<E_MCMT_QueueCommon, E_MCQCST_ExitQueue>() :
                {
                        auto msg = std::dynamic_pointer_cast<MsgExitQueue>(evt._data);
                        switch (msg->error_type())
                        {
                        case E_CET_Success :
                                {
#if 0
                                        player->_msgPlayerInfo.mutable_queue_info()->mutable_base_info()->CopyFrom(msg->base_info());
                                        PlayerWeakPtr weakPlayer = player;
                                        _exitQueueTimerGuid = player->StartTimerWithRelativeTimeOnce(RandInRange(0.0, 0.0), [weakPlayer, this](TimerGuidType id) {
                                                auto p = weakPlayer.lock();
                                                if (p && _exitQueueTimerGuid == id)
                                                {
                                                        MsgReqQueue sendMsg;
                                                        auto baseInfo = sendMsg.mutable_base_info();
                                                        baseInfo->set_region_type(E_RT_PVE);
                                                        baseInfo->set_queue_type(E_QT_Normal);
                                                        p->SendPB(E_MCMT_QueueCommon, E_MCQCST_ReqQueue, &sendMsg);
                                                        /*
                                                           {
                                                           static auto sendMsg = []() {
                                                           auto ret = std::make_shared<MsgClientLogin>();
                                                        // ret->set_nick_name("a");
                                                        return ret;
                                                        }();
                                                        for (int64_t i=0; i<70; ++i)
                                                        p->SendPB(0x7f, 1, sendMsg.get());
                                                        }
                                                        */
                                                }
                                        });
#endif
                                }
                                break;
                        default :
                                LOG_WARN("玩家[{}] player_guid[{}] 在 E_PST_Lobby 收到消息 E_MCMT_QueueCommon E_MCQCST_ExitQueue 时，error[{:#x}] rt[{}] qt[{}]",
                                         player->GetID(), msg->player_guid(), msg->error_type(), msg->base_info().region_type(), msg->base_info().queue_type());
                                break;
                        }
                }
                break;
        case Player::ActorMailType::MsgTypeMerge<E_MCMT_QueueCommon, E_MCCCST_Disconnect>() :
                player->ChangeState(E_PST_None, evt);
                break;
        default :
                LOG_INFO("玩家[{}] 在 E_PST_Lobby 状态收到消息 mt[{:#x}] st[{:#x}]",
                         player->GetID(),
                         Player::ActorMailType::MsgMainType(evt._eventType),
                         Player::ActorMailType::MsgSubType(evt._eventType));
                break;
        }
}

DEFINE_STATE_ACTOR_MAIL_HANDLE(E_MCMT_QueueCommon, E_MCQCST_ReqQueue, MsgReqQueue);
DEFINE_STATE_ACTOR_MAIL_HANDLE(E_MCMT_QueueCommon, E_MCQCST_ExitQueue, MsgExitQueue);

void PlayerQueueState::OnEvent(const PlayerPtr& player, StateEventInfo& evt)
{
        player->CheckThreadSafe();
        switch (evt._eventType)
        {
        default :
                LOG_INFO("玩家[{}] 在 E_PST_Queue 状态收到消息 mt[{:#x}] st[{:#x}]",
                         player->GetID(),
                         Player::ActorMailType::MsgMainType(evt._eventType),
                         Player::ActorMailType::MsgSubType(evt._eventType));
                break;
        }
}

DEFINE_STATE_ACTOR_MAIL_HANDLE(E_MCMT_GameCommon, E_MCGCST_RegionInfo, Jump::MsgRegionInfo);
DEFINE_STATE_ACTOR_MAIL_HANDLE(E_MCMT_GameCommon, E_MCGCST_OnFighterEnter, Jump::MsgFighterEnter);
DEFINE_STATE_ACTOR_MAIL_HANDLE(E_MCMT_GameCommon, E_MCGCST_OnFighterExit, MsgFighterExit);
DEFINE_STATE_ACTOR_MAIL_HANDLE(E_MCMT_GameCommon, E_MCGCST_UpdateRegionStateInfo, Jump::MsgRegionStateInfo);
DEFINE_STATE_ACTOR_MAIL_HANDLE(E_MCMT_GameCommon, E_MCGCST_Conclude, Jump::MsgConcludeInfo);
DEFINE_STATE_ACTOR_MAIL_HANDLE_EMPTY(E_MCMT_GameCommon, E_MCGCST_GameDisconnect);

DEFINE_STATE_ACTOR_MAIL_HANDLE(E_MCMT_Game, Jump::E_MCGST_Sync, Jump::MsgSync);
// DEFINE_STATE_ACTOR_MAIL_HANDLE(E_MCMT_Game, Jump::E_MCGST_SyncCommon, Jump::MsgSync);
DEFINE_STATE_ACTOR_MAIL_HANDLE(E_MCMT_Game, Jump::E_MCGST_ChangeAI, Jump::MsgChangeAI);

void PlayerGameState::Enter(const PlayerPtr& player, StateEventInfo& evt)
{
        player->CheckThreadSafe();
        // LOG_INFO("玩家[{}] 进入游戏状态!!!", player->GetID());
        // player->SendPB(E_MCMT_GameCommon, E_MCGCST_ReqExitRegion);
}

void PlayerGameState::OnEvent(const PlayerPtr& player, StateEventInfo& evt)
{
        player->CheckThreadSafe();
        // LOG_INFO("玩家[{}] 收到游戏消息 type[{:#x}]", player->GetID(), evt._eventType);
        switch (evt._eventType)
        {
        case Player::ActorMailType::MsgTypeMerge<E_MCMT_GameCommon, E_MCGCST_SwitchRegion>() :
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
        case Player::ActorMailType::MsgTypeMerge<E_MCMT_GameCommon, E_MCGCST_RegionInfo>() :
                // LOG_INFO("玩家[{}] 收到 RegionInfo!!!", player->GetID());
                {
                        /*
                        ::nl::util::SteadyTimer::StaticStart(RandInRange(0.0, 10.0), [player]() {
                                player->SendPB(E_MCMT_GameCommon, E_MCGCST_ReqExitRegion);
                        });
                        */

                        std::weak_ptr<Player> weakPlayer = player;
                        if (!player->_inTimer)
                        {
                                player->_inTimer = true;
                                ::nl::util::SteadyTimer::StartForever(0.1, [weakPlayer]() {
                                        auto player = weakPlayer.lock();
                                        if (player)
                                        {
                                                auto sendMsg = std::make_shared<Jump::MsgSync>();
                                                sendMsg->set_player_guid(player->GetID());
                                                sendMsg->set_state(RandInRange(Jump::E_SST_1, Jump::ESyncStateType_ARRAYSIZE));
                                                sendMsg->set_param_1(INT64_MAX);
                                                sendMsg->set_param_2(INT64_MAX);
                                                sendMsg->set_param_3(INT64_MAX);
                                                player->SendPB(E_MCMT_Game, Jump::E_MCGST_Sync, sendMsg);
                                                return true;
                                        }
                                        return false;
                                });
                        }

                        ::nl::util::SteadyTimer::StaticStart(RandInRange(0.0, 10.0), [weakPlayer]() {
                                auto player = weakPlayer.lock();
                                if (player)
                                {
                                        auto ses = player->_ses.lock();
                                        if (ses)
                                                ses->Close(1777);
                                }
                        });

                        /*
                           _sendMsg = std::make_shared<Jump::MsgSync>();
                           _sendMsg->set_player_guid(player->GetID());
                           player->SendPB(E_MCMT_Game, Jump::E_MCGST_Sync, _sendMsg);
                           for (int64_t i=0; i<200; ++i)
                           player->SendPB(E_MCMT_Game, Jump::E_MCGST_SyncCommon, _sendMsg);
                           */
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
        case Player::ActorMailType::MsgTypeMerge<E_MCMT_Game, Jump::E_MCGST_Sync>() :
                {
                        /*
                        auto sendMsg = std::make_shared<Jump::MsgSync>();
                        sendMsg->set_player_guid(player->GetID());
                        sendMsg->set_state(Jump::E_SST_3);
                        sendMsg->set_param_1(INT64_MAX);
                        sendMsg->set_param_2(INT64_MAX);
                        sendMsg->set_param_3(INT64_MAX);
                        player->SendPB(E_MCMT_Game, Jump::E_MCGST_Sync, _sendMsg);
                        */
                        /*
                           player->SendPB(E_MCMT_Game, Jump::E_MCGST_Sync, _sendMsg);
                           for (int64_t i=0; i<100; ++i)
                           player->SendPB(E_MCMT_Game, Jump::E_MCGST_SyncCommon, nullptr);
                           */
                }
                break;
        // case Player::ActorMailType::MsgTypeMerge<E_MCMT_Game, Jump::E_MCGST_SyncCommon>() :

        case Player::ActorMailType::MsgTypeMerge<E_MCMT_GameCommon, E_MCGCST_OnFighterEnter>() :
        case Player::ActorMailType::MsgTypeMerge<E_MCMT_GameCommon, E_MCGCST_OnFighterExit>() :
        case Player::ActorMailType::MsgTypeMerge<E_MCMT_GameCommon, E_MCGCST_UpdateRegionStateInfo>() :
        case Player::ActorMailType::MsgTypeMerge<E_MCMT_GameCommon, E_MCGCST_GameDisconnect>() :
        case Player::ActorMailType::MsgTypeMerge<E_MCMT_GameCommon, E_MCGCST_Conclude>() :

        case Player::ActorMailType::MsgTypeMerge<E_MCMT_Game, Jump::E_MCGST_ChangeAI>() :
                break;
        default :
                LOG_INFO("玩家[{}] 在 E_PST_Game 状态收到消息 mt[{:#x}] st[{:#x}]",
                         player->GetID(),
                         Player::ActorMailType::MsgMainType(evt._eventType),
                         Player::ActorMailType::MsgSubType(evt._eventType));
                break;
        }
}

// } // end namespace Jump

// vim: fenc=utf8:expandtab:ts=8:sw=8
