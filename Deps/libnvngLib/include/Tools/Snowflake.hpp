#pragma once

#include <chrono>
#include <mutex>
#include <assert.h>

template <typename _Tag, typename _Ty = uint64_t, std::size_t _Fx = 0, std::size_t _Tx = 22, std::size_t _Px = 12, typename _Tt = std::chrono::milliseconds>
class Snowflake
{
public:
        typedef _Ty GuidType;

        static_assert(_Px < 64);
        static_assert(std::chrono::steady_clock::is_steady);

        Snowflake() { }

        static bool Init(GuidType param)
        {
                if ((GuidType)-1 == sParam)
                {
                        sParam = param;
                        return true;
                }
                else
                {
                        return false;
                }
        }

        inline GuidType operator()()
        {
                return Gen();
        }

        static GuidType Gen()
        {
                assert((GuidType)-1 != sParam);

                std::lock_guard lock(sSeqMutex);
                typename _Tt::rep now = NowSteady() + sTimeDiff;
                if (sOldTime != now)
                {
                        sOldTime = now;
                        sSeqGuid = -1;
                }

                if (++sSeqGuid >= (uint64_t)1 << _Px)
                {
                        while (sOldTime == now) { now = NowSteady() + sTimeDiff; }
                        sOldTime = now;
                        sSeqGuid = 0;
                }

                return (((GuidType)now - _Fx) << _Tx) | (sParam << _Px) | sSeqGuid;
        }

private:
        static inline std::chrono::milliseconds::rep NowSteady()
        {
                return std::chrono::duration_cast<_Tt>(std::chrono::steady_clock::now().time_since_epoch()).count();
        }

        static inline std::chrono::milliseconds::rep Now()
        {
                return std::chrono::duration_cast<_Tt>(std::chrono::system_clock::now().time_since_epoch()).count();
        }

private:
        static GuidType sParam;
        static typename _Tt::rep sOldTime;
        static typename _Tt::rep sTimeDiff;
        static SpinLock sSeqMutex;
        static uint64_t sSeqGuid;
};

template <typename _Tag, typename _Ty, std::size_t _Fx, std::size_t _Tx, std::size_t _Px, typename _Tt>
_Ty Snowflake<_Tag, _Ty, _Fx, _Tx, _Px, _Tt>::sParam = (_Ty)-1;

template <typename _Tag, typename _Ty, std::size_t _Fx, std::size_t _Tx, std::size_t _Px, typename _Tt>
typename _Tt::rep Snowflake<_Tag, _Ty, _Fx, _Tx, _Px, _Tt>::sOldTime = 0;

template <typename _Tag, typename _Ty, std::size_t _Fx, std::size_t _Tx, std::size_t _Px, typename _Tt>
typename _Tt::rep Snowflake<_Tag, _Ty, _Fx, _Tx, _Px, _Tt>::sTimeDiff = Now() - NowSteady();

template <typename _Tag, typename _Ty, std::size_t _Fx, std::size_t _Tx, std::size_t _Px, typename _Tt>
SpinLock Snowflake<_Tag, _Ty, _Fx, _Tx, _Px, _Tt>::sSeqMutex;

template <typename _Tag, typename _Ty, std::size_t _Fx, std::size_t _Tx, std::size_t _Px, typename _Tt>
uint64_t Snowflake<_Tag, _Ty, _Fx, _Tx, _Px, _Tt>::sSeqGuid = 0;

struct stSnowflakeUint64Tag {};
typedef Snowflake<stSnowflakeUint64Tag> SnowflakeUint64;

// vim: fenc=utf8:expandtab:ts=8:noma// vim: fenc=utf8:expandtab:ts=8:noma// vim: fenc=utf8:expandtab:ts=8:noma
