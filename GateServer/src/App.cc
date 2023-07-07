#include "App.h"

#include <malloc.h>

#ifdef ____USE_IMPL_GATE_LOBBY_SESSION_ONRECV___
void GateLobbySession::OnRecv(const MsgHeaderType& msgHead, evbuffer* evbuf)
{
        switch (msgHead._type)
        {
        case MsgHeaderType::MsgTypeMerge<E_MIMT_Internal, E_MIIST_HeartBeat>() :
        case MsgHeaderType::MsgTypeMerge<E_MIMT_Internal, E_MIIST_ServerInit>() :
                SuperType::OnRecv(msgHead, evbuf);
                break;
        case MsgHeaderType::MsgTypeMerge<E_MIMT_Internal, E_MIIST_MultiCast>() :
        case MsgHeaderType::MsgTypeMerge<E_MIMT_Internal, E_MIIST_BroadCast>() :
                OnMessageMultiCast(msgHead, evbuf);
                break;
        default :
                {
                        const auto mainType = MsgHeaderType::MsgMainType(msgHead._type);
                        const auto subType  = MsgHeaderType::MsgSubType(msgHead._type);
                        // DLOG_INFO("GateLobbySession recv mainType:{:#x} subType:{:#x} sesCnt:{}", mainType, subType, NetMgr::GetInstance()->GetSessionCnt());

                        PlayerPtr player;
                        switch (msgHead._type)
                        {
                        case MsgHeaderType::MsgTypeMerge<E_MCMT_ClientCommon, E_MCCCST_Login>() :
                                player = PlayerMgr::GetInstance()->GetLoginPlayer(msgHead._to, msgHead._from);
                                DLOG_WARN_IF(!player, "网关收到大厅消息，但玩家[{}] 没找到!!! mt[{:#x}] st[{:#x}]", msgHead._to, mainType, subType);
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
                                                        if (E_CET_InLogin != msgHead._guid)
                                                        {
#ifdef ____BENCHMARK____
                                                                bool playerMgrRet = PlayerMgr::GetInstance()->AddPlayer(player);
                                                                LOG_FATAL_IF(!playerMgrRet, "2222222222222222222222222");
                                                                bool sesRet = AddPlayer(player);
                                                                LOG_FATAL_IF(!sesRet, "3333333333333333333333333");
                                                                if (playerMgrRet && sesRet)
#else
                                                                if (PlayerMgr::GetInstance()->AddPlayer(player) && AddPlayer(player))
#endif
                                                                {
                                                                        clientSes->_player_ = player;
                                                                        evbuffer_drain(evbuf, sizeof(MsgHeaderType));
                                                                        player->Send2Client(evbuf, msgHead._size-sizeof(MsgHeaderType), msgHead.CompressType(), mainType, subType);

									static std::string str = []() {
										std::string ret;
										for (int64_t i=0; i<230; ++i)
											ret += "a";
										return ret;
									}();
									MsgClientLogin msg;
									msg.set_nick_name(str);
									player->Send2Lobby(&msg, 0x7f, 0);
									// player->Send2Lobby(nullptr, 0x7f, 0);
                                                                }
                                                                else
                                                                {
#ifdef ____BENCHMARK____
                                                                        auto t = PlayerMgr::GetInstance()->GetPlayer(player->GetUniqueID(), player->GetID());
                                                                        auto t1 = GetLobbyPlayer(player->GetUniqueID(), player->GetID());
                                                                        LOG_FATAL("玩家[{}] 登录返回后，添加到 GateLobbySession 失败!!! player[{}] t[{}] t1[{}]",
                                                                                 player->GetID(), (void*)player.get(), (void*)t.get(), (void*)t1.get());
#else
                                                                        LOG_WARN("玩家[{}] 登录返回后，添加到 GateLobbySession 失败!!!", player->GetID());
#endif
                                                                        MsgResult msg;
                                                                        msg.set_error_type(E_CET_Fail);
                                                                        player->Send2Client(&msg, E_MCMT_ClientCommon, E_MCCCST_Result);
                                                                        PlayerMgr::GetInstance()->RemovePlayer(player->GetID(), player.get());
                                                                }
                                                        }
                                                        else
                                                        {
                                                                evbuffer_drain(evbuf, sizeof(MsgHeaderType));
                                                                player->Send2Client(evbuf, msgHead._size-sizeof(MsgHeaderType), msgHead.CompressType(), mainType, subType);
                                                                player->ResetLobbySes();
                                                                clientSes->AsyncClose(1006);
                                                        }
                                                }
                                                else
                                                {
                                                        DLOG_WARN("玩家[{}] 登录返回后，clientSes已经释放!!! shared_ptr:{}", player->GetID(), player.use_count());
                                                }
                                                auto rp = PlayerMgr::GetInstance()->RemoveLoginPlayer(player->GetID(), player.get());
                                                (void)rp;
#ifdef ____BENCHMARK____
                                                LOG_FATAL_IF(!rp, "4444444444444444444444444444444 guid:{}",  msgHead._guid);
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
                                                        evbuffer_drain(evbuf, sizeof(MsgHeaderType));
                                                        tmpP->Send2Client(evbuf, msgHead._size-sizeof(MsgHeaderType), msgHead.CompressType(), mainType, subType);
                                                        auto ses = tmpP->_clientSes.lock();
                                                        if (ses)
                                                                ses->AsyncClose(1003);
                                                        return;
                                                }
                                                return;
                                        }
                                        break;
                                case MsgHeaderType::MsgTypeMerge<E_MCMT_GameCommon, E_MCGCST_SwitchRegion>() :
                                        {
                                                auto buf = evbuffer_pullup(evbuf, msgHead._size);
                                                MailSwitchRegion msg;
                                                bool ret = Compress::UnCompressAndParseAlloc(static_cast<Compress::ECompressType>(msgHead._ct), msg, buf+sizeof(MsgHeaderType), msgHead._size-sizeof(MsgHeaderType));
                                                if (ret)
                                                {
                                                        MsgSwitchRegion msgSwitchRegion;
                                                        msgSwitchRegion.set_error_type(E_CET_Success);
                                                        msgSwitchRegion.set_region_type(msg.region_type());
                                                        msgSwitchRegion.set_old_region_type(msg.old_region_type());

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
                                                                auto gameSes = NetMgr::GetInstance()->DistGameSession(msg.game_sid(), player->GetID());
                                                                if (gameSes)
                                                                {
                                                                        if (gameSes->AddPlayer(player))
                                                                        {
                                                                                player->_regionGuid = msg.region_guid();
                                                                                player->_gameSes = gameSes;
                                                                        }
                                                                        else
                                                                        {
                                                                                // TODO: 错误处理。
                                                                                LOG_ERROR("玩家[{}] 收到切场景消息，但 gameSes player 添加失败!!!", player->GetID());
                                                                                msgSwitchRegion.set_error_type(E_CET_Fail);
                                                                        }
                                                                }
                                                                else
                                                                {
                                                                        // TODO: 错误处理。
                                                                        LOG_ERROR("网关收到大厅切场景消息，但 gameSes 分配失败!!! sid:{}", msg.game_sid());
                                                                        msgSwitchRegion.set_error_type(E_CET_Fail);
                                                                }
                                                        }

                                                        player->Send2Client(&msgSwitchRegion, E_MCMT_GameCommon, E_MCGCST_SwitchRegion);
                                                }
                                                else
                                                {
                                                        LOG_ERROR("网关收到大厅切场景消息，但消息解析失败!!!");
                                                }
                                        }
                                        break;
				case MsgHeaderType::MsgTypeMerge<0x7f, 0>() :
                                        {
                                                evbuffer_drain(evbuf, sizeof(MsgHeaderType));
                                                evbuffer* sendBuf = evbuffer_new();
                                                evbuffer_remove_buffer(evbuf, sendBuf, msgHead._size-sizeof(MsgHeaderType));
                                                SendBufRef(sendBuf, static_cast<Compress::ECompressType>(msgHead._ct),
                                                           0x7f, 0, MsgHeaderType::E_RMT_Send, 0, player->GetUniqueID(), player->GetID());
                                                for (int64_t i=0; i<100; ++i)
                                                        SendBufRef(sendBuf, static_cast<Compress::ECompressType>(msgHead._ct),
                                                                   0x7f, 1, MsgHeaderType::E_RMT_Send, 0, player->GetUniqueID(), player->GetID());
                                                evbuffer_free(sendBuf);
                                        }
					break;
				case MsgHeaderType::MsgTypeMerge<0x7f, 1>() :
					break;
                                default :
                                        evbuffer_drain(evbuf, sizeof(MsgHeaderType));
                                        player->Send2Client(evbuf, msgHead._size-sizeof(MsgHeaderType), msgHead.CompressType(), mainType, subType);
                                        break;
                                }
                        }
                        else
                        {
                                DLOG_ERROR("玩家[{}] 收到大厅消息，但玩家没找到!!!", msgHead._from);
                        }
                }
                break;
        }
}
#endif

AppBase* GetAppBase()
{
	return App::GetInstance();
}

App* GetApp()
{
	return App::GetInstance();
}

App::App(const std::string& appName)
  : SuperType(appName, E_ST_Gate)
{
	NetMgr::CreateInstance();
	PlayerMgr::CreateInstance();
}

App::~App()
{
	NetMgr::DestroyInstance();
	PlayerMgr::DestroyInstance();
}

bool App::Init()
{
	LOG_FATAL_IF(!SuperType::Init(), "super init fail!!!");
	_gateCfg = ServerCfgMgr::GetInstance()->_gateCfg;

	LOG_FATAL_IF(!NetMgr::GetInstance()->Init(), "net mgr init fail!!!");
	LOG_FATAL_IF(!PlayerMgr::GetInstance()->Init(), "player mgr init fail!!!");

	GetSteadyTimer().StartWithRelativeTimeForever(1.0, [](TimedEventItem& eventData) {
		std::size_t lobbyPlayerCnt = 0;
		std::size_t gamePlayerCnt = 0;
		NetMgr::GetInstance()->Foreach([&lobbyPlayerCnt, &gamePlayerCnt](const auto& ses) {
			auto lobbySes = std::dynamic_pointer_cast<GateLobbySession>(ses);
			if (lobbySes)
				lobbyPlayerCnt += lobbySes->GetPlayerCnt();
			auto gameSes = std::dynamic_pointer_cast<GateGameSession>(ses);
			if (gameSes)
				gamePlayerCnt += gameSes->GetPlayerCnt();
		});

                static int64_t clientRecvCnt = 0;
                static int64_t serverRecvCnt = 0;
		LOG_INFO_IF(true, "sesCnt[{}] pCnt[{}] lpCnt[{}] gpCnt[{}] crc[{}] src[{}] avg[{}]",
			    NetMgr::GetInstance()->GetSessionCnt(),
			    PlayerMgr::GetInstance()->GetPlayerCnt(),
			    lobbyPlayerCnt,
			    gamePlayerCnt,
                            GetApp()->_clientRecvCnt - clientRecvCnt,
                            GetApp()->_serverRecvCnt - serverRecvCnt,
			    GetFrameController().GetAverageFrameCnt()
			   );

                clientRecvCnt = GetApp()->_clientRecvCnt;
                serverRecvCnt = GetApp()->_serverRecvCnt;

		// malloc_trim(0);
	});

	// {{{ start task
	_startPriorityTaskList->AddFinalTaskCallback([this]() {
		auto gateInfo = GetServerInfo<stGateServerInfo>();
#if defined(____CLIENT_USE_WS____) || defined(____CLIENT_USE_WSS____)
		WSSession::InitWS(1, gateInfo->_client_port, _gateCfg->_crt, _gateCfg->_key);
#else
		auto proc = NetProcMgr::GetInstance()->Dist(0);
		proc->StartListener("0.0.0.0", gateInfo->_client_port, _gateCfg->_crt, _gateCfg->_key, []() { return CreateSession<GateClientSession>(); });
#endif
	});

	std::vector<std::string> preTaskList;

	preTaskList.clear();
	_startPriorityTaskList->AddTask(preTaskList, GateGameSession::scPriorityTaskKey, [](const std::string& key) {
		int64_t idx = 0;
		ServerListCfgMgr::GetInstance()->Foreach<stGameServerInfo>([&idx](const stGameServerInfoPtr& sInfo) {
			auto proc = NetProcMgr::GetInstance()->Dist(++idx);
			proc->Connect(sInfo->_ip, sInfo->_gate_port, false, []() { return CreateSession<GateGameSession>(); });
		});
	});

	preTaskList.clear();
	preTaskList.emplace_back(GateGameSession::scPriorityTaskKey);
	_startPriorityTaskList->AddTask(preTaskList, GateLobbySession::scPriorityTaskKey, [](const std::string& key) {
		int64_t idx = 0;
		ServerListCfgMgr::GetInstance()->Foreach<stLobbyServerInfo>([&idx](const stLobbyServerInfoPtr& sInfo) {
			auto proc = NetProcMgr::GetInstance()->Dist(++idx);
			proc->Connect(sInfo->_ip, sInfo->_gate_port, false, []() { return CreateSession<GateLobbySession>(); });
		});
	});
	// }}}

	// {{{ stop task
	_stopPriorityTaskList->AddFinalTaskCallback([]() {
		NetProcMgr::GetInstance()->Terminate();

		NetProcMgr::GetInstance()->WaitForTerminate();
	});
	// }}}

        LOG_INFO("RRRRRRRRRRRRRRRRRRRRRRRRRRelease");
        DLOG_INFO("DDDDDDDDDDDDDDDDDDDDDDbug");

	return true;
}

void App::Stop()
{
	SuperType::Stop();
}

int main(int argc, char* argv[])
{
	App::CreateInstance(argv[0]);
	INIT_OPT();

	LOG_FATAL_IF(!App::GetInstance()->Init(), "app init error!!!");
	App::GetInstance()->Start();
	App::DestroyInstance();

	return 0;
}

// vim: fenc=utf8:expandtab:ts=8
