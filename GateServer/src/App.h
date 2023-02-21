#pragma once

class App : public AppBase, public Singleton<App>
{
  typedef AppBase SuperType;

  App();
  ~App() override;
  friend class Singleton<App>;

public :
  bool Init(const std::string& appName);
  void Stop() override;

public :
  nl::af::impl::stGateServerInfoPtr _gateInfo;
  nl::af::impl::stGateServerCfgPtr _gateCfg;
};

extern App* GetApp();

// vim: fenc=utf8:expandtab:ts=8
