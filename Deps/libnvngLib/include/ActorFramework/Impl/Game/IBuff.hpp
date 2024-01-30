#pragma once

class IBuff
{
};
typedef boost::shared_ptr<IBuff> IBuffPtr;
typedef boost::weak_ptr<IBuff> IBuffWeakPtr;

template <typename _Iy, typename _Oy, typename _Cy, typename _Ey, typename _Py>
class BuffImpl : public IBuff, public boost::enable_shared_from_this<BuffImpl<_Iy, _Oy, _Cy, _Ey, _Py>>
{
        typedef IBuff SuperType;

public :
        typedef _Iy ImplType;
        typedef _Oy OwnerType;
        typedef _Cy CfgType;
        typedef _Ey EventInfoType;
        typedef _Py MessageType;
        typedef boost::shared_ptr<OwnerType> OwnerTypePtr;
        typedef BuffImpl<ImplType, OwnerType, CfgType, EventInfoType, MessageType> ThisType;
public :
        BuffImpl(int64_t id,
                 const boost::shared_ptr<OwnerType>& orig,
                 const boost::shared_ptr<OwnerType>& src,
                 const boost::shared_ptr<OwnerType>& owner,
                 const std::shared_ptr<CfgType>& cfg)
                : _id(id)
                  , _orig(orig)
                  , _src(src)
                  , _owner(owner)
                  , _cfg(cfg)
        {
                if (orig)
                        _origID = orig->GetID();
        }

        virtual ~BuffImpl()
        {
        }

        virtual void Pack(const EventInfoType& evt, MessageType& msg)
        {
                msg.set_id(GetID());
                msg.set_cfg_id(_cfg->_id);

                auto owner = GetOwner();
                msg.set_owner(owner->GetID());
                msg.set_orig(_origID);
        }

        static void ForceAddBuff(const EventInfoType& evt,
                                 const OwnerTypePtr& s,
                                 int64_t id,
                                 int64_t cfgID,
                                 const OwnerTypePtr& orig,
                                 const OwnerTypePtr& src)
        {
                auto f = s->GetFighter();
                auto region = f->GetRegion();
                if (s->GetBuff(id))
                        return;

                s->AddBuffByID(evt, id, cfgID, nullptr, nullptr);

                auto item = region->_msgEffectiveInfo.add_item_list();
                item->set_guid(region->GetEffectiveGuid());
                auto info = item->mutable_force_add_or_remove_buff_effective_info();
                info->set_id(id);
                info->set_cfg_id(cfgID);
                info->set_owner(s->GetID());
        }

        static void ForceRemoveBuff(const EventInfoType& evt, const OwnerTypePtr& s, int64_t id)
        {
                auto f = s->GetFighter();
                auto region = f->GetRegion();
                if (!s->GetBuff(id))
                        return;

                auto item = region->_msgEffectiveInfo.add_item_list();
                item->set_guid(region->GetEffectiveGuid());
                auto info = item->mutable_force_add_or_remove_buff_effective_info();
                info->set_id(id);
                info->set_owner(s->GetID());

                s->RemoveBuff(evt, id);
        }

        virtual void OnEnter(const EventInfoType& evt) { }
        virtual void Run(const EventInfoType& evt) { }
        virtual void AfterRun(const EventInfoType& evt) { }
        virtual void OnExit(const EventInfoType& evt) { _isRemoved = true; }

public :
        FORCE_INLINE int64_t GetID() const { return _id; }
        FORCE_INLINE int64_t GetCfgID() const { return _cfg->_id; }
        FORCE_INLINE std::shared_ptr<CfgType> GetCfg() const { return _cfg; }
        FORCE_INLINE OwnerTypePtr GetOwner() { return _owner.lock(); }
        FORCE_INLINE OwnerTypePtr GetSrc() { return _src.lock(); }
        FORCE_INLINE OwnerTypePtr GetOrig() { return _orig.lock(); }
private :
        int64_t _id = -1;
        int64_t _origID = -1;
        const boost::weak_ptr<OwnerType> _orig;
        const boost::weak_ptr<OwnerType> _src;
        boost::weak_ptr<OwnerType> _owner;
        const std::shared_ptr<CfgType> _cfg;

public :
        bool CheckAndMarkRemove(const EventInfoType& evt)
        {
                auto owner = GetOwner();
                auto cfg = GetCfg();
                if (FLAG_HAS(cfg->_betParamRemoveTriggerPointListBit, 1<<evt._eventType))
                {
                        ForceRemoveBuff(evt, owner, GetID());
                        return true;
                }
                return false;
        }
        bool CheckInterval(const EventInfoType& evt);
        inline bool IsRemoved() const { return _isRemoved; }
protected :
        int32_t _curTimes = 0;
private :
        bool _isRemoved = false;
};
typedef boost::shared_ptr<IBuff> IBuffPtr;
typedef boost::weak_ptr<IBuff> IBuffWeakPtr;

// vim: fenc=utf8:expandtab:ts=8:sw=8
