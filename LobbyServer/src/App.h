#pragma once

class App : public AppBase, public Singleton<App>
{
  typedef AppBase SuperType;

  App(const std::string& appName);
  ~App() override;
  friend class Singleton<App>;

public :
  bool Init() override;

public :
  void OnDayChange();
  void OnDataReset();

public :
  // std::shared_ptr<ServiceDistcoveryActor> _serviceDiscoveryActor;
  std::shared_ptr<GlobalVarActor> _globalVarActor;

public :
  std::atomic_int64_t _cnt;
  std::atomic_int64_t _cnt_1;
  std::atomic_int64_t _mainCityCnt;

public :
  ThreadSafeUnorderedMap<int64_t, std::weak_ptr<LobbyGateSession>> _gateSesList;
  UnorderedSet<uint64_t> _testList;
  uint64_t _i = 0;
};

extern App* GetApp();

// vim: fenc=utf8:expandtab:ts=8
