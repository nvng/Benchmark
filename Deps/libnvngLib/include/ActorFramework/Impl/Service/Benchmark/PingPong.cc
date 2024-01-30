#include "PingPong.h"

#if defined (PING_PONG_SERVICE_CLIENT) || defined (PING_PONG_SERVICE_SERVER)

SERVICE_NET_HANDLE(PingPongService::SessionType, 0xfff, 0x0)
{
        ++GetApp()->_cnt;
        SendPB(nullptr
               , 0xfff, 0x0
               , PingPongService::SessionType::MsgHeaderType::E_RMT_Send
               , 0, 0, 0);
}

#endif

#ifdef PING_PONG_SERVICE_CLIENT

ACTOR_MAIL_HANDLE(PingPongActor, 0xfff, 0x0)
{
        boost::this_fiber::sleep_for(std::chrono::seconds(10));
        return nullptr;

        for (int64_t i=0; i<PingPongService::scSessionCnt; ++i)
        {
                auto ses = PingPongService::GetInstance()->DistSession(i);
                for (int64_t j=0; j<PingPongService::scPkgPerSession; ++j)
                {
                        ses->SendPB(nullptr
                                    , 0xfff, 0x0
                                    , PingPongService::SessionType::MsgHeaderType::E_RMT_Send
                                    , 0, 0, 0);
                }
        }

        return nullptr;
}

#endif
