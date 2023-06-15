#pragma once

class Player;
class ClientGateSession : public TcpSessionClient<ClientGateSession, MsgHeaderClient, Compress::ECompressType::E_CT_ZLib>
{
	typedef TcpSessionClient<ClientGateSession, MsgHeaderClient, Compress::ECompressType::E_CT_ZLib> SuperType;
public :
	ClientGateSession();

	void OnConnect() override;
	void OnClose(int32_t reasonType) override;

        void OnRecv(const MsgHeaderType& msgHead, evbuffer* evbuf) override;

public :
	std::weak_ptr<Player> _player;

	DECLARE_SHARED_FROM_THIS(ClientGateSession);
	EXTERN_MSG_HANDLE();
};

typedef std::shared_ptr<ClientGateSession> ClientGateSessionPtr;
typedef std::weak_ptr<ClientGateSession> ClientGateSessionWeakPtr;

// vim: fenc=utf8:expandtab:ts=8
