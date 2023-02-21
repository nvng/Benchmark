#pragma once

class LobbyGateSession;
class App : public AppBase, public Singleton<App>
{
  typedef AppBase SuperType;

  App();
  ~App() override;
  friend class Singleton<App>;

public :
  bool Init(const std::string& appName, int32_t coCnt = 100, int32_t thCnt = 1);
  void Stop() override;

public :
  void OnDayChange();
  void OnDataReset();

private :
  TimerGuidType _dayChangeTimerGuid = INVALID_TIMER_GUID;
  TimerGuidType _dataResetTimerGuid = INVALID_TIMER_GUID;

private :
  void InitPreTask();
  void StopPreTask();

public :
  // std::shared_ptr<ServiceDistcoveryActor> _serviceDiscoveryActor;
  std::shared_ptr<nl::af::impl::GlobalVarActor> _globalVarActor;

public :
  std::atomic_int64_t _cnt;
  nl::af::impl::stLobbyServerInfoPtr _lobbyInfo;

public :
  ThreadSafeUnorderedMap<int64_t, std::weak_ptr<LobbyGateSession>> _gateSesList;
};

extern App* GetApp();

// vim: fenc=utf8:expandtab:ts=8
