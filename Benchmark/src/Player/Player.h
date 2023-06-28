#pragma once

#include "PlayerState.h"
#include "Net/ClientGateSession.h"
#include "PlayerMgr.h"

class Player : public ActorImpl<Player, PlayerMgr>
{
        typedef ActorImpl<Player, PlayerMgr> SuperType;
public :
        Player(uint64_t guid);
        ~Player() override;

        bool Init() override;

        static uint64_t GenPlayerGuid();
        void UnPack(const MsgPlayerInfo& msg);

public :
        void SendPB(uint16_t mainType, uint16_t subType, google::protobuf::MessageLite* pb=nullptr);
        bool UseGMGoods();
        void DealPlayerChange(const MsgPlayerChange& msg);
        void OnDisconnect();

public :
        std::shared_ptr<ClientGateSession> _ses;
        MsgPlayerInfo _msgPlayerInfo;
        std::unordered_map<int64_t, int64_t> _goodsList;

public :
        FORCE_INLINE void ChangeState(int stateType, StateEventInfo& evt) { _stateMgr.ChangeState(stateType, shared_from_this(), evt); }
        FORCE_INLINE void OnEvent(int64_t mt, int64_t st, const MessageLitePtr& msg)
        { StateEventInfo evt(ActorMail::MsgTypeMerge(mt, st)); evt._data = msg; OnEvent(evt); }
        FORCE_INLINE void OnEvent(StateEventInfo& evt) { _stateMgr.OnEvent(shared_from_this(), evt); }
private :
        PlayerStateMgr _stateMgr;

private :
        friend class ActorImpl<Player, PlayerMgr>;
        DECLARE_SHARED_FROM_THIS(Player);
        EXTERN_ACTOR_MAIL_HANDLE();
};

typedef std::shared_ptr<Player> PlayerPtr;
typedef std::weak_ptr<Player> PlayerWeakPtr;

// vim: fenc=utf8:expandtab:ts=8
