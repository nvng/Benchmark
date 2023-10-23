#pragma once

namespace nl::af::impl {

struct stMsgRecvInfo
{
        stMsgRecvInfo(uint8_t maxCnt, uint16_t minInterval)
                : _maxCnt(maxCnt), _minInterval(minInterval)
        {
        }

        time_t _time = 0;
        uint8_t _cnt = 0;
        const uint8_t _maxCnt = 0;
        const uint16_t _minInterval = 0;
};
typedef std::shared_ptr<stMsgRecvInfo> stMsgRecvInfoPtr;

class Player;
class GateClientSession : public ::nl::net::client::TcpSession<GateClientSession
#if !defined(____CLIENT_USE_WS____) && !defined(____CLIENT_USE_WSS____)
                          , ::nl::net::client::SocketWapper<boost::asio::ip::tcp::socket, true, false>
#else
                          , ::nl::net::client::WebsocketWapper<true, false>
#endif
                          , true
                          , MsgHeaderClient
                          , Compress::ECompressType::E_CT_ZLib>
{
        typedef ::nl::net::client::TcpSession<GateClientSession
#if !defined(____CLIENT_USE_WS____) && !defined(____CLIENT_USE_WSS____)
                , ::nl::net::client::SocketWapper<boost::asio::ip::tcp::socket, true, false>
#else
                , ::nl::net::client::WebsocketWapper<true, false>
#endif
                , true
                , MsgHeaderClient
                , Compress::ECompressType::E_CT_ZLib> SuperType;

public :
        explicit GateClientSession(typename SuperType::SocketType&& s);
        ~GateClientSession() override;

	void OnConnect() override;
	void OnClose(int32_t reasonType) override;
        static void Msg_Handle_ServerHeartBeat(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef);
        void OnRecv(SuperType::BuffTypePtr::element_type* buf, const SuperType::BuffTypePtr& bufRef) override;

public :
	std::weak_ptr<Player> _player_;
        std::unordered_map<int64_t, stMsgRecvInfoPtr> _lastMsgRecvTimeList;

        DECLARE_SHARED_FROM_THIS(GateClientSession);
};

bool CheckMsgRecvTimeIllegal(GateClientSession* ses, uint64_t mt, uint64_t st);
void RegistCheckIllegalMsgTime(GateClientSession* ses, uint64_t mt, uint64_t st, uint8_t maxCnt, uint16_t maxInterval);
void DealClientMessage(const std::shared_ptr<GateClientSession>& clientSes, evbuffer* evbuf);

}; // end of namespace nl::af::impl

// vim: fenc=utf8:expandtab:ts=8
