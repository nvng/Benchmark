#include "GateClientSession.h"

#include "NetMgrImpl.h"
#include "Player.h"

#ifdef ANNOUNCEMENT_SERVICE_CLIENT
#include "AnnouncementService.h"
#endif

namespace nl::af::impl {

GateClientSession::GateClientSession(typename SuperType::SocketType&& s)
        : SuperType(std::move(s))
{
        ++GetApp()->_cnt;
}

GateClientSession::~GateClientSession()
{
        --GetApp()->_cnt;
	// NetMgrImpl::GetInstance()->RemoveSession(E_GST_Client, this);
}

void GateClientSession::OnConnect()
{
	SuperType::OnConnect();
	// NetMgrImpl::GetInstance()->AddSession(E_GST_Client, shared_from_this());
}

void GateClientSession::OnClose(int32_t reasonType)
{
        // DLOG_INFO("客户端断开连接!!! reasonType[{}]", reasonType);
        auto p = _player_.lock();
        if (p)
        {
                DLOG_INFO("客户端断开连接!!! 玩家[{}] ptr[{}] reasonType[{}]", p->GetID(), (void*)p.get(), reasonType);
                p->OnClientClose();
        }
	SuperType::OnClose(reasonType);
}

void GateClientSession::Msg_Handle_ServerHeartBeat(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
{
        auto now = GetClock().GetTime() * 1000;
        auto sendMsg = std::make_shared<MsgClientHeartBeat>();
        sendMsg->set_server_time(now);

        auto thisPtr = std::dynamic_pointer_cast<GateClientSession>(ses);
        thisPtr->SendHeartBeat(sendMsg);
        thisPtr->_lastHeartBeatTime = GetClock().GetSteadyTime();
}

void GateClientSession::OnRecv(SuperType::BuffTypePtr::element_type* buf, const SuperType::BuffTypePtr& bufRef)
{
        auto msgHead = *reinterpret_cast<MsgHeaderType*>(buf);
	if (0 == msgHead._type)
	{
		SuperType::OnRecv(buf, bufRef);
		return;
	}

        ++GetApp()->_clientRecvCnt;
        if (Compress::E_CT_None != msgHead._ct)
                msgHead._ct = GateClientSession::CompressType;

	const auto mainType = MsgHeaderType::MsgMainType(msgHead._type);
	const auto subType  = MsgHeaderType::MsgSubType(msgHead._type);
        DLOG_INFO("type[{:#x}] mainType[{:#x}] subType[{:#x}] size[{}]",
		 msgHead._type,
		 MsgHeaderType::MsgMainType(msgHead._type),
		 MsgHeaderType::MsgSubType(msgHead._type),
		 msgHead._size);

        CheckMsgRecvTimeIllegal(this, mainType, subType);

	switch (mainType)
        {
#ifdef ANNOUNCEMENT_SERVICE_CLIENT
        case E_MCMT_Announcement :
                {
                        auto cache = AnnouncementService::GetInstance()->_cache;
                        SendPB(cache, E_MCMT_Announcement, E_MCANNST_Sync);
                }
                break;
#endif
	case E_MCMT_Login :
		{
			MsgClientLoginCheck msg;
                        bool ret = Compress::UnCompressAndParseAlloc(static_cast<Compress::ECompressType>(msgHead._ct), msg, buf+sizeof(MsgHeaderType), msgHead._size-sizeof(MsgHeaderType));
                        // TODO: 重复登录，将流量卡在网关。
                        if (ret)
			{
                                std::shared_ptr<GateLoginSession> ses = NetMgrImpl::GetInstance()->DistLoginSession(msg.user_id());
				if (ses)
				{
                                        ses->SendBuf(bufRef, buf+sizeof(MsgHeaderType), msgHead._size-sizeof(MsgHeaderType),
                                                     msgHead.CompressType(), mainType, subType,
                                                     GateLoginSession::MsgHeaderType::E_RMT_Send, 0, GetID(), 0);
                                        break;
				}
				else
				{
					LOG_WARN("根据客户端 user_id[{}] 分配登录服失败!!!", msg.user_id());
				}
			}
			else
			{
				LOG_WARN("客户端发送的 登录消息 解析失败!!!");
			}
		}
		break;
        case E_MCMT_QueueCommon :
        case E_MCMT_Queue :
                {
                        auto p = _player_.lock();
                        if (p)
                        {
                                p->Send2Lobby(bufRef, buf+sizeof(MsgHeaderType), msgHead._size-sizeof(MsgHeaderType), msgHead.CompressType(), mainType, subType);
                        }
                }
                break;
        case E_MCMT_GameCommon :
                {
                        auto p = _player_.lock();
			if (p)
			{
                                switch (subType)
                                {
                                case E_MCGCST_ReqEnterRegion :
                                case E_MCGCST_ReqExitRegion :
                                case E_MCGCST_LoadFinish :
                                        p->Send2Lobby(bufRef, buf+sizeof(MsgHeaderType), msgHead._size-sizeof(MsgHeaderType), msgHead.CompressType(), mainType, subType);
                                        break;
                                default :
                                        p->Send2Game(bufRef, buf+sizeof(MsgHeaderType), msgHead._size-sizeof(MsgHeaderType), msgHead.CompressType(), mainType, subType);
                                        break;
                                }
                                break;
                        }
                        else
                        {
                                DLOG_WARN("客户端发送消息给 game server 时，该连接对应玩家已经不存在!!!");
                        }
                }
                break;
        case E_MCMT_Game :
                {
                        auto p = _player_.lock();
			if (p)
			{
                                p->Send2Game(bufRef, buf+sizeof(MsgHeaderType), msgHead._size-sizeof(MsgHeaderType), msgHead.CompressType(), mainType, subType);
                        }
                        else
                        {
                                LOG_ERROR("客户端发送消息给 game server 时，该连接对应玩家已经不存在!!!");
                        }
                }
                break;
	default:
		{
                        auto p = _player_.lock();
			if (p)
			{
				p->Send2Lobby(bufRef, buf+sizeof(MsgHeaderType), msgHead._size-sizeof(MsgHeaderType), msgHead.CompressType(), mainType, subType);
			}
			else
                        {
                                if (MsgHeaderType::MsgTypeMerge<E_MCMT_ClientCommon, E_MCCCST_Login>() == msgHead._type)
                                {
                                        MsgClientLogin msg;
                                        bool ret = Compress::UnCompressAndParseAlloc(static_cast<Compress::ECompressType>(msgHead._ct), msg, buf+sizeof(MsgHeaderType), msgHead._size-sizeof(MsgHeaderType));
                                        if (ret)
                                        {
                                                const auto playerGuid = msg.player_guid();
                                                std::shared_ptr<GateLobbySession> lobbySes = NetMgrImpl::GetInstance()->DistLobbySession(playerGuid);
                                                if (lobbySes)
                                                {
                                                        p = std::make_shared<Player>(playerGuid, shared_from_this());
                                                        DLOG_INFO("客户端[{}] ptr[{}] 发送消息给大厅!!!", playerGuid, (void*)p.get());
                                                        if (PlayerMgr::GetInstance()->AddLoginPlayer(p))
                                                        {
                                                                ++p->_playerFlag;
                                                                p->SetLobbySes(lobbySes);
                                                                PlayerWeakPtr weakPlayer = p;
                                                                SteadyTimer::StaticStart(30, [weakPlayer]() {
                                                                        // TODO: 有多线程风险。
                                                                        auto p = weakPlayer.lock();
                                                                        if (p)
                                                                        {
#ifdef ____BENCHMARK____
                                                                                FLAG_ADD(p->_internalFlag, E_GPFT_Delete);
#endif
                                                                                auto t = PlayerMgr::GetInstance()->RemoveLoginPlayer(p->GetID(), p.get());
                                                                                if (t)
                                                                                {
                                                                                        t->ClearSessionRelation();
                                                                                        auto ses = t->_clientSes.lock();
                                                                                        if (ses)
                                                                                        {
                                                                                                ses->Close(1005);
                                                                                        }
                                                                                }
                                                                        }
                                                                });

                                                                p->Send2Lobby(bufRef, buf+sizeof(MsgHeaderType), msgHead._size-sizeof(MsgHeaderType), msgHead.CompressType(), mainType, subType);
                                                                break;
                                                        }
                                                        else
                                                        {
                                                                DLOG_WARN("玩家[{}] 添加失败!!!", playerGuid);
                                                                auto msg = std::make_shared<MsgResult>();
                                                                msg->set_error_type(E_CET_InLogin);
                                                                SendPB(msg, E_MCMT_ClientCommon, E_MCCCST_Result);
                                                                Close(1006);
                                                        }
                                                }
                                                else
                                                {
                                                        LOG_WARN("根据客户端 uuid[{}] 分配大厅失败!!!", msg.player_guid());
                                                }
                                        }
                                        else
                                        {
                                                LOG_WARN("客户发送的登录大厅消息解析失败!!!");
                                        }
                                }
                                else
                                {
                                        LOG_WARN("登录过程中收到其它消息!!! mt[{}] st[{}]", mainType, subType);
                                }
                        }
		}
		break;
	}
}

bool CheckMsgRecvTimeIllegal(GateClientSession* ses, uint64_t mt, uint64_t st)
{
        /*
        ++GetApp()->_clientRecvCnt;
        const int64_t msgType = GateClientSession::MsgHeaderType::MsgTypeMerge(mt, st);
        auto it = ses->_lastMsgRecvTimeList.find(msgType);
        if (ses->_lastMsgRecvTimeList.end() != it)
        {
                auto& info = it->second;
                const int64_t nowMilli = GetClock().GetSteadyTime() * 1000;
                if (nowMilli - info->_time > info->_minInterval)
                {
                        info->_cnt = 0;
                }
                else
                {
                        info->_cnt += 1;
                        if (info->_cnt > info->_maxCnt)
                        {
                                ses->Close(1004);
                        }
                }
                info->_time = nowMilli;
        }
        return true;
        */
        return false;
}

void RegistCheckIllegalMsgTime(GateClientSession* ses, uint64_t mt, uint64_t st, uint8_t maxCnt, uint16_t maxInterval)
{
        auto info = std::make_shared<stMsgRecvInfo>(maxCnt, maxInterval);
        const int64_t msgType = GateClientSession::MsgHeaderType::MsgTypeMerge(mt, st);
        ses->_lastMsgRecvTimeList.emplace(msgType, info);
}

}; // end of namespace nl::af::impl

// vim: fenc=utf8:expandtab:ts=8
