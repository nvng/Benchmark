#include "ClientGateSession.h"

#include "Player/Player.h"

#if 0
static std::atomic_int64_t _guid = 0;

std::mutex ClientGateSession::_costTimeListMutex;
std::deque<double> ClientGateSession::_costTimeList;

void ClientGateSession::OnConnect()
{
	SuperType::OnConnect();

	// for (int i=0; i<1000 * 90; ++i)
	for (int i=0; i<1000; ++i)
	// for (int i=0; i<1; ++i)
	{
		auto p = std::make_shared<Player>(1000 * 1000 * 10 + ++_guid);
		p->_ses = this;
		p->Start();
	}
}
#endif

ClientGateSession::ClientGateSession()
{
}

void ClientGateSession::OnConnect()
{
	// DLOG_INFO("连接成功!!!");
	SuperType::OnConnect();
	++GetApp()->_cnt;

	MsgClientLogin msg;
	if (UINT64_MAX == _playerGuid)
		_playerGuid = Player::GenPlayerGuid();
	msg.set_player_guid(_playerGuid);
	SendPB(&msg, E_MCMT_Client, E_MCCST_Login);

	if (0 != RandInRange(0, 10))
	{
		TcpSessionWeakPtr weakSes = shared_from_this();
		GetSteadyTimer().StartWithRelativeTimeOnce(RandInRange(0.01, 40.0), [weakSes](TimedEventItem& eventData) {
			auto ses = weakSes.lock();
			if (ses)
				ses->Close(1024);
		});
	}
	else
	{
		Close(1025);
	}
}

void ClientGateSession::OnClose(int32_t reasonType)
{
	// DLOG_INFO("连接关闭!!! reasonType[{}]", reasonType);
	SuperType::OnClose(reasonType);

	auto p = _player.lock();
	if (p)
	{
		p->Terminate();
		p->WaitForTerminate();
		PlayerMgr::GetInstance()->RemoveActor(p);
	}
}

void ClientGateSession::OnRecv(const MsgHeaderType& msgHead, evbuffer* evbuf)
{
	switch (msgHead.type_)
	{
	case 0 :
		SuperType::OnRecv(msgHead, evbuf);
		break;
	case MsgHeaderType::MsgTypeMerge<0x7f, 0>() :
		{
			SendPB(nullptr, 0x7f, 0);
			int64_t i = 0;
			for (; i<10; ++i)
				SendPB(nullptr, 0x7f, 1);
			GetApp()->_cnt += i + 1;
		}
		break;
	case MsgHeaderType::MsgTypeMerge<0x7f, 1>() :
		break;
	default :
		{
			// LOG_INFO("收到网关消息 mt[{:#x}] st[{:#x}] size[{}]", MsgHeaderType::MsgMainType(msgHead.type_), MsgHeaderType::MsgSubType(msgHead.type_), msgHead.size_);
			auto p = _player.lock();
			if (p)
			{
				p->SendPush(nullptr, msgHead.MainType(), msgHead.SubType(), stNetBufferActorMailData::Create(evbuf, msgHead.size_, sizeof(MsgHeaderType), msgHead.CompressType()));
			}
			else
			{
				SuperType::OnRecv(msgHead, evbuf);
			}
		}
		break;
	}
}
