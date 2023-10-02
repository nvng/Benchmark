#include "RegionState.h"

#include "Region.h"

namespace Jump {

FSMBase<const RegionPtr, StateEventInfo>::StateBaseType*
RegionStateMgr::CreateStateByType(int stateType)
{
	switch (stateType)
	{
	case E_RST_None: return new RegionNoneState(); break;
	case E_RST_Prepare: return new RegionPrepareState(); break;
	case E_RST_Play: return new RegionPlayState(); break;
	case E_RST_Conclude: return new RegionConcludeState(); break;
        case E_RST_Timeout: return nullptr; break;
	default: DLOG_FATAL("Region 状态没有注册 stateType[{}]", stateType); break;
	}

	return nullptr;
}

void RegionStateBase::Enter(const RegionPtr& region, StateEventInfo& evt)
{
        auto interval = region->GetStateInterval(GetStateType()) / 1000.0;
        REGION_DLOG_INFO(region->GetID(), "state[{}] interval[{}]", GetStateType(), interval);
        std::weak_ptr<Region> weakRegion = region;
        // Note: 因为 state 与 region 一一绑定，因此只需要检测 region 就可以了。
        _timer.Start(weakRegion, interval, [weakRegion]() {
                auto region = weakRegion.lock();
                if (!region)
                        return;

                region->OnEvent(E_RSECT_OverTime);
        });

        _curStateEndTime = GetClock().GetTime() + interval;
        region->UpdateRegionState(GetStateType(), _curStateEndTime);
}

void RegionStateBase::Exit(const RegionPtr& region, StateEventInfo& evt)
{
        _timer.Stop();
}

// {{{ RegionNoneState
void RegionNoneState::Enter(const RegionPtr& region, StateEventInfo& evt)
{
        // REGION_DLOG_INFO(region->GetID(), "region[{}] 进入 None 状态!!! now[{}]", region->GetID(), GetClock().GetTimeStamp());
        RegionStateBase::Enter(region, evt);
}

void RegionNoneState::Exit(const RegionPtr& region, StateEventInfo& evt)
{
        RegionStateBase::Exit(region, evt);
}

void RegionNoneState::OnEvent(const RegionPtr& region, StateEventInfo& evt)
{
        switch (evt._eventType)
        {
        case E_RSECT_OnFighterExit :
                if (region->GetAllFighterCnt() <= 0)
                        region->Destroy();
                break;
        case E_RSECT_OnFighterEnter :
                {
                        if (region->CheckStart())
                        {
                                region->ChangeState(E_RST_Prepare, evt);
                        }
                }
                break;
        case E_RSECT_OverTime : region->Destroy(); break;
        default : break;
        }
}
// }}}

// {{{ RegionPrepareState
void RegionPrepareState::Enter(const RegionPtr& region, StateEventInfo& evt)
{
        // REGION_DLOG_INFO(region->GetID(), "region[{}] 进入 Prepare 状态!!! now[{}]", region->GetID(), GetClock().GetTimeStamp());
        RegionStateBase::Enter(region, evt);
        region->Prepare();
}

void RegionPrepareState::Exit(const RegionPtr& region, StateEventInfo& evt)
{
        RegionStateBase::Exit(region, evt);
}

void RegionPrepareState::OnEvent(const RegionPtr& region, StateEventInfo& evt)
{
        switch (evt._eventType)
        {
        case E_RSECT_OnFighterExit :
                if (region->GetAllFighterCnt() <= 0)
                        region->Destroy();
                break;
        case E_RSECT_OverTime :
                region->Play();
                region->ChangeState(E_RST_Play, evt);
                break;
        default : break;
        }
}
// }}}

// {{{ RegionPlayState
void RegionPlayState::Enter(const RegionPtr& region, StateEventInfo& evt)
{
        REGION_DLOG_INFO(region->GetID(), "region[{}] 进入 Play 状态!!! now[{}]", region->GetID(), GetClock().GetTimeStamp());
        RegionStateBase::Enter(region, evt);
}

void RegionPlayState::Exit(const RegionPtr& region, StateEventInfo& evt)
{
        RegionStateBase::Exit(region, evt);
}

void RegionPlayState::OnEvent(const RegionPtr& region, StateEventInfo& evt)
{
        switch (evt._eventType)
        {
        case E_RSECT_OnFighterExit :
                if (region->GetAllFighterCnt() <= 0)
                        region->Destroy();
                break;
        case E_RSET_PlayerPlayEnd :
                _timer.Stop();
                break;
        case E_RSET_PlayEnd :
        case E_RSECT_OverTime :
        case E_RSET_ForceEndState :
                region->ChangeState(E_RST_Conclude, evt);
                break;

        default : break;
        }
}
// }}}

// {{{ RegionConcludeState
void RegionConcludeState::Enter(const RegionPtr& region, StateEventInfo& evt)
{
        REGION_DLOG_INFO(region->GetID(), "region[{}] 进入 Conclude 状态!!! now[{}]", region->GetID(), GetClock().GetTimeStamp());
        region->PlayEnd();
        RegionStateBase::Enter(region, evt);
        region->Conclude();
}

void RegionConcludeState::Exit(const RegionPtr& region, StateEventInfo& evt)
{
        RegionStateBase::Exit(region, evt);
}

void RegionConcludeState::OnEvent(const RegionPtr& region, StateEventInfo& evt)
{
        switch (evt._eventType)
        {
        case E_RSECT_OnFighterExit :
                if (region->GetAllFighterCnt() <= 0)
                        region->Destroy();
                break;
        case E_RSECT_OverTime : 
                region->Destroy();
                break;
        default : break;
        }
}
// }}}

}; // end namespace Jump

// vim: fenc=utf8:expandtab:ts=8
