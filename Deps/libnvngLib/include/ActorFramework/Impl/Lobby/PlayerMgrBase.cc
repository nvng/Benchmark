#include "PlayerMgrBase.h"

#include "PlayerBase.h"
#include "Tools/AppBase.h"
#include <boost/fiber/fiber.hpp>

namespace nl::af::impl {

// 玩家登录
SPECIAL_ACTOR_MAIL_HANDLE(PlayerMgrActor, 0, MsgClientLogin)
{
        const auto playerGuid = msg->player_guid();
        auto loginInfo = GetPlayerMgrBase()->_loginInfoList.Get(playerGuid);
        if (!loginInfo)
        {
                PLAYER_LOG_WARN(playerGuid,
                                "玩家[{}] 登录时，loginInfo is nullptr!!!",
                                playerGuid);
                return nullptr;
        }

        auto ses = loginInfo->_ses.lock();
        if (!ses)
        {
                PLAYER_LOG_WARN(playerGuid,
                                "玩家[{}] 登录时，ses is nullptr!!!",
                                playerGuid);
                GetPlayerMgrBase()->_loginInfoList.Remove(playerGuid, loginInfo.get());
                return nullptr;
        }

        PLAYER_DLOG_INFO(playerGuid,
                         "玩家[{}] 登录!!! mgrID[{}] _uniqueID[{}]",
                         playerGuid, GetID(), LobbyGateSession::ActorAgentType::GenUniqueID(ses, loginInfo->_from));
        auto clientActor = std::make_shared<LobbyGateSession::ActorAgentType>(loginInfo->_from, ses);
        loginInfo->_clientAgent = clientActor;
        loginInfo->_pb = msg;
        auto p = std::dynamic_pointer_cast<PlayerBase>(GetPlayerMgrBase()->GetActor(playerGuid));
        if (p)
        {
#ifdef ____BENCHMARK____
                PLAYER_LOG_FATAL_IF(playerGuid, GetID() != p->_actorMgrID, "aaaaaaaaaaaaaaaaaaaaaaaa");
#endif
                // 以下两行不能交换位置，uniqueID 修改后，E_PIF_Online 必须已修改，多线程会使用。
                FLAG_ADD(p->_internalFlag, E_PIF_Online);
                p->_uniqueID = clientActor->GetUniqueID();

                // 保证是第一个邮件，必须在 PlayerMgrActor 中执行，才能保证与 disconnect 消息时序性。
                p->SendPush(clientActor, E_MCMT_ClientCommon, E_MCCCST_Login, loginInfo);
        }
        else
        {
                p = GetPlayerMgrBase()->CreatePlayer(playerGuid, msg->nick_name(), msg->icon());

                // 以下两行不能交换位置，uniqueID 修改后，E_PIF_Online 必须已修改，多线程会使用。
                FLAG_ADD(p->_internalFlag, E_PIF_Online);
                p->_uniqueID = clientActor->GetUniqueID();

                // 保证是第一个邮件，必须在 PlayerMgrActor 中执行，才能保证与 disconnect 消息时序性。
                p->SendPush(clientActor, E_MCMT_ClientCommon, E_MCCCST_Login, loginInfo);
                GetPlayerMgrBase()->AddActor(p); // 必须在 PlayerMgrActor 中添加。

#ifdef ____BENCHMARK____
                p->_actorMgrID = GetID();
#endif
                std::weak_ptr<stLoginInfo> weakLoginInfo = loginInfo;
                boost::fibers::fiber(std::allocator_arg,
                                     // boost::fibers::fixedsize_stack{ 32 * 1024 },
                                     boost::fibers::segmented_stack{},
                                     [p, weakLoginInfo]() mutable {
                        do
                        {
                                {
                                        auto loginInfo = weakLoginInfo.lock();
                                        if (!loginInfo)
                                        {
                                                PLAYER_LOG_WARN(p->GetID(),
                                                                "玩家[{}] 登录时，loginInfo is nullptr!!!",
                                                                p->GetID());
                                                break;
                                        }

                                        auto clientActor = loginInfo->_clientAgent;
                                        auto ses = clientActor->GetSession();
                                        if (!ses)
                                        {
                                                PLAYER_LOG_WARN(p->GetID(),
                                                                "玩家[{}] 登录时，ses is nullptr!!!",
                                                                p->GetID());
                                                break;
                                        }

                                        if (!p->Init() || !p->LoadFromDB(loginInfo->_pb))
                                        {
                                                PLAYER_LOG_WARN(p->GetID(),
                                                                "玩家[{}] 登录时，init fail!!!",
                                                                p->GetID());

                                                auto sendMsg = std::make_shared<MsgClientLoginRet>();
                                                sendMsg->set_error_type(E_CET_Fail);
                                                ses->SendPB(sendMsg,
                                                            E_MCMT_ClientCommon,
                                                            E_MCCCST_Login,
                                                            LobbyGateSession::MsgHeaderType::E_RMT_Send,
                                                            E_CET_Fail,
                                                            p->GetID(),
                                                            clientActor->GetID());
                                                break;
                                        }
                                }

                                p->Start();
                                GetPlayerMgrBase()->RemoveActor(p);
                                return;
                        } while (0);

                        GetPlayerMgrBase()->RemoveActor(p);
                        GetPlayerMgrBase()->_loginInfoList.Remove(p->GetID(), weakLoginInfo.lock().get());
                }).detach();
        }

        return nullptr;
}

// 玩家删除
SPECIAL_ACTOR_MAIL_HANDLE(PlayerMgrActor, 1, MailUInt)
{
        auto p = std::dynamic_pointer_cast<PlayerBase>(from);
        if (p && msg->val() == p->_uniqueID)
        {
#ifdef ____BENCHMARK____
                PLAYER_LOG_FATAL_IF(p->GetID(), GetID() != p->_actorMgrID, "aaaaaaaaaaaaaaaaaaaaaaaa");
#endif
                // PLAYER_LOG_INFO(p->GetID(), "玩家[{}] terminate _taskID[{}]", p->GetID(), p->_taskID);
                GetPlayerMgrBase()->RemoveActor(p); // 必须删除。
                p->Terminate();
                // p->WaitForTerminate(); // 不能卡住。
        }
        else
        {
#ifndef ____BENCHMARK____
                PLAYER_LOG_WARN(p?p->GetID():0, "玩家[{}] 删除时，p[{}] pUniqueID[{}]",
                         p ? p->GetID() : 0, (void*)p.get(), p?p->_uniqueID.load():0);
#endif
        }

        return msg;
}

SPECIAL_ACTOR_MAIL_HANDLE(PlayerMgrActor, 2, stDisconnectInfo)
{
        auto p = std::dynamic_pointer_cast<PlayerBase>(GetPlayerMgrBase()->GetActor(msg->_to));
        if (p && p->_uniqueID == msg->_uniqueID)
        {
#ifdef ____BENCHMARK____
                PLAYER_LOG_FATAL_IF(p->GetID(), GetID() != p->_actorMgrID, "aaaaaaaaaaaaaaaaaaaaaaaa");
#endif
                /*
                PLAYER_LOG_INFO(p->GetID(), "玩家[{}] 收到 disconnect uniqueID[{}] msg[{}] mgrID[{}] _taskID[{}]",
                         p->GetID(), p->_uniqueID, msg->_uniqueID, GetID(), p->_taskID);
                         */
                FLAG_DEL(p->_internalFlag, E_PIF_Online);
                p->SendPush(nullptr, E_MCMT_ClientCommon, E_MCCCST_Disconnect, msg);
        }
        else
        {
#ifndef ____BENCHMARK____
                PLAYER_LOG_WARN(p?p->GetID():0,
                                "玩家[{}] disconnect，p[{}] pUniqueID[{}] msgUniqueID[{}]",
                                p ? p->GetID() : 0, (void*)p.get(), p ? p->_uniqueID.load() : 0, msg->_uniqueID);
#endif
        }
        return nullptr;
}

PlayerMgrBase::PlayerMgrBase()
        : SuperType("PlayerMgrBase")
          , _playerMgrActorArrSize(Next2N(std::thread::hardware_concurrency()) - 1)
          , _loginInfoList("PlayerMgrBase_loginInfoList")
{
        _playerMgrActorArr = new PlayerMgrActorPtr[_playerMgrActorArrSize+1];
}

PlayerMgrBase::~PlayerMgrBase()
{
        delete[] _playerMgrActorArr;
        _playerMgrActorArr = nullptr;
}

bool PlayerMgrBase::Init()
{
	if (!SuperType::Init())
		return false;

        for (int64_t i=0; i<_playerMgrActorArrSize+1; ++i)
        {
                _playerMgrActorArr[i] = std::make_shared<PlayerMgrActor>();
                _playerMgrActorArr[i]->Start();
        }

	return true;
}

void PlayerMgrBase::OnDayChange()
{
        DealReset(E_MILST_DayChange, 0);
}

void PlayerMgrBase::OnDataReset(int64_t h)
{
        DealReset(E_MILST_DataResetNoneZero, h);
}

void PlayerMgrBase::DealReset(EMsgInternalLocalSubType t, int64_t param)
{
        std::vector<std::vector<IActorWeakPtr>> tmpList;
        tmpList.resize(_playerMgrActorArrSize+1);
        for (int64_t i=0; i<_playerMgrActorArrSize+1; ++i)
                tmpList[i].reserve(1024);

        Foreach([this, &tmpList](const auto& weakAct) {
                auto act = weakAct.lock();
                if (act)
                        tmpList[act->GetID() & _playerMgrActorArrSize].emplace_back(act);
        });

        auto mail = std::make_shared<MailInt>();
        mail->set_val(param);
        auto m = std::make_shared<PlayerBase::ActorMailType>(nullptr, mail, PlayerBase::ActorMailType::MsgTypeMerge(E_MIMT_Local, t));

        // 需要用到 E_PIF_Online，需要在 PlayerMgrActor 中处理，没用 BroadCast。
        for (int64_t i=0; i<_playerMgrActorArrSize+1; ++i)
        {
                GetPlayerMgrActor(i)->SendPush([m, tl{std::move(tmpList[i])}]() {
                        for (auto& wa : tl)
                        {
                                auto act = std::dynamic_pointer_cast<PlayerBase>(wa.lock());
                                if (act && FLAG_HAS(act->_internalFlag, E_PIF_Online))
                                        act->Push(m);
                        }
                });
        }
}

void PlayerMgrBase::Terminate()
{
        for (int64_t i=0; i<_playerMgrActorArrSize+1; ++i)
                _playerMgrActorArr[i]->Terminate();
        for (int64_t i=0; i<_playerMgrActorArrSize+1; ++i)
                _playerMgrActorArr[i]->WaitForTerminate();

        BroadCast(nullptr, E_MIMT_Local, E_MILST_Terminate, nullptr);
}

void PlayerMgrBase::WaitForTerminate()
{
        SuperType::WaitForTerminate();
}

NET_MSG_HANDLE(LobbyGateSession, E_MCMT_ClientCommon, E_MCCCST_Login, MsgClientLogin)
{
        // PLAYER_LOG_INFO(msgHead._to, "from:{} to:{}", msgHead._from, msgHead._to);
        const auto playerGuid = msg->player_guid();
        if (0 == playerGuid)
        {
                PLAYER_LOG_WARN(playerGuid,
                                "玩家[{}] 登录时出错，playerGuid 不能为0!!!",
                                playerGuid);
                return;
        }

        PLAYER_LOG_FATAL_IF(playerGuid,
                            !(0!=msgHead._from && playerGuid == static_cast<decltype(playerGuid)>(msgHead._to)),
                            "playerGuid[{}] msgHead._to[{}]",
                            playerGuid, msgHead._to);
        auto loginInfo = std::make_shared<stLoginInfo>();
        /*
         * 必须在此卡住，有可能在 redis load 数据时卡住，如果此处放过，
         * 则会导致 player 的 channel 长度 128 不够用，从而卡住网络线程，
         * 导致整个 lobby 出问题。
         * 任何时候如被 redis 卡住，时间长了会导致客户端重连，第一次重连
         * 过来时，会向 player 发送 E_MCCCST_Login，而该消息在redis处理好
         * 之前不会被处理，因此 _loginInfoList 不会删除相关信息，后面的登录
         * 就会卡在此处。
         */
        if (GetPlayerMgrBase()->_loginInfoList.Add(playerGuid, loginInfo))
        {
                loginInfo->_from = msgHead._from;
                loginInfo->_ses = shared_from_this();
                GetPlayerMgrBase()->GetPlayerMgrActor(playerGuid)->SendPush(0, msg);
        }
        else
        {
                PLAYER_DLOG_WARN(playerGuid,
                                 "玩家[{}] 在登录时，已经有别的端在登录!!!",
                                 playerGuid);

                auto sendMsg = std::make_shared<MsgClientLoginRet>();
                sendMsg->set_error_type(E_CET_InLogin);
                SendPB(sendMsg,
                       E_MCMT_ClientCommon,
                       E_MCCCST_Login,
                       MsgHeaderType::E_RMT_Send,
                       E_CET_InLogin,
                       msgHead._to,
                       msgHead._from);
        }
}

NET_MSG_HANDLE(LobbyGateSession, E_MCMT_ClientCommon, E_MCCCST_Disconnect)
{
        PLAYER_LOG_FATAL_IF(msgHead._to, !(0 != msgHead._from), "");
        auto msg = std::make_shared<stDisconnectInfo>();
        msg->_uniqueID = ActorAgentType::GenUniqueID(shared_from_this(), msgHead._from);
        msg->_to = msgHead._to;
        GetPlayerMgrBase()->GetPlayerMgrActor(msgHead._to)->SendPush(2, msg);
}

}; // end of namespace nl::af::impl

// vim: fenc=utf8:expandtab:ts=8:noma
