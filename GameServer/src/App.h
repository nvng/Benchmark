#pragma once

class App : public AppBase, public Singleton<App>
        {
                typedef AppBase SuperType;

                App(const std::string& appName);
                ~App() override;
                friend class Singleton<App>;

        public :
                bool Init() override;
        };

extern App* GetApp();

// vim: fenc=utf8:expandtab:ts=8
