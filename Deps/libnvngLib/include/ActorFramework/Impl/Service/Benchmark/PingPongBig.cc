#include "PingPongBig.h"

#if defined (PING_PONG_BIG_SERVICE_CLIENT) || defined (PING_PONG_BIG_SERVICE_SERVER)

/*
SERVICE_NET_HANDLE(PingPongBigService::SessionType, 0xfff, 0x0, MsgClientLogin)
{
        ++GetApp()->_cnt;
        SendPB(msg
               , 0xfff, 0x0
               , PingPongBigService::SessionType::MsgHeaderType::E_RMT_Send
               , 0, 0, 0);
}
*/

#endif

#ifdef PING_PONG_BIG_SERVICE_CLIENT

ACTOR_MAIL_HANDLE(PingPongBigActor, 0xfff, 0x0)
{
        boost::this_fiber::sleep_for(std::chrono::seconds(10));

        auto msg = std::make_shared<MsgClientLogin>();
        msg->set_nick_name(GenRandStr(1024 * 100));

        for (int64_t i=0; i<PingPongBigService::scSessionCnt * 4; ++i)
        {
                auto ses = PingPongBigService::GetInstance()->DistSession(i);
                for (int64_t j=0; j<PingPongBigService::scPkgPerSession; ++j)
                {
                        ses->SendPB(msg
                                    , 0xfff, 0x0
                                    , PingPongBigService::SessionType::MsgHeaderType::E_RMT_Send
                                    , 0, 0, 0);
                }
        }

        return nullptr;
}

#endif
