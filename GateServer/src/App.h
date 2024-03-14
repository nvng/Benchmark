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
  std::atomic_int64_t _cnt;
  std::atomic_int64_t _clientRecvCnt;
  std::atomic_int64_t _serverRecvCnt;
  std::atomic_int64_t _gameRecvCnt;
  std::atomic_int64_t _loginRecvCnt;
};

extern App* GetApp();

// vim: fenc=utf8:expandtab:ts=8
