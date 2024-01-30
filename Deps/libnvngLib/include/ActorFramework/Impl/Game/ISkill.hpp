#pragma once

#define PREPARE_SOME_VAR() \
       auto cfg = GetCfg(); \
       auto owner = boost::dynamic_pointer_cast<Soldier>(GetOwner()); if (!owner) return; \
       auto f = owner->GetFighter(); if (!f) return; \
       auto region = f->GetRegion(); if (!region) return

#define PREPARE_SOME_VAR_RETURN_BOOL() \
  auto cfg = GetCfg(); \
  auto owner = boost::dynamic_pointer_cast<Soldier>(GetOwner()); if (!owner) return false; \
  auto f = owner->GetFighter(); if (!f) return false; \
  auto region = f->GetRegion(); if (!region) return false

class ISkill
{
public :
};

typedef boost::shared_ptr<ISkill> ISkillPtr;
typedef boost::weak_ptr<ISkill> ISkillWeakPtr;

template <typename _Iy, typename _Oy, typename _Cy, typename _Ey, typename _Py>
class SkillImpl : public ISkill, public boost::enable_shared_from_this<SkillImpl<_Iy, _Oy, _Cy, _Ey, _Py>>
{
  typedef ISkill SuperType;

public :
  typedef _Iy ImplType;
  typedef _Oy OwnerType;
  typedef _Cy CfgType;
  typedef _Ey EventInfoType;
  typedef _Py MessageType;
  typedef SkillImpl<ImplType, OwnerType, CfgType, EventInfoType, MessageType> ThisType;

public :
  SkillImpl(int64_t id,
            const boost::shared_ptr<OwnerType>& owner,
            const std::shared_ptr<CfgType>& cfg)
    : _id(id)
      , _owner(owner)
      , _cfg(cfg)
  {
    _subSkillList.reserve(4);
  }

  virtual ~SkillImpl()
  {
  }

  virtual bool Init(std::set<int64_t>& inheritTargetList)
  {
    return true;
  }

  virtual void Pack(const EventInfoType& evt, MessageType& msg)
  {
    msg.set_id(GetID());
    msg.set_cfg_id(_cfg->_id);
    msg.set_trigger_cnt(_triggerCnt);

    auto owner = GetOwner();
    msg.set_owner(owner->GetID());
  }

  virtual bool CheckSkillCost(const EventInfoType& evt) { return true; }
  virtual void DelSkillCost(const EventInfoType& evt) { }
  virtual void OnEnter() { }

  virtual bool Run(const EventInfoType& evt) { return true; }
  virtual void AfterRun(const EventInfoType& evt)
  {
    auto owner = GetOwner();
    if (!owner || owner->IsDie())
      return;

    for (auto& skill : _subSkillList)
    {
      skill->Run(evt);
    }
  }

  virtual void AfterRun(const EventInfoType& evt, std::vector<std::set<boost::shared_ptr<OwnerType>>>& targetList)
  {
    if (!targetList.empty())
    {
      auto owner = GetOwner();
      if (!owner)
        return;

      if (_saveTarget)
      {
        std::vector<std::vector<boost::weak_ptr<OwnerType>>> tmpList;
        tmpList.reserve(2);
        std::vector<boost::weak_ptr<OwnerType>> tv;
        for (auto& tl : targetList)
        {
          tv.reserve(tl.size());
          for (auto& s : tl)
          {
            if (!s->IsDie())
              tv.emplace_back(s);
          }
          tmpList.emplace_back(std::move(tv));
        }
        bool ret = owner->_skillInheritTargetList.emplace(GetCfgID(), std::move(tmpList)).second;
        LOG_ERROR_IF(!ret, "处理子技能时，可能出现了循环情况!!! skill cfg id:{}", GetCfgID());
      }

      for (auto& skill : _subSkillList)
      {
        skill->Run(evt);
      }

      if (_saveTarget)
      {
        owner->_skillInheritTargetList.erase(GetCfgID());
      }
    }
  }

public :
  inline int64_t GetID() const { return _id; }
  inline int64_t GetCfgID() const { return _cfg->_id; }
  inline ESkillTriggerPointType GetSTPT() const { return _cfg->_stpt; }
  inline std::shared_ptr<CfgType> GetCfg() const { return _cfg; }
  inline boost::shared_ptr<OwnerType> GetOwner() { return _owner.lock(); }
private :
  int64_t _id = -1;
  boost::weak_ptr<OwnerType> _owner;
  std::shared_ptr<CfgType> _cfg;
protected :
  std::vector<boost::shared_ptr<ThisType>> _subSkillList;
  int64_t _runTimes = 0;
  int64_t _triggerCnt = 0;
public :
  bool _saveTarget = false;
};

// vim: fenc=utf8:expandtab:ts=8
