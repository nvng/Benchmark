#pragma once

#include "ActorFramework/IActor.h"

namespace nl::af
{
class IActor;
};

namespace nl::util
{

template <typename Flag>
class TimerMgrBase : public Singleton<TimerMgrBase<Flag>>
{
        TimerMgrBase() = default;
        ~TimerMgrBase() = default;
        friend class Singleton<TimerMgrBase<Flag>>;

public :
        bool Init()
        {
                std::thread t([this]() {
                        SetThreadName("timer");
                        auto w = boost::asio::make_work_guard(_ioCtx);
                        boost::system::error_code ec;
                        _ioCtx.run(ec);
                        LOG_WARN_IF(boost::system::errc::success != ec.value(),
                                    "结束 TimerMgrBase io_context 错误!!! ec[{}]", ec.what());
                });
                _thread.swap(t);
                _thread.detach();

                return true;
        }

        void Terminate()
        {
                _ioCtx.stop();
        }

        void WaitForTerminate()
        {
                if (_thread.joinable())
                        _thread.joinable();
        }

public :
        boost::asio::io_context _ioCtx;
private :
        std::thread _thread;
};

typedef TimerMgrBase<stDefaultFlag> TimerMgr;

template <typename Flag>
class SteadyTimerBase
{
public :
        SteadyTimerBase()
                : SteadyTimerBase(TimerMgrBase<Flag>::GetInstance()->_ioCtx)
        {
        }

        explicit SteadyTimerBase(auto& ctx)
                : _timer(ctx)
        {
        }

        ~SteadyTimerBase()
        {
                Stop();
        }

        void Start(const std::weak_ptr<af::IActor>& weakAct, double interval, const auto& cb)
        {
                _timer.expires_from_now(std::chrono::milliseconds((time_t)(interval * 1000)));
                // _timer.expires_after(duration);
                // _timer.expires_at(std::chrono::steady_clock::now() + duration);
                _timer.async_wait([weakAct, cb{std::move(cb)}](const auto& ec) {
                        if (boost::system::errc::success == ec.value())
                        {
                                auto act = weakAct.lock();
                                if (act)
                                        act->SendPush(std::move(cb));
                        }
                });
        }

        void Start(double interval, const auto& cb)
        {
                _timer.expires_from_now(std::chrono::milliseconds((time_t)(interval * 1000)));
                _timer.async_wait([cb{ std::move(cb) }](const auto& ec) {
                        if (boost::system::errc::success == ec.value())
                                cb();
                });
        }

        static void StaticStart(const auto& duration, const auto& cb)
        {
                auto timer = std::make_shared<boost::asio::steady_timer>(TimerMgrBase<Flag>::GetInstance()->_ioCtx);
                timer->expires_after(duration);
                timer->async_wait([cb{ std::move(cb) }, timer](const auto& ec) {
                        if (boost::system::errc::success == ec.value())
                                cb();
                });
        }

        void Stop()
        {
                // Note: 会立即执行 handler。

                boost::system::error_code ec;
                _timer.cancel(ec);
                LOG_WARN_IF(boost::system::errc::success != ec.value(),
                            "退出 timer 错误!!! ec[{}]", ec.what());
        }

        FORCE_INLINE void StartForever(double interval, const auto& cb)
        { StartForeverInternal(interval, interval, std::move(cb)); }

        FORCE_INLINE void StartForever(double startTime, double interval, const auto& cb)
        { StartForeverInternal(startTime, interval, std::move(cb)); }

private :
        void StartForeverInternal(double startTime, double interval, const auto& cb)
        {
                _timer.expires_from_now(std::chrono::milliseconds((time_t)(startTime * 1000)));
                _timer.async_wait([this, interval, cb{ std::move(cb) }](const auto& ec) {
                        if (boost::system::errc::success == ec.value())
                        {
                                if (cb())
                                        StartForeverInternal(interval, interval, std::move(cb));
                        }
                });
        }

private :
        boost::asio::steady_timer _timer;
};

typedef SteadyTimerBase<stDefaultFlag> SteadyTimer;

template <typename Flag>
class SystemTimerBase
{
public :
        SystemTimerBase()
                : SystemTimerBase(TimerMgrBase<Flag>::GetInstance()->_ioCtx)
        {
        }

        explicit SystemTimerBase(auto& ctx)
                : _timer(ctx)
        {
        }

        ~SystemTimerBase()
        {
                Stop();
        }

        void StartAt(const std::weak_ptr<af::IActor>& weakAct, double t, const auto& cb)
        {
                auto timePoint = std::chrono::system_clock::from_time_t(0) + std::chrono::milliseconds((time_t)(t * 1000));
                _timer.expires_at(timePoint);
                _timer.async_wait([weakAct, cb{std::move(cb)}](const auto& ec) {
                        if (boost::system::errc::success == ec.value())
                        {
                                auto act = weakAct.lock();
                                if (act)
                                        act->SendPush(std::move(cb));
                        }
                });
        }

        void StartAt(double t, const auto& cb)
        {
                auto timePoint = std::chrono::system_clock::from_time_t(0) + std::chrono::milliseconds((time_t)(t * 1000));
                _timer.expires_at(timePoint);
                _timer.async_wait([cb{ std::move(cb) }](const auto& ec) {
                        if (boost::system::errc::success == ec.value())
                                cb();
                });
        }

        void Stop()
        {
                // Note: 会立即执行 handler。

                boost::system::error_code ec;
                _timer.cancel(ec);
                LOG_WARN_IF(boost::system::errc::success != ec.value(),
                            "退出 timer 错误!!! ec[{}]", ec.what());
        }

private :
        boost::asio::system_timer _timer;
};

typedef SystemTimerBase<stDefaultFlag> SystemTimer;

}; // end of namespace nl::util

// vim: fenc=utf8:expandtab:ts=8:noma
