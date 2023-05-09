#pragma once

class App : public AppBase, public Singleton<App>
{
  typedef AppBase SuperType;

  App(const std::string& appName);
  ~App() override;
  friend class Singleton<App>;

public :
  bool Init(int32_t coCnt = 100, int32_t thCnt = 1);

public :
  void OnDayChange();
  void OnDataReset();

private :
  TimerGuidType _dayChangeTimerGuid = INVALID_TIMER_GUID;
  TimerGuidType _dataResetTimerGuid = INVALID_TIMER_GUID;

public :
  // std::shared_ptr<ServiceDistcoveryActor> _serviceDiscoveryActor;
  std::shared_ptr<GlobalVarActor> _globalVarActor;

public :
  std::atomic_int64_t _cnt;
  stLobbyServerInfoPtr _lobbyInfo;

public :
  ThreadSafeUnorderedMap<int64_t, std::weak_ptr<LobbyGateSession>> _gateSesList;
  UnorderedSet<uint64_t> _testList;
  uint64_t _i = 0;
};

extern App* GetApp();

// vim: fenc=utf8:expandtab:ts=8
