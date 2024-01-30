#pragma once

template <typename _Ty>
class Singleton
{
public:
        static __attribute__((always_inline)) _Ty* GetInstance() { return _instance; }

        template <typename ... Args>
        static void CreateInstance(const Args&... args)
        {
                static bool _ = [args...]()
                {
                        _instance = new _Ty(std::forward<const Args>(args)...);
                        return true;
                }();
                (void)_;
        }

        static void DestroyInstance()
        {
                static bool _ = []()
                {
                        delete _instance;
                        _instance = nullptr;
                        return true;
                }();
                (void)_;
        }

private:
        static _Ty* _instance;

protected:
        Singleton() = default;
        Singleton(const Singleton&) = delete;
        Singleton(const Singleton&&) = delete;
        Singleton& operator=(const Singleton&) = delete;
        Singleton& operator=(const Singleton&&) = delete;
};

template <typename _Ty>
_Ty* Singleton<_Ty>::_instance = nullptr;

// vim: fenc=utf8:expandtab:ts=8:noma
