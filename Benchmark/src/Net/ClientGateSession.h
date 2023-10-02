#pragma once

class Player;
class ClientGateSession : public ::nl::net::client::TcpSession<ClientGateSession
                          , SocketWapper<boost::asio::ip::tcp::socket, false, false>
                          , false
                          , MsgHeaderClient
                          , Compress::ECompressType::E_CT_ZLib>
{
        typedef ::nl::net::client::TcpSession<ClientGateSession
                , SocketWapper<boost::asio::ip::tcp::socket, false, false>
                , false
                , MsgHeaderClient
                , Compress::ECompressType::E_CT_ZLib> SuperType;
public :
        typedef SuperType::Tag Tag;

	ClientGateSession(typename SuperType::SocketType&& s)
                : SuperType(std::move(s))
        {
                SuperType::DelAutoReconnect();
        }

	void OnConnect() override;
	void OnClose(int32_t reasonType) override;

        void OnRecv(SuperType::BuffTypePtr::element_type* buf, const SuperType::BuffTypePtr& bufRef) override;

public :
	std::weak_ptr<Player> _player;

	DECLARE_SHARED_FROM_THIS(ClientGateSession);
	EXTERN_MSG_HANDLE();
};

typedef std::shared_ptr<ClientGateSession> ClientGateSessionPtr;
typedef std::weak_ptr<ClientGateSession> ClientGateSessionWeakPtr;

// vim: fenc=utf8:expandtab:ts=8
