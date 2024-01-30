#pragma once

namespace nl::af::impl {

template <typename _Ty>
class GameMgrAgent : public ActorAgent<_Ty>
{
        typedef ActorAgent<_Ty> SuperType;
public :
        GameMgrAgent(uint32_t id,
                     const std::shared_ptr<_Ty>& ses,
                     ERegionType regionType)
                : SuperType(id, ses)
                  , _regionType(regionType)
        {
                // DLOG_INFO("GameMgrAgent::GameMgrAgent id:{}", SuperType::GetID());
        }

        ~GameMgrAgent() override
        {
                // DLOG_INFO("GameMgrAgent::~GameMgrAgent id:{} localID:{}", SuperType::GetID(), SuperType::GetLocalID());
        }

        uint64_t GetType() const override { return _regionType; }

        bool QueueEqual(const MsgQueueBaseInfo& baseInfo)
        {
                return _regionType == baseInfo.region_type()
                        && _queueType == baseInfo.queue_type()
                        && static_cast<int64_t>(SuperType::GetID()) == baseInfo.queue_guid()
                        && _time == baseInfo.time();
        }

        const ERegionType _regionType = E_RT_None;
        EQueueType _queueType = E_QT_None;
        time_t _time = 0;
        int64_t _param = 0;
        int64_t _param_1 = 0;
        google::protobuf::RepeatedPtrField<MsgPlayerQueueInfo> _playerList;
        std::function<void(uint16_t, const ActorMailDataPtr& msg)> _cb;

        void PackQueueBaseInfo(auto& msg)
        {
                msg.set_queue_guid(SuperType::GetID());
                msg.set_queue_type(_queueType);
                msg.set_region_type(_regionType);
                msg.set_time(_time);
        }

        void PackQueueInfo(auto& msg)
        {
                PackQueueBaseInfo(*msg.mutable_base_info());
                msg.set_param(_param);
                msg.mutable_player_list()->CopyFrom(_playerList);
        }
};

class LobbyGameMgrSession : public ActorAgentSession<TcpSession, GameMgrAgent, LobbyGameMgrSession, false>
{
public :
	typedef ActorAgentSession<TcpSession, GameMgrAgent, LobbyGameMgrSession, false> SuperType;
public :
        LobbyGameMgrSession(typename SuperType::SocketType&& s)
                : SuperType(std::move(s))
        {
        }

        ~LobbyGameMgrSession() override;

public :
	void OnConnect() override;
	void OnClose(int32_t reasonType) override;

        static void MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef);
        FORCE_INLINE void OnRecvCallRet(ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
        {
                auto msgHead = *reinterpret_cast<MsgHeaderType*>(buf);
                auto agent = GetAgent(msgHead._from, msgHead._to);
                if (agent)
                {
                        switch (msgHead._type)
                        {
                        case MsgHeaderType::MsgTypeMerge<E_MCMT_QueueCommon, E_MCQCST_ReqQueue>() :
                                {
                                        auto msg = std::make_shared<MailReqQueue>();
                                        Compress::UnCompressAndParseAlloc(static_cast<Compress::ECompressType>(msgHead._ct), *msg, buf+sizeof(MsgHeaderType), msgHead._size-sizeof(MsgHeaderType));
                                        auto gameMgrAgent = std::dynamic_pointer_cast<ActorAgentType>(agent);
                                        if (gameMgrAgent->_cb)
                                                gameMgrAgent->_cb(msgHead._type, msg);
                                }
                                break;
                        default :
                                break;
                        }
                        agent->RemoteCallRet(buf, bufRef);
                }
                else
                {
                        LOG_WARN("call ret 没找到agent!!! mt[{:#x}] st[{:#x}] to[{}] from[{}]",
                                 MsgHeaderType::MsgMainType(msgHead._type),
                                 MsgHeaderType::MsgSubType(msgHead._type),
                                 msgHead._to, msgHead._from);
                }
        }

public :
        static const std::string _sPriorityTaskKey;
	EXTERN_MSG_HANDLE();
        DECLARE_SHARED_FROM_THIS(LobbyGameMgrSession);
};

}; // end of namespace nl::af::impl

// vim: fenc=utf8:expandtab:ts=8
