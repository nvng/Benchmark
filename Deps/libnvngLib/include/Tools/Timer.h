#pragma once

#include <functional>
#include <stdint.h>
#include "Util.h"

#define INVALID_TIMER_GUID              0
#define INVALID_COMMON_TIMEOUT_IDX      UINT8_MAX

#ifndef TIME_RATIO
#define TIME_RATIO      1000
#endif

typedef uint64_t TimerGuidType;
typedef int64_t CommonTimerIdxType;

namespace nl::af
{
class IActor;
};

using namespace nl::af;

class TimedEventItem;
class TimedEventLoop final
{
public:
        typedef std::function<void(TimerGuidType)> EventAsyncCallbackType;
        typedef std::function<void(TimedEventItem&)> EventCallbackType;
        typedef std::function<double()> GetCurTimeFuncType;

        explicit TimedEventLoop(const GetCurTimeFuncType& func);
        ~TimedEventLoop();

        TimerGuidType AddItem(TimedEventItem* eventData);

        void Update();
        void DealTimedEventItem(time_t now, TimedEventItem* data);

        TimerGuidType StartWithAbsoluteTime(double absoluteTime,
                double interval,
                int64_t loopCnt,
                EventCallbackType&& cb);

        inline TimerGuidType StartWithAbsoluteTimeOnce(double absoluteTime, EventCallbackType&& cb)
        { return StartWithAbsoluteTime(absoluteTime, 0, 1, std::move(cb)); }

        inline TimerGuidType StartWithAbsoluteTimeForever(double absoluteTime, double interval, EventCallbackType&& cb)
        { return StartWithAbsoluteTime(absoluteTime, interval, -1, std::move(cb)); }

        TimerGuidType StartWithRelativeTime(double relativeTime,
                double interval,
                int64_t loopCnt,
                EventCallbackType&& cb);

        inline TimerGuidType StartWithRelativeTimeOnce(double relativeTime, EventCallbackType&& cb)
        { return StartWithRelativeTime(relativeTime, relativeTime, 1, std::move(cb)); }

        inline TimerGuidType StartWithRelativeTimeForever(double relativeTime, double interval, EventCallbackType&& cb)
        { return StartWithRelativeTime(relativeTime, interval, -1, std::move(cb)); }

        inline TimerGuidType StartWithRelativeTimeForever(double interval, EventCallbackType&& cb)
        { return StartWithRelativeTime(interval, interval, -1, std::move(cb)); }

        void Stop(TimerGuidType idx, bool isExcute = false);
        void Pause(TimerGuidType idx);
        void Resume(TimerGuidType idx);
        void Restart(TimerGuidType idx);

        double GetAbsoluteTime(double relativeTime) const;

public:
        TimerGuidType AddCommonItem(TimedEventItem* ted);
        CommonTimerIdxType InitCommonTimeout(double interval);
        TimerGuidType StartCommonTimeout(CommonTimerIdxType idx, int64_t loopCnt, const EventCallbackType& cb);
        inline TimerGuidType StartCommonTimeoutOnce(CommonTimerIdxType idx, const EventCallbackType& cb)
        { return StartCommonTimeout(idx, 1, std::move(cb)); }
        inline TimerGuidType StartCommonTimeoutForever(CommonTimerIdxType idx, const EventCallbackType& cb)
        { return StartCommonTimeout(idx, -1, std::move(cb)); }

public:
        static bool IsLast(TimedEventItem& eventData);
        static time_t GetOverTime(TimedEventItem& eventData);
        static TimerGuidType GetGuid(TimedEventItem& eventData);
        static void SetOverTime(TimedEventItem& eventData, double overTime);
        static void SetInterval(TimedEventItem& eventData, double interval);
        void Stop(TimedEventItem& eventData);
        void Pause(TimedEventItem& eventData);

private:
        class Impl;
        Impl* this_;

private :
        DISABLE_COPY_AND_ASSIGN(TimedEventLoop);
};

TimedEventLoop& GetTimer();
TimedEventLoop& GetSteadyTimer();

class FrameEventItem;
class FrameEventLoop final
{
public:
        typedef std::function<void()> EventCallbackType;

        FrameEventLoop();
        ~FrameEventLoop();

        void Update();

        uint64_t Start(int64_t relativeFrameCnt, const EventCallbackType& cb);
        void Stop(uint64_t guid, bool isExcute = false);

        void Delay(const EventCallbackType& cb);
        void DelayForever(const EventCallbackType& cb);

private:
        class Impl;
        Impl* this_;

private:
        DISABLE_COPY_AND_ASSIGN(FrameEventLoop);
};

FrameEventLoop& GetFrameEventLoop();

// vim: fenc=utf8:expandtab:ts=8:noma
