#include "ClientGateSession.h"

#include "Player/Player.h"
#include "Tools/TimerMgr.hpp"
#include "msg_jump.pb.h"

void ClientGateSession::OnConnect()
{
	DLOG_INFO("连接成功!!!");
	SuperType::OnConnect();
	++GetApp()->_cnt;

        /*
        auto msg = std::make_shared<MsgClientLogin>();
	static std::atomic_int64_t cnt = 0;
	auto playerGuid = PlayerMgr::GetInstance()->_idList[++cnt % PlayerMgr::GetInstance()->_idList.size()];
	msg->set_player_guid(playerGuid);
	SendPB(msg, E_MCMT_ClientCommon, E_MCCCST_Login);
        */

#if 0
	if (0 != RandInRange(0, 3))
	{
                std::weak_ptr<ClientGateSession> weakSes = shared_from_this();
                SteadyTimer::StaticStart(RandInRange(0, 61), [weakSes]() {
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
        DLOG_INFO("OnClose reasonType:{}", reasonType);
        SuperType::OnClose(reasonType);

        auto p = _player.lock();
        if (p)
                p->OnDisconnect();

	std::weak_ptr<ClientGateSession> weakSes = shared_from_this();
        SteadyTimer::StaticStart(RandInRange(0, 61), [
                                 ip{_connectEndPoint.address().to_string()},
                                 port{_connectEndPoint.port()}
        ]() {
                NetMgrBase<ClientGateSession::Tag>::GetInstance()->Connect(ip, port, [](auto&& s) {
                        return std::make_shared<ClientGateSession>(std::move(s));
                });
        });
}

void ClientGateSession::OnRecv(SuperType::BuffType buf, const VoidPtr& bufRef)
{
        auto msgHead = *reinterpret_cast<MsgHeaderType*>(buf);
	switch (msgHead._type)
	{
	case 0 :
		SuperType::OnRecv(buf, bufRef);
		break;
	case MsgHeaderType::MsgTypeMerge<0x7f, 0>() :
		{
			SendPB(nullptr, 0x7f, 0);
                        /*
			int64_t i = 0;
			for (; i<200; ++i)
				SendPB(nullptr, 0x7f, 1);
			GetApp()->_cnt += i + 1;
                        */
		}
		break;
	case MsgHeaderType::MsgTypeMerge<0x7f, 1>() :
		GetApp()->_cnt += 1;
		break;
                /*
	case MsgHeaderType::MsgTypeMerge<E_MCMT_GameCommon, E_MCGCST_SwitchRegion>() :
		SendPB(nullptr, E_MCMT_GameCommon, E_MCGCST_LoadFinish);
		// SendPB(nullptr, 0x7f, 0);
                */
		break;
        case MsgHeaderType::MsgTypeMerge<E_MCMT_Game, Jump::E_MCGST_Sync>() :
                ++GetApp()->_recvCnt;
                /*
                {
                        SendBuf<MsgHeaderType>(bufRef, buf, msgHead._size, msgHead.CompressType(), E_MCMT_Game, Jump::E_MCGST_Sync);
                        auto p = _player.lock();
                        auto msg = std::make_shared<Jump::MsgSync>();
                        msg->set_player_guid(p ? p->GetID() : 0);
                        for (int64_t i=0; i<200; ++i)
                                SendPB(msg, E_MCMT_Game, Jump::E_MCGST_SyncCommon);
                }
                */
                break;
        // case MsgHeaderType::MsgTypeMerge<E_MCMT_Game, Jump::E_MCGST_SyncCommon>() :
                // SendBuf<MsgHeaderType>(bufRef, buf, msgHead._size, msgHead.CompressType(), E_MCMT_Game, Jump::E_MCGST_SyncCommon);
                break;
        case MsgHeaderType::MsgTypeMerge<E_MCMT_Shop, E_MCSST_Refresh>() :
                SendBuf<MsgHeaderType>(bufRef, buf, msgHead._size, msgHead.CompressType(), E_MCMT_Shop, E_MCSST_Refresh);
                break;
	default :
                if (false)
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
                                auto tmpMsgHeader = msgHead;
                                tmpMsgHeader._type = Player::ActorMailType::MsgTypeMerge(MsgHeaderType::MsgMainType(msgHead._type), MsgHeaderType::MsgSubType(msgHead._type));
                                /*
                                LOG_INFO("1111111 type[{:#x}] newType[{:#x}] mt[{:#x}] st[{:#x}]",
                                         msgHead._type, tmpMsgHeader._type, MsgHeaderType::MsgMainType(msgHead._type), MsgHeaderType::MsgSubType(msgHead._type));
                                         */
                                auto mail = std::make_shared<nl::net::ActorNetMail<ActorMail, MsgHeaderType>>(nullptr, tmpMsgHeader, buf, bufRef);
                                p->Push(mail);
                        }
                        else
                        {
                                SuperType::OnRecv(buf, bufRef);
                        }
                }
		break;
	}
}

// vim: fenc=utf8:expandtab:ts=8
