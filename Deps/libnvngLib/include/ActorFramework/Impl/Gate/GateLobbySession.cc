#include "GateLobbySession.h"

#include "NetMgrImpl.h"
#include "Player.h"
#include "GateClientSession.h"

namespace nl::af::impl {

const std::string GateLobbySession::scPriorityTaskKey = "gate_lobby_load";
bool GateLobbySession::InitOnce()
{
        if (!SuperType::InitOnce())
                return false;

        SteadyTimer::StaticStart(1, []() {
                int64_t cnt = 0;
                NetMgrImpl::GetInstance()->ForeachLobby([&cnt](const auto& ses) {
                        ++cnt;
                });

                if (cnt >= ServerListCfgMgr::GetInstance()->GetSize<stLobbyServerInfo>())
                {
                        GetAppBase()->_startPriorityTaskList->Finish(scPriorityTaskKey);
                }
        });

        return true;
}

GateLobbySession::~GateLobbySession()
{
}

void GateLobbySession::OnConnect()
{
        LOG_WARN("lobby 连接成功!!!");
	SuperType::OnConnect();
}

void GateLobbySession::OnClose(int32_t reasonType)
{
        LOG_WARN("lobby 断开连接!!!");

        std::unordered_set<std::shared_ptr<GateClientSession>> tmpList;
        _playerList.Foreach([&tmpList](const std::weak_ptr<Player>& weakPlayer) {
                auto p = weakPlayer.lock();
                if (!p)
                        return;

                auto clientSes = p->_clientSes.lock();
                if (clientSes)
                        tmpList.emplace(clientSes);
                p->OnLobbyClose();
        });

        for (auto& ses : tmpList)
        {
                if (ses)
                        ses->Close(1001);
        }

	SuperType::OnClose(reasonType);
        NetMgrImpl::GetInstance()->RemoveSession(E_GST_Lobby, this);
}

void GateLobbySession::MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
{
        SuperType::MsgHandleServerInit(ses, buf, bufRef);
        NetMgrImpl::GetInstance()->AddSession(E_GST_Lobby, ses);
        LOG_WARN("初始化大厅 sid[{}] 成功!!!", ses->GetSID());
}

#ifndef ____USE_IMPL_GATE_LOBBY_SESSION_ONRECV____
void GateLobbySession::OnRecv(typename SuperType::BuffTypePtr::element_type* buf, const typename SuperType::BuffTypePtr& bufRef)
{
        ++GetApp()->_serverRecvCnt;
        auto msgHead = *reinterpret_cast<MsgHeaderType*>(buf);
        const auto mainType = MsgHeaderType::MsgMainType(msgHead._type);
        const auto subType  = MsgHeaderType::MsgSubType(msgHead._type);
        DLOG_INFO("GateLobbySession recv mainType:{:#x} subType:{:#x} sesCnt:{}", mainType, subType, NetMgrImpl::GetInstance()->GetSessionCnt());

        PlayerPtr player;
        switch (msgHead._type)
        {
        case MsgHeaderType::MsgTypeMerge<E_MCMT_ClientCommon, E_MCCCST_Login>() :
                player = PlayerMgr::GetInstance()->GetLoginPlayer(msgHead._to, msgHead._from);
                LOG_WARN_IF(!player, "网关收到大厅消息，但玩家[{}] 没找到!!! mt[{:#x}] st[{:#x}]", msgHead._to, mainType, subType);
                break;
        default :
                player = GetLobbyPlayer(msgHead._to, msgHead._from);
                DLOG_WARN_IF(!player, "网关收到大厅消息，但玩家[{}] 没找到!!! mt[{:#x}] st[{:#x}]", msgHead._to, mainType, subType);
                break;
        }

        if (player)
        {
                switch (msgHead._type)
                {
                case MsgHeaderType::MsgTypeMerge<E_MCMT_ClientCommon, E_MCCCST_Login>() :
                        {
                                auto clientSes = player->_clientSes.lock();
                                if (clientSes)
                                {
                                        if (0 == msgHead._guid) // 登录异常时为非0。
                                        {
#ifdef ____BENCHMARK____
                                                bool playerMgrRet = PlayerMgr::GetInstance()->AddPlayer(player);
                                                LOG_FATAL_IF(!playerMgrRet, "2222222222222222222222222 id[{}] uniqueID[{}]", player->GetID(), player->GetUniqueID());
                                                bool sesRet = AddPlayer(player);
                                                LOG_FATAL_IF(!sesRet, "3333333333333333333333333 id[{}] uniqueID[{}]", player->GetID(), player->GetUniqueID());
                                                if (playerMgrRet && sesRet)
#else
                                                        if (PlayerMgr::GetInstance()->AddPlayer(player) && AddPlayer(player))
#endif
                                                        {
                                                                clientSes->_player_ = player;

                                                                // 只是为了在登录消息之前，返回服务器时间。
                                                                auto sendMsg = std::make_shared<MsgClientHeartBeat>();
                                                                sendMsg->set_server_time(GetClock().GetTime() * 1000);
                                                                player->Send2Client(sendMsg, E_MCMT_Internal, E_MCIST_HeartBeat);

                                                                player->Send2Client<MsgHeaderType>(bufRef, buf, msgHead._size, msgHead.CompressType(), mainType, subType);
                                                        }
                                                        else
                                                        {
#ifdef ____BENCHMARK____
                                                                auto t = PlayerMgr::GetInstance()->GetPlayer(player->GetUniqueID(), player->GetID());
                                                                auto t1 = GetLobbyPlayer(player->GetUniqueID(), player->GetID());
                                                                LOG_FATAL("玩家[{}] 登录返回后，添加到 GateLobbySession 失败!!! uniqueID[{}] player[{}] t[{}] t1[{}]",
                                                                          player->GetID(), player->GetUniqueID(), (void*)player.get(), (void*)t.get(), (void*)t1.get());
#else
                                                                LOG_WARN("玩家[{}] 登录返回后，添加到 GateLobbySession 失败!!!", player->GetID());
#endif
                                                                auto msg = std::make_shared<MsgClientLoginRet>();
                                                                msg->set_error_type(E_CET_Fail);
                                                                player->Send2Client(msg, E_MCMT_ClientCommon, E_MCCCST_Login);
                                                                PlayerMgr::GetInstance()->RemovePlayer(player->GetID(), player.get());
                                                        }
                                        }
                                        else
                                        {
                                                player->Send2Client<MsgHeaderType>(bufRef, buf, msgHead._size, msgHead.CompressType(), mainType, subType);
                                                player->ResetLobbySes();
                                                clientSes->Close(1006);
                                        }
                                }
                                else
                                {
                                        DLOG_WARN("玩家[{}] 登录返回后，clientSes已经释放!!! shared_ptr:{}", player->GetID(), player.use_count());
                                }
                                auto rp = PlayerMgr::GetInstance()->RemoveLoginPlayer(player->GetID(), player.get());
                                (void)rp;
#ifdef ____BENCHMARK____
                                LOG_FATAL_IF(!rp && !FLAG_HAS(player->_internalFlag, E_GPFT_Delete),
                                             "4444444444444444444444444444444 guid[{}] p[{}] id[{}]",
                                             (uint64_t)(msgHead._guid), fmt::ptr(player), player->GetID());
                                FLAG_DEL(player->_internalFlag, E_GPFT_Delete);
#endif
                        }
                        break;
                case MsgHeaderType::MsgTypeMerge<E_MCMT_ClientCommon, E_MCCCST_Kickout>() :
                        {
#ifdef ____BENCHMARK____
                                auto t = RemovePlayer(player->GetID(), player.get());
#endif
                                auto tmpP = PlayerMgr::GetInstance()->RemovePlayer(player->GetID(), player.get());
#ifdef ____BENCHMARK____
                                LOG_FATAL_IF(!t && t != tmpP, "55555555555555555555555555555 lobbyPlayer[{}] mgrPlayer[{}]", (void*)t.get(), (void*)tmpP.get());
#endif
                                if (tmpP)
                                {
                                        tmpP->ResetLobbySes();
                                        tmpP->Send2Client<MsgHeaderType>(bufRef, buf, msgHead._size, msgHead.CompressType(), mainType, subType);
                                        auto ses = tmpP->_clientSes.lock();
                                        if (ses)
                                                ses->Close(1003);
                                        return;
                                }
                                return;
                        }
                        break;
                case MsgHeaderType::MsgTypeMerge<E_MCMT_GameCommon, E_MCGCST_GameDisconnect>() :
                        {
                                auto gameSes = player->_gameSes.lock();
                                if (gameSes)
                                {
                                        gameSes->RemovePlayer(player->GetID(), player.get());
                                        player->_gameSes.reset();
                                }
                                player->Send2Client<MsgHeaderType>(bufRef, buf, msgHead._size, msgHead.CompressType(), mainType, subType);
                        }
                        break;
                case MsgHeaderType::MsgTypeMerge<E_MCMT_GameCommon, E_MCGCST_SwitchRegion>() :
                        {
                                MailSwitchRegion msg;
                                bool ret = Compress::UnCompressAndParseAlloc(static_cast<Compress::ECompressType>(msgHead._ct), msg, buf+sizeof(MsgHeaderType), msgHead._size-sizeof(MsgHeaderType));
                                if (ret)
                                {
                                        auto msgSwitchRegion = std::make_shared<MsgSwitchRegion>();
                                        msgSwitchRegion->set_error_type(E_CET_Success);
                                        msgSwitchRegion->set_region_type(msg.region_type());
                                        msgSwitchRegion->set_old_region_type(msg.old_region_type());

                                        if (E_RT_MainCity == msg.region_type())
                                        {
                                                auto gameSes = player->_gameSes.lock();
                                                if (gameSes)
                                                {
                                                        gameSes->RemovePlayer(player->GetID(), player.get());
                                                        player->_gameSes.reset();
                                                }
                                        }
                                        else
                                        {
                                                auto gameSes = NetMgrImpl::GetInstance()->DistGameSession(msg.game_sid(), player->GetID());
                                                if (gameSes)
                                                {
                                                        if (gameSes->AddPlayer(player))
                                                        {
                                                                player->_regionGuid = msg.region_id();
                                                                player->_gameSes = gameSes;
                                                        }
                                                        else
                                                        {
                                                                LOG_WARN("玩家[{}] 收到切场景消息，但 gameSes player 添加失败!!!", player->GetID());
                                                                auto sendMsg = std::make_shared<MsgClientKickout>();
                                                                sendMsg->set_error_type(E_CET_GateAdd2GameFail);
                                                                player->Send2Client(sendMsg, E_MCMT_ClientCommon, E_MCCCST_Kickout);
                                                                auto ses = player->_clientSes.lock();
                                                                if (ses)
                                                                        ses->Close(1003);
                                                                break;
                                                        }
                                                }
                                                else
                                                {
                                                        LOG_WARN("网关收到大厅切场景消息，但 gameSes 分配失败!!! sid:{}", msg.game_sid());
                                                        player->Send2Client(nullptr, E_MCMT_GameCommon, E_MCGCST_GameDisconnect);
                                                        break;
                                                }
                                        }

                                        player->Send2Client(msgSwitchRegion, E_MCMT_GameCommon, E_MCGCST_SwitchRegion);
                                }
                                else
                                {
                                        LOG_WARN("网关收到大厅切场景消息，但消息解析失败!!!");
                                }
                        }
                        break;
                default :
                        player->Send2Client<MsgHeaderType>(bufRef, buf, msgHead._size, msgHead.CompressType(), mainType, subType);
                        break;
                }
        }
        else
        {
                DLOG_WARN("玩家[{}] 收到大厅消息，但玩家没找到!!!", msgHead._from);
        }
}
#endif

}; // end of namespace nl::af::impl

// vim: fenc=utf8:expandtab:ts=8
