#pragma once

class App : public AppBase, public Singleton<App>
{
  typedef AppBase SuperType;

  App(const std::string& appName);
  ~App() override;
  friend class Singleton<App>;

public :
  bool Init() override;
  void Stop() override;

public :
  std::atomic_uint64_t _cnt;
};

extern App* GetApp();

// vim: fenc=utf8:expandtab:ts=8
