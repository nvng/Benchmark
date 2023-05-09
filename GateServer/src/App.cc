#include "App.h"

#include <malloc.h>

/*
void GateLobbySession::OnRecv(const MsgHeaderType& msgHead, evbuffer* evbuf)
{
        switch (msgHead.type_)
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
                        const auto mainType = MsgHeaderType::MsgMainType(msgHead.type_);
                        const auto subType  = MsgHeaderType::MsgSubType(msgHead.type_);
                        // DLOG_INFO("GateLobbySession recv mainType:{:#x} subType:{:#x} sesCnt:{}", mainType, subType, NetMgr::GetInstance()->GetSessionCnt());

                        PlayerPtr player;
                        switch (msgHead.type_)
                        {
                        case MsgHeaderType::MsgTypeMerge<E_MCMT_ClientCommon, E_MCCCST_Login>() :
                                player = PlayerMgr::GetInstance()->GetLoginPlayer(msgHead.to_, msgHead.from_);
                                DLOG_WARN_IF(!player, "网关收到大厅消息，但玩家[{}] 没找到!!! mt[{:#x}] st[{:#x}]", msgHead.to_, mainType, subType);
                                break;
                        default :
                                player = GetLobbyPlayer(msgHead.to_, msgHead.from_);
                                DLOG_WARN_IF(!player, "网关收到大厅消息，但玩家[{}] 没找到!!! mt[{:#x}] st[{:#x}]", msgHead.to_, mainType, subType);
                                break;
                        }

                        if (player)
                        {
                                switch (msgHead.type_)
                                {
                                case MsgHeaderType::MsgTypeMerge<E_MCMT_ClientCommon, E_MCCCST_Login>() :
                                        {
                                                auto clientSes = player->_clientSes.lock();
                                                if (clientSes)
                                                {
                                                        if (E_CET_InLogin != msgHead.guid_)
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
                                                                        player->Send2Client(evbuf, msgHead.size_-sizeof(MsgHeaderType), msgHead.CompressType(), mainType, subType);

									static std::string str = []() {
										std::string ret;
										for (int64_t i=0; i<100; ++i)
											ret += "a";
										return ret;
									}();
									MsgClientLogin msg;
									msg.set_nick_name(str);
									player->Send2Lobby(&msg, 0x7f, 0);
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
                                                                player->Send2Client(evbuf, msgHead.size_-sizeof(MsgHeaderType), msgHead.CompressType(), mainType, subType);
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
                                                LOG_FATAL_IF(!rp, "4444444444444444444444444444444 guid:{}",  msgHead.guid_);
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
                                                        tmpP->Send2Client(evbuf, msgHead.size_-sizeof(MsgHeaderType), msgHead.CompressType(), mainType, subType);
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
                                                auto buf = evbuffer_pullup(evbuf, msgHead.size_);
                                                MailSwitchRegion msg;
                                                bool ret = Compress::UnCompressAndParseAlloc(static_cast<Compress::ECompressType>(msgHead.ct_), msg, buf+sizeof(MsgHeaderType), msgHead.size_-sizeof(MsgHeaderType));
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
                                                evbuffer_remove_buffer(evbuf, sendBuf, msgHead.size_-sizeof(MsgHeaderType));
                                                SendBufRef(sendBuf, static_cast<Compress::ECompressType>(msgHead.ct_),
                                                           0x7f, 0, MsgHeaderType::E_RMT_Send, 0, player->GetUniqueID(), player->GetID());
                                                for (int64_t i=0; i<300; ++i)
                                                        SendBufRef(sendBuf, static_cast<Compress::ECompressType>(msgHead.ct_),
                                                                   0x7f, 1, MsgHeaderType::E_RMT_Send, 0, player->GetUniqueID(), player->GetID());
                                                evbuffer_free(sendBuf);
                                        }
					break;
				case MsgHeaderType::MsgTypeMerge<0x7f, 1>() :
					break;
                                default :
                                        evbuffer_drain(evbuf, sizeof(MsgHeaderType));
                                        player->Send2Client(evbuf, msgHead.size_-sizeof(MsgHeaderType), msgHead.CompressType(), mainType, subType);
                                        break;
                                }
                        }
                        else
                        {
                                DLOG_ERROR("玩家[{}] 收到大厅消息，但玩家没找到!!!", msgHead.from_);
                        }
                }
                break;
        }
}
*/

AppBase* GetAppBase()
{
	return App::GetInstance();
}

App* GetApp()
{
	return App::GetInstance();
}

App::App(const std::string& appName)
  : SuperType(appName)
{
        ServerListCfgMgr::CreateInstance();
	ServerCfgMgr::CreateInstance();

	NetProcMgr::CreateInstance();
	NetMgr::CreateInstance();
	PlayerMgr::CreateInstance();
}

App::~App()
{
	NetProcMgr::DestroyInstance();
	NetMgr::DestroyInstance();
	PlayerMgr::DestroyInstance();

        ServerListCfgMgr::DestroyInstance();
	ServerCfgMgr::DestroyInstance();
}

bool App::Init()
{
        LOG_FATAL_IF(!ServerListCfgMgr::GetInstance()->Init("./config_cx/server_list.json"), "服务器列表初始化失败!!!");
        LOG_FATAL_IF(!ServerCfgMgr::GetInstance()->Init("./config_cx/server_cfg.json"), "服务器配置初始化失败!!!");

        ServerListCfgMgr::GetInstance()->_gateServerList.Foreach([](const stGateServerInfoPtr& sInfo) {
                if (GetApp()->_gateInfo)
                        return;
                auto ip = GetIP(AF_INET, sInfo->_faName);
		if (ip == sInfo->_ip
		    && RunShellCmd(fmt::format("netstat -apn | grep 0.0.0.0:{}", sInfo->_client_port)).empty())
			GetApp()->_gateInfo = sInfo;
        });
	LOG_FATAL_IF(!_gateInfo, "server info not found!!!");
	_gateCfg = ServerCfgMgr::GetInstance()->_gateCfg;

	LOG_FATAL_IF(!SuperType::Init(ServerListCfgMgr::GetInstance()->_rid, _gateInfo->_sid, 0), "super init fail!!!");
	LOG_FATAL_IF(!NetProcMgr::GetInstance()->Init(2, "Net"), "net proc mgr init fail!!!");
	LOG_FATAL_IF(!NetMgr::GetInstance()->Init(), "net mgr init fail!!!");
	LOG_FATAL_IF(!PlayerMgr::GetInstance()->Init(), "player mgr init fail!!!");

	int64_t idx = 0;
	auto proc = NetProcMgr::GetInstance()->Dist(++idx);
	// proc->StartListener("0.0.0.0", _gateInfo->_client_port, _gateCfg->_crt, _gateCfg->_key, []() { return CreateSession<GateClientSession>(); });
	proc->StartListener("0.0.0.0", _gateInfo->_client_port, "", "", []() { return CreateSession<GateClientSession>(); });

	ServerListCfgMgr::GetInstance()->_lobbyServerList.Foreach([&idx](const stLobbyServerInfoPtr& sInfo) {
	  auto proc = NetProcMgr::GetInstance()->Dist(++idx);
	  proc->Connect(sInfo->_ip, sInfo->_gate_port, false, []() { return CreateSession<GateLobbySession>(); });
	});

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

		LOG_INFO_IF(true, "sesCnt[{}] pCnt[{}] lpCnt[{}] gpCnt[{}] avg[{}]",
			    NetMgr::GetInstance()->GetSessionCnt(),
			    PlayerMgr::GetInstance()->GetPlayerCnt(),
			    lobbyPlayerCnt,
			    gamePlayerCnt,
			    GetFrameController().GetAverageFrameCnt()
			   );

		// malloc_trim(0);
	});

	struct stTest
	{
		int64_t t = 0;
	};

	CREATE_ACTOR_CALL_MAIL(stTest, r, nullptr, 0, 0);
	r.t = 123;
	LOG_INFO("111111111111 t:{}", ACTOR_MAIL(r)->_data.t);

	CREATE_ACTOR_CALL_MAIL(MsgResult, ret, nullptr, 0, 0);
	ret.set_error_type(E_CET_CfgNotFound);
	LOG_INFO("222222222222 t:{}", ACTOR_MAIL(ret)->_data.error_type());


	return true;
}

void App::Stop()
{
	NetProcMgr::GetInstance()->Terminate();

	NetProcMgr::GetInstance()->WaitForTerminate();

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
