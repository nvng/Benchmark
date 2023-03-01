#pragma once

#if 0
class ClientGateSession : public ActorAgentSession<TcpSessionClient, PlayerMgr>
{
	typedef ActorAgentSession<TcpSessionClient, PlayerMgr> SuperType;
public :
	void OnConnect() override;

public :
	static double GetAvgCostTime()
	{
		double sum = 0.0;
		std::lock_guard<std::mutex> l(_costTimeListMutex);
		for (auto& val : _costTimeList)
			sum += val;
		return sum / _costTimeList.size();
	}

private :
	std::unordered_map<uint64_t, double> _sendTimeList;
	static std::mutex _costTimeListMutex;
	static std::deque<double> _costTimeList;

public :
	EXTERN_MSG_HANDLE();
};
#endif

class Player;
class ClientGateSession : public TcpSessionClient<ClientGateSession, nl::af::impl::MsgHeaderClient, Compress::ECompressType::E_CT_ZLib>
{
	typedef TcpSessionClient<ClientGateSession, nl::af::impl::MsgHeaderClient, Compress::ECompressType::E_CT_ZLib> SuperType;
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
