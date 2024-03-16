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
  stGateServerCfgPtr _gateCfg;
  std::atomic_int64_t _testCnt = 0;
  std::atomic_int64_t _clientRecvCnt = 0;
  std::atomic_int64_t _serverRecvCnt = 0;
  std::atomic_int64_t _gameRecvCnt = 0;
  std::atomic_int64_t _loginRecvCnt = 0;
};

extern App* GetApp();

// vim: fenc=utf8:expandtab:ts=8
