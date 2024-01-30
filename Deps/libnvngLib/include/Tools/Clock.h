#pragma once

#include <string>
#include <time.h>
#include <atomic>

#define MIN_TO_SEC(val)       ((val) * 60)
#define HOUR_TO_SEC(val)      ((val) * MIN_TO_SEC(60))
#define DAY_TO_SEC(val)       ((val) * HOUR_TO_SEC(24))
#define WEEK_TO_SEC(val)      ((val) * DAY_TO_SEC(7))

#ifdef WIN32
#define LOCAL_TIME_SAFE(now, lt) localtime_s(lt, now);
#else
#define LOCAL_TIME_SAFE(now, lt) localtime_r(now, lt);
#endif

#define FORCE_INLINE BOOST_FORCEINLINE

class Clock
{
public :
        Clock() { }

        void UpdateTime();
        FORCE_INLINE time_t GetTimeStamp() const  { return static_cast<time_t>(_now); }
        FORCE_INLINE double GetTime() const       { return _now; }
        FORCE_INLINE double GetSteadyTime() const { return _steadyNow; }

        FORCE_INLINE int64_t GetYear() const { return _tm.load().tm_year + 1900; }
        FORCE_INLINE int64_t GetMonth() const { return _tm.load().tm_mon + 1; }
        FORCE_INLINE int64_t GetDay() const { return _tm.load().tm_mday; }
        FORCE_INLINE int64_t GetHour() const { return _tm.load().tm_hour; }
        FORCE_INLINE int64_t GetMin() const { return _tm.load().tm_min; }
        FORCE_INLINE int64_t GetSec() const { return _tm.load().tm_sec; }

        FORCE_INLINE int64_t GetWeekDay() const { return _tm.load().tm_wday; }
        FORCE_INLINE int64_t GetMonthDay() const { return _tm.load().tm_mday; }

private :
        std::atomic<double> _now = 0.0;
        std::atomic<double> _steadyNow = 0.0; // 不会随系统时间修改而出问题。心跳包需要，frame sleep需要
        std::atomic<struct tm> _tm = tm();

public :
        static Clock& GetClockInternal();

// {{{ clock static
public :
        static time_t GetTimeStampNow_Slow();
        static double GetTimeNow_Slow();
        static double GetSteadyTimeNow_Slow();
        static double GetHighResolutionTimeNow_Slow();

        // 不提倡在工程中使用 time string
        static const std::string GetTimeString_Slow(time_t time, const char* fmt="%F %T");
        static time_t ParseTimeFromString_Slow(const char* now, const char* fmt="%d-%d-%d %d:%d:%d");

        // 除年和月外，其它时间均可直接换算成秒
        static time_t TimeAdd_Slow(time_t now, int sec, int mon=0, int year=0);
        static time_t TimeSet_Slow(int year, int mon=1, int sec=0);

        enum E_CLEAR_TIME_TYPE
        {
                E_CTT_MON,
                E_CTT_DAY,
                E_CTT_Week,
                E_CTT_HOUR,
                E_CTT_MIN,
        };

        // 消除 year 后没有意义。
        static time_t TimeClear_Slow(time_t now, E_CLEAR_TIME_TYPE clearType);

        FORCE_INLINE static int32_t GetYear_Slow(time_t now)
        { struct tm lt; LOCAL_TIME_SAFE(&now, &lt); return lt.tm_year+1900; }
        FORCE_INLINE static int32_t GetMonth_Slow(time_t now)
        { struct tm lt; LOCAL_TIME_SAFE(&now, &lt); return lt.tm_mon+1; }
        FORCE_INLINE static int32_t GetDay_Slow(time_t now)
        { struct tm lt; LOCAL_TIME_SAFE(&now, &lt); return lt.tm_mday; }
        FORCE_INLINE static int32_t GetMin_Slow(time_t now)
        { struct tm lt; LOCAL_TIME_SAFE(&now, &lt); return lt.tm_min; }

        FORCE_INLINE static int64_t GetWeekDay(time_t now)
        { struct tm lt; LOCAL_TIME_SAFE(&now, &lt); return lt.tm_wday; }
        FORCE_INLINE static int64_t GetMonthDay(time_t now)
        { struct tm lt; LOCAL_TIME_SAFE(&now, &lt); return lt.tm_mday; }
// }}}

private :
        Clock(const Clock&) = delete;
        Clock& operator=(const Clock&) = delete;
};

inline Clock& GetClock()
{
        static Clock& cl = Clock::GetClockInternal();
        return cl;
}

class TimeCost
{
public :
        TimeCost(const std::string& prefix, double interval=.0);
        ~TimeCost();

private :
        const std::string _prefix;
        clock_t _beginTime;
        double  _interval;
};

class TimeCostReal
{
public:
        TimeCostReal(const std::string& prefix, double interval = .0);
        ~TimeCostReal();

private:
        const std::string _prefix;
        double _beginTime;
        double _interval;
};

// vim: fenc=utf8:expandtab:ts=8:noma
