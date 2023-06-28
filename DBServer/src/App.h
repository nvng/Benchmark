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
  std::atomic_int64_t _loadVersionCnt;
  std::atomic_int64_t _loadVersionSize;
  std::atomic_int64_t _loadCnt;
  std::atomic_int64_t _loadSize;
  std::atomic_int64_t _saveCnt;
  std::atomic_int64_t _saveSize;
};

extern App* GetApp();

// vim: fenc=utf8:expandtab:ts=8
