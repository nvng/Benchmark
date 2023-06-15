#include "ClientGateSession.h"

#include "Player/Player.h"

ClientGateSession::ClientGateSession()
{
}

void ClientGateSession::OnConnect()
{
	// DLOG_INFO("连接成功!!!");
	SuperType::OnConnect();
	++GetApp()->_cnt;

	MsgClientLogin msg;
	static std::atomic_int64_t cnt = 0;
	auto playerGuid = PlayerMgr::GetInstance()->_idList[++cnt % PlayerMgr::GetInstance()->_idList.size()];
	msg.set_player_guid(playerGuid);
	SendPB(&msg, E_MCMT_ClientCommon, E_MCCCST_Login);

#if 1
	if (0 != RandInRange(0, 3))
	{
		TcpSessionWeakPtr weakSes = shared_from_this();
		GetSteadyTimer().StartWithRelativeTimeOnce(RandInRange(0.0, 40.0), [weakSes](TimedEventItem& eventData) {
			auto ses = weakSes.lock();
			if (ses)
				ses->Close(1024);
		});
	}
	else
	{
		Close(1025);
	}
#endif
}

void ClientGateSession::OnClose(int32_t reasonType)
{
	std::weak_ptr<ClientGateSession> weakSes = shared_from_this();
	GetSteadyTimer().StartWithRelativeTimeOnce(RandInRange(0.0, 4.0), [weakSes, reasonType](TimedEventItem& eventData) {
		auto ses = weakSes.lock();
		if (ses)
		{
			// DLOG_INFO("连接关闭!!! reasonType[{}]", reasonType);
			ses->SuperType::OnClose(reasonType);

			auto p = ses->_player.lock();
			if (p)
			{
				p->Terminate();
				p->WaitForTerminate();
				PlayerMgr::GetInstance()->RemoveActor(p);
			}
		}
	});
}

void ClientGateSession::OnRecv(const MsgHeaderType& msgHead, evbuffer* evbuf)
{
	switch (msgHead._type)
	{
	case 0 :
		SuperType::OnRecv(msgHead, evbuf);
		break;
#if 0
	case MsgHeaderType::MsgTypeMerge<0x7f, 0>() :
		{
			SendPB(nullptr, 0x7f, 0);
			int64_t i = 0;
			for (; i<0; ++i)
				SendPB(nullptr, 0x7f, 1);
			GetApp()->_cnt += i + 1;
		}
		break;
	case MsgHeaderType::MsgTypeMerge<0x7f, 1>() :
		GetApp()->_cnt += 1;
		break;
	case MsgHeaderType::MsgTypeMerge<E_MCMT_GameCommon, E_MCGCST_SwitchRegion>() :
		SendPB(nullptr, E_MCMT_GameCommon, E_MCGCST_LoadFinish);
		// SendPB(nullptr, 0x7f, 0);
		break;
#endif
	default :
                if (true)
                {
                        /*
                           LOG_INFO("收到网关消息 mt[{:#x}] st[{:#x}] size[{}]",
                                   MsgHeaderType::MsgMainType(msgHead.type_),
                                   MsgHeaderType::MsgSubType(msgHead.type_),
                                   msgHead.size_);
                           */
                        auto p = _player.lock();
                        if (p)
                        {
                                auto mail = std::make_shared<ActorNetMail<ActorMail, MsgHeaderType>>(nullptr, msgHead, evbuf);
                                p->Push(mail);
                        }
                        else
                        {
                                SuperType::OnRecv(msgHead, evbuf);
                        }
                }
		break;
	}
}

// vim: fenc=utf8:expandtab:ts=8
