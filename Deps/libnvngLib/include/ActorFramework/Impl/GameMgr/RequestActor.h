#pragma once

#include "RegionMgr.h"
#include "GameMgrGameSession.h"

class RequestActor : public ActorImpl<RequestActor, RegionMgr>
{
        typedef ActorImpl<RequestActor, RegionMgr> SuperType;
public :
        RequestActor(uint64_t id);
        ~RequestActor() override;

        void ReqQueue(const stQueueInfoPtr& msg);
        void RegionDestroy(const GameMgrGameSession::ActorAgentTypePtr& regionAgent, const std::shared_ptr<MailRegionDestroyInfo>& msg);
        void DelRegionFromRegionMgr(const GameMgrGameSession::ActorAgentTypePtr& regionAgent, const std::shared_ptr<MailRegionDestroyInfo>& msg);
        EInternalErrorType ExitQueue(const IActorPtr& from, const std::shared_ptr<MsgExitQueue>& msg);

protected :
        friend class ActorImpl<RequestActor, RegionMgr>;

	EXTERN_ACTOR_MAIL_HANDLE();
        DECLARE_SHARED_FROM_THIS(RequestActor);
};
typedef std::shared_ptr<RequestActor> RequestActorPtr;

// vim: fenc=utf8:expandtab:ts=8
