#pragma once

#include "IService.h"

#include "Tools/ThreadSafeMap.hpp"

namespace nl::af
{

class ServiceImpl : public IService
{
public :
        ServiceImpl(const std::string& flag) : _actorList(fmt::format("{}_ServiceImpl_actorList", flag)) { }

        bool Init() override
        {
                return true;
        }

        FORCE_INLINE std::size_t GetActorCnt() { return _actorList.Size(); }
        FORCE_INLINE IActorPtr GetActor(uint64_t id) { return _actorList.Get(id).lock(); }

        bool AddActor(const IActorPtr& act) override
        {
                assert(act->GetService() == reinterpret_cast<IService*>(this));
                return act ? _actorList.Add(act->GetID(), act) : false;
        }

        void RemoveActor(const IActorPtr& act) override
        { if (act) _actorList.Remove(act->GetID(), act.get()); }

        template <typename _Fy>
        FORCE_INLINE void Foreach(const _Fy& cb)
        { _actorList.Foreach(cb); }

        FORCE_INLINE void BroadCast(const IActorPtr& from,
                                    uint64_t mt,
                                    uint64_t st,
                                    const ActorMailDataPtr& msg)
        {
                auto m = std::make_shared<ActorMail>(from, msg, ActorMail::MsgTypeMerge(mt, st));
                Foreach([m](const auto& wp) {
                        auto p = wp.lock();
                        if (p)
                                p->Push(m);
                });
        }
        
        void Terminate() override
        {
                _actorList.Foreach([](const auto& weakAct) {
                        auto act = weakAct.lock();
                        if (act)
                                act->Terminate();
                });
        }

        void WaitForTerminate() override
        {
                _actorList.ForeachClear([](const auto& weakAct) {
                        auto act = weakAct.lock();
                        if (act)
                                act->WaitForTerminate();
                });
        }

private :
        ThreadSafeUnorderedMap<uint64_t, IActorWeakPtr> _actorList;
};

}; // end of namespace nl::af

// vim: fenc=utf8:expandtab:ts=8:noma
