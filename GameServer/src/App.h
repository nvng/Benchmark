#pragma once

#include <atomic>
class App : public AppBase, public Singleton<App>
{
        typedef AppBase SuperType;

        App(const std::string& appName);
        ~App() override;
        friend class Singleton<App>;

public :
        bool Init() override;

public :
        std::atomic_int64_t _regionCreateCnt;
        std::atomic_int64_t _regionDestroyCnt;
        std::atomic_int64_t _cnt = 0;
};

extern App* GetApp();

// vim: fenc=utf8:expandtab:ts=8
