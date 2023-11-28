#pragma once

#include <chrono>
#include <mutex>
#include <assert.h>

template <typename _Tag
        , std::size_t _Fx = 0
        , typename _Ty = uint64_t
        , std::size_t _Tx = 30
        , std::size_t _Px = 20
        , typename _Tt = std::chrono::seconds>
class Snowflake final
{
public:
        typedef _Ty GuidType;

        static_assert(_Px < 64);
        static_assert(std::chrono::steady_clock::is_steady);

        Snowflake() { }

        static bool Init(GuidType param)
        {
                if ((GuidType)-1 == sParam
                    && 0<=param && param<(1<<(_Tx-_Px)))
                {
                        sParam = param;
                        return true;
                }
                else
                {
                        return false;
                }
        }

        FORCE_INLINE GuidType operator()() { return Gen(); }

        static GuidType Gen()
        {
                assert((GuidType)-1 != sParam);
                typename _Tt::rep now = NowSteady() + sTimeDiff;

                std::lock_guard lock(sSeqMutex);
                if (sOldTime != now)
                {
                        sOldTime = now;
                        sSeqGuid = -1;
                }

                if (++sSeqGuid >= (GuidType)1 << _Px)
                {
                        while (sOldTime == now) { now = NowSteady() + sTimeDiff; }
                        sOldTime = now;
                        sSeqGuid = 0;
                }

                return (((GuidType)now - _Fx) << _Tx) | (sParam << _Px) | sSeqGuid;
        }

private:
        static FORCE_INLINE std::chrono::milliseconds::rep NowSteady()
        { return std::chrono::duration_cast<_Tt>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

        static FORCE_INLINE std::chrono::milliseconds::rep Now()
        { return std::chrono::duration_cast<_Tt>(std::chrono::system_clock::now().time_since_epoch()).count(); }

private:
        static GuidType sParam;
        static typename _Tt::rep sOldTime;
        static const typename _Tt::rep sTimeDiff;
        static SpinLock sSeqMutex;
        static GuidType sSeqGuid;
};

template <typename _Tag, std::size_t _Fx, typename _Ty, std::size_t _Tx, std::size_t _Px, typename _Tt>
_Ty Snowflake<_Tag, _Fx, _Ty, _Tx, _Px, _Tt>::sParam = (_Ty)-1;

template <typename _Tag, std::size_t _Fx, typename _Ty, std::size_t _Tx, std::size_t _Px, typename _Tt>
typename _Tt::rep Snowflake<_Tag, _Fx, _Ty, _Tx, _Px, _Tt>::sOldTime = 0;

template <typename _Tag, std::size_t _Fx, typename _Ty, std::size_t _Tx, std::size_t _Px, typename _Tt>
const typename _Tt::rep Snowflake<_Tag, _Fx, _Ty, _Tx, _Px, _Tt>::sTimeDiff = Now() - NowSteady();

template <typename _Tag, std::size_t _Fx, typename _Ty, std::size_t _Tx, std::size_t _Px, typename _Tt>
SpinLock Snowflake<_Tag, _Fx, _Ty, _Tx, _Px, _Tt>::sSeqMutex;

template <typename _Tag, std::size_t _Fx, typename _Ty, std::size_t _Tx, std::size_t _Px, typename _Tt>
_Ty Snowflake<_Tag, _Fx, _Ty, _Tx, _Px, _Tt>::sSeqGuid = 0;

// vim: fenc=utf8:expandtab:ts=8:noma// vim: fenc=utf8:expandtab:ts=8:noma// vim: fenc=utf8:expandtab:ts=8:noma
