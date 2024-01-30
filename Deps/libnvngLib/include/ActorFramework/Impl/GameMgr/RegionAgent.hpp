#pragma once

template <typename _Ty>
class RegionAgent : public ActorAgent<_Ty>
{
        typedef _Ty SessionType;
        typedef ActorAgent<SessionType> SuperType;
public :
        RegionAgent(const std::shared_ptr<MailRegionCreateInfo>& cfg,
                    const std::shared_ptr<_Ty>& ses,
                    const IActorPtr& b)
                : SuperType(cfg->region_guid(), ses)
                  , _cfg(cfg)
        {
                SuperType::BindActor(b);
                DLOG_INFO("RegionAgent::RegionAgent id:{}", SuperType::GetID());
        }

        ~RegionAgent() override
        {
                DLOG_INFO("RegionAgent::~RegionAgent id:{}", SuperType::GetID());
        }

        bool CanEnter(const std::shared_ptr<MailReqEnterRegion>& req)
        {
                return _fighterList.size() <= 2;
        }

        uint64_t GetType() const override { return _cfg->region_type(); }

public :
        FORCE_INLINE bool AddFighter(const IActorPtr& f)
        { return _fighterList.emplace(f->GetID(), f).second; }
        FORCE_INLINE IActorPtr RemoveFighter(uint64_t fighterGuid)
        {
                IActorPtr ret;
                auto it = _fighterList.find(fighterGuid);
                if (_fighterList.end() != it)
                {
                        ret = it->second;
                        _fighterList.erase(it);
                }
                return ret;
        }

        FORCE_INLINE void ForeachFighter(const auto& cb)
        {
                for (auto& val : _fighterList)
                        cb(val.second);
        }

        std::shared_ptr<MailRegionCreateInfo> _cfg;
        IActorPtr _regionMgr;
        EQueueType _queueType = E_QT_None;
        int64_t _queueGuid = 0;
private :
        std::unordered_map<uint64_t, IActorPtr> _fighterList;
};
