#pragma once

#include "Define.hpp"
#include "RegionImpl.hpp"

#include "msg_jump.pb.h"

namespace Jump {

enum ERegionStateEventType
{
	E_RSET_PlayEnd = ERegionStateEventCommonType_ARRAYSIZE,
	E_RSET_PlayerPlayEnd,
        E_RSET_ForceEndState,
};

class Region;
typedef std::shared_ptr<Region> RegionPtr;

class RegionStateMgr : public FSMBase<const RegionPtr, StateEventInfo>
{
public:
	RegionStateMgr() : FSMBase(ERegionStateType_ARRAYSIZE) {}
	~RegionStateMgr() override {}

	StateBaseType* CreateStateByType(int stateType) override;
};

class RegionStateBase : public StateBase<const RegionPtr, StateEventInfo>
{
public :
	RegionStateBase(int32_t stateType)
		: StateBase(stateType)
	{
	}

	void StartStateTimer(const RegionPtr& region);
	void StopStateTimer();

        void Enter(const RegionPtr& region, StateEventInfo& evt) override;
        void Exit(const RegionPtr& region, StateEventInfo& evt) override;

protected :
        SteadyTimer _timer;
	double _curStateEndTime = 0.0;
};

class RegionNoneState : public RegionStateBase
{
public:
        RegionNoneState() : RegionStateBase(E_RST_None) { }
        void Enter(const RegionPtr& region, StateEventInfo& evt) override;
        void Exit(const RegionPtr& region, StateEventInfo& evt) override;
	void OnEvent(const RegionPtr& region, StateEventInfo& evt) override;
};

class RegionPrepareState : public RegionStateBase
{
public:
        RegionPrepareState() : RegionStateBase(E_RST_Prepare) { }
        void Enter(const RegionPtr& region, StateEventInfo& evt) override;
        void Exit(const RegionPtr& region, StateEventInfo& evt) override;
	void OnEvent(const RegionPtr& region, StateEventInfo& evt) override;
};

class RegionPlayState : public RegionStateBase
{
public:
	RegionPlayState() : RegionStateBase(E_RST_Play) { }
	void Enter(const RegionPtr& region, StateEventInfo& evt) override;
	void Exit(const RegionPtr& region, StateEventInfo& evt) override;
	void OnEvent(const RegionPtr& region, StateEventInfo& evt) override;
};

class RegionConcludeState : public RegionStateBase
{
public:
	RegionConcludeState() : RegionStateBase(E_RST_Conclude) { }
	void Enter(const RegionPtr& region, StateEventInfo& evt) override;
	void Exit(const RegionPtr& region, StateEventInfo& evt) override;
	void OnEvent(const RegionPtr& region, StateEventInfo& evt) override;
};

}; // end namespace Jump

// vim: fenc=utf8:expandtab:ts=8
