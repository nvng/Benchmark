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
  std::atomic_uint64_t _cnt;
  std::atomic_uint64_t _reqQueueCnt;
};

extern App* GetApp();

// vim: fenc=utf8:expandtab:ts=8
