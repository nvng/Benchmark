#pragma once

template<typename _Ty>
class FighterAgent : public ActorAgent<_Ty>
{
        typedef _Ty SessionType;
        typedef ActorAgent<_Ty> SuperType;
public :
        FighterAgent(uint32_t id, const std::shared_ptr<SessionType>& ses)
                : SuperType(id, ses)
        {
                // DLOG_INFO("FighterAgent::FighterAgent id:{}", SuperType::GetID());
        }

        ~FighterAgent() override
        {
                // DLOG_INFO("FighterAgent::~FighterAgent id:{}", SuperType::GetID());
        }

        bool CallPushInternal(const IActorPtr& from,
                              uint64_t mainType,
                              uint64_t subType,
                              const ActorMailDataPtr& msg,
                              uint16_t guid) override
        {
                assert(false);
                return true;
        }
};

// vim: fenc=utf8:expandtab:ts=8
