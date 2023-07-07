#pragma once

#include "RegionImpl.hpp"
#include "Fighter.h"
#include "RegionState.h"
#include "msg_jump.pb.h"

namespace Jump {

class Region : public RegionImpl<Region, Fighter, RegionCfg, MsgRegionInfo, MsgFighterEnter, MsgRegionStateInfo>
{
        typedef RegionImpl<Region, Fighter, RegionCfg, MsgRegionInfo, MsgFighterEnter, MsgRegionStateInfo> SuperType;
public :
        typedef Fighter FighterType;
        typedef std::shared_ptr<FighterType> FighterPtr;

public :
        Region(const std::shared_ptr<MailRegionCreateInfo>& cInfo,
               const std::shared_ptr<RegionCfg>& cfg,
               const GameMgrSession::ActorAgentTypePtr& agent);
        ~Region() override;

public :
        static bool InitOnce();

public :
        bool Init() override;

	FighterPtr CreateFighter(const GameLobbySession::ActorAgentTypePtr& p,
                                 const GameGateSession::ActorAgentTypePtr& cli,
                                 const MailReqEnterRegion::MsgReqFighterInfo& info) override;
        bool CheckStart() override;
        void OnFighterDisconnect(const FighterPtr& f) override
        {
                SuperType::OnFighterDisconnect(f);
                UnRegisterFighter(f->GetID());
        }

        FORCE_INLINE FighterPtr GetFighter(int64_t id)
        { return std::dynamic_pointer_cast<FighterType>(SuperType::GetFighter(id)); }

public :
        void Prepare();
        void Play();
        void PlayEnd();
        void Conclude();

public :
        FORCE_INLINE void ChangeState(int stateType, StateEventInfo& evt)
	{ _regionStateMgr.ChangeState(stateType, shared_from_this(), evt); }
        void OnEvent(int64_t eventType) override
        { StateEventInfo evt(eventType); OnEvent(evt); }
        FORCE_INLINE void OnEvent(StateEventInfo& evt)
	{ _regionStateMgr.OnEvent(shared_from_this(), evt); }
private :
        RegionStateMgr _regionStateMgr;

        DECLARE_SHARED_FROM_THIS(Region);
	EXTERN_ACTOR_MAIL_HANDLE();
};

}; // end namespace Jump

// vim: fenc=utf8:expandtab:ts=8
