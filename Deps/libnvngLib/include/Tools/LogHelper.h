#pragma once

#include <signal.h>
#include <sstream>

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <spdlog/spdlog.h>

#if 0

#define BACKWARD_HAS_BFD 1
#include "Tools/backward.hpp"

#define STACK_TRACE_LOAD() \
        std::stringstream __internalStringStream__; \
        backward::StackTrace __internalStackTrace__; \
        __internalStackTrace__.load_here(128); \
        backward::Printer().print(__internalStackTrace__, __internalStringStream__)

#else

#define BOOST_STACKTRACE_USE_BACKTRACE
#include <boost/stacktrace.hpp>

#define STACK_TRACE_LOAD() \
        std::stringstream __internalStringStream__; \
        __internalStringStream__ << boost::stacktrace::stacktrace()

#endif

#include "Tools/Singleton.hpp"

#define CHECK_2N(x)     ((x)>0 && 0 == ((x)&((x)-1)))
#define FORCE_INLINE    BOOST_FORCEINLINE

enum ELogModuleType
{
        E_LOG_MT_None = 0x0,
        E_LOG_MT_Net = 0x1,
        E_LOG_MT_NetHttp = 0x2,
        E_LOG_MT_DBMySql = 0x3,
        E_LOG_MT_DBRedis = 0x4,
        E_LOG_MT_Player = 0x5,
        E_LOG_MT_Region = 0x6,
        E_LOG_MT_UserDefine = 0x10,
        E_LOG_MT_UserDefine_1 = 0x20,
        E_LOG_MT_UserDefine_2 = 0x30,
        E_LOG_MT_Invalid = 0x40,
};

template <size_t lv> constexpr const char* GetLoglevelName() { return ""; }
template <> constexpr const char* GetLoglevelName<spdlog::level::trace>() { return "trace"; }
template <> constexpr const char* GetLoglevelName<spdlog::level::info>() { return "info"; }
template <> constexpr const char* GetLoglevelName<spdlog::level::warn>() { return "warn"; }
template <> constexpr const char* GetLoglevelName<spdlog::level::err>() { return "err"; }
template <> constexpr const char* GetLoglevelName<spdlog::level::critical>() { return "critical"; }

template <size_t lv> constexpr const char* GetLogModuleName() { return ""; }
template <> constexpr const char* GetLogModuleName<E_LOG_MT_None>() { return ""; }
template <> constexpr const char* GetLogModuleName<E_LOG_MT_Net>() { return "net"; }
template <> constexpr const char* GetLogModuleName<E_LOG_MT_NetHttp>() { return "http"; }
template <> constexpr const char* GetLogModuleName<E_LOG_MT_DBMySql>() { return "mysql"; }
template <> constexpr const char* GetLogModuleName<E_LOG_MT_DBRedis>() { return "redis"; }
template <> constexpr const char* GetLogModuleName<E_LOG_MT_Player>() { return "p"; }
template <> constexpr const char* GetLogModuleName<E_LOG_MT_Region>() { return "r"; }

template <size_t lv> constexpr uint64_t GetLogModuleFilterLevel() { return 0; }
template <> constexpr uint64_t GetLogModuleFilterLevel<E_LOG_MT_None>() { return 1<<E_LOG_MT_None; }
template <> constexpr uint64_t GetLogModuleFilterLevel<E_LOG_MT_Net>() { return 1<<E_LOG_MT_Net; }
template <> constexpr uint64_t GetLogModuleFilterLevel<E_LOG_MT_NetHttp>() { return 1<<E_LOG_MT_NetHttp; }
template <> constexpr uint64_t GetLogModuleFilterLevel<E_LOG_MT_DBMySql>() { return 1<<E_LOG_MT_DBMySql; }
template <> constexpr uint64_t GetLogModuleFilterLevel<E_LOG_MT_DBRedis>() { return 1<<E_LOG_MT_DBRedis; }
template <> constexpr uint64_t GetLogModuleFilterLevel<E_LOG_MT_Player>() { return 1<<E_LOG_MT_Player; }
template <> constexpr uint64_t GetLogModuleFilterLevel<E_LOG_MT_Region>() { return 1<<E_LOG_MT_Region; }

template <size_t lv> constexpr uint64_t GetLogModuleParallelCnt() { return 4; }

#define LOG_FUNC(m, lv, ...)            SPDLOG_LOGGER_CALL(LogHelper::GetInstance()->GetLogger<m>(), lv, ## __VA_ARGS__)
#define LOG_FUNC_IDX(m, idx, lv, ...)   SPDLOG_LOGGER_CALL(LogHelper::GetInstance()->GetLogger<m>(idx), lv, ## __VA_ARGS__)

// {{{ LOG IDX
#define LOG_TRACE_IDX_M(m, idx, ...)            LOG_FUNC_IDX(m, idx, spdlog::level::trace, ## __VA_ARGS__)
#define LOG_INFO_IDX_M(m, idx, ...)             LOG_FUNC_IDX(m, idx, spdlog::level::info, ## __VA_ARGS__)
#define LOG_WARN_IDX_M(m, idx, ...)             LOG_FUNC_IDX(m, idx, spdlog::level::warn, ## __VA_ARGS__)
#define LOG_ERROR_IDX_M(m, idx, msg, ...)       do { STACK_TRACE_LOAD(); LOG_FUNC_IDX(m, idx, spdlog::level::err, "{}", LogHelper::GetInstance()->StackString(fmt::format(msg, ## __VA_ARGS__), __internalStringStream__)); } while(0)
#define LOG_FATAL_IDX_M(m, idx, msg, ...)       do { STACK_TRACE_LOAD(); LOG_FUNC_IDX(m, idx, spdlog::level::critical, "{}", LogHelper::GetInstance()->StackString(fmt::format(msg, ## __VA_ARGS__), __internalStringStream__)); raise(SIGINT); } while(0)

#define LOG_TRACE_IDX_M_IF(m, idx, cond, ...)   if (cond) { LOG_TRACE_IDX_M(m, idx, ## __VA_ARGS__); }
#define LOG_INFO_IDX_M_IF(m, idx, cond, ...)    if (cond) { LOG_INFO_IDX_M(m, idx, ## __VA_ARGS__); }
#define LOG_WARN_IDX_M_IF(m, idx, cond, ...)    if (cond) { LOG_WARN_IDX_M(m, idx, ## __VA_ARGS__); }
#define LOG_ERROR_IDX_M_IF(m, idx, cond, ...)   if (cond) { LOG_ERROR_IDX_M(m, idx, ## __VA_ARGS__); }
#define LOG_FATAL_IDX_M_IF(m, idx, cond, ...)   if (cond) { LOG_FATAL_IDX_M(m, idx, ## __VA_ARGS__); }

#define LOG_FILTER_IDX_M(m, idx, ...)           if (FLAG_HAS(LogHelper::sLogFilterFlag, GetLogModuleFilterLevel<m>())) { LOG_INFO_IDX_M(m, idx, ## __VA_ARGS__); }

// {{{ LOG PLAYER

#if 0

#define PLAYER_LOG_TRACE(idx, ...)              LOG_TRACE_IDX_M(ELogModuleType::E_LOG_MT_Player, idx, ## __VA_ARGS__)
#define PLAYER_LOG_INFO(idx, ...)               LOG_INFO_IDX_M(ELogModuleType::E_LOG_MT_Player, idx, ## __VA_ARGS__)
#define PLAYER_LOG_WARN(idx, ...)               LOG_WARN_IDX_M(ELogModuleType::E_LOG_MT_Player, idx, ## __VA_ARGS__)
#define PLAYER_LOG_ERROR(idx, ...)              LOG_ERROR_IDX_M(ELogModuleType::E_LOG_MT_Player, idx, ## __VA_ARGS__)
#define PLAYER_LOG_FATAL(idx, ...)              LOG_FATAL_IDX_M(ELogModuleType::E_LOG_MT_Player, idx, ## __VA_ARGS__)

#define PLAYER_LOG_TRACE_IF(idx, cond, ...)     LOG_TRACE_IDX_M_IF(ELogModuleType::E_LOG_MT_Player, idx, cond, ## __VA_ARGS__)
#define PLAYER_LOG_INFO_IF(idx, cond, ...)      LOG_INFO_IDX_M_IF(ELogModuleType::E_LOG_MT_Player, idx, cond, ## __VA_ARGS__)
#define PLAYER_LOG_WARN_IF(idx, cond, ...)      LOG_WARN_IDX_M_IF(ELogModuleType::E_LOG_MT_Player, idx, cond, ## __VA_ARGS__)
#define PLAYER_LOG_ERROR_IF(idx, cond, ...)     LOG_ERROR_IDX_M_IF(ELogModuleType::E_LOG_MT_Player, idx, cond, ## __VA_ARGS__)
#define PLAYER_LOG_FATAL_IF(idx, cond, ...)     LOG_FATAL_IDX_M_IF(ELogModuleType::E_LOG_MT_Player, idx, cond, ## __VA_ARGS__)

#else

#define PLAYER_LOG_TRACE(idx, ...)              LOG_TRACE_M(ELogModuleType::E_LOG_MT_Player, ## __VA_ARGS__)
#define PLAYER_LOG_INFO(idx, ...)               LOG_INFO_M(ELogModuleType::E_LOG_MT_Player, ## __VA_ARGS__)
#define PLAYER_LOG_WARN(idx, ...)               LOG_WARN_M(ELogModuleType::E_LOG_MT_Player, ## __VA_ARGS__)
#define PLAYER_LOG_ERROR(idx, ...)              LOG_ERROR_M(ELogModuleType::E_LOG_MT_Player, ## __VA_ARGS__)
#define PLAYER_LOG_ERROR_PS(idx, ...)           LOG_ERROR_PS_M(ELogModuleType::E_LOG_MT_Player, ## __VA_ARGS__)
#define PLAYER_LOG_FATAL(idx, ...)              LOG_FATAL_M(ELogModuleType::E_LOG_MT_Player, ## __VA_ARGS__)

#define PLAYER_LOG_TRACE_IF(idx, cond, ...)     LOG_TRACE_M_IF(ELogModuleType::E_LOG_MT_Player, cond, ## __VA_ARGS__)
#define PLAYER_LOG_INFO_IF(idx, cond, ...)      LOG_INFO_M_IF(ELogModuleType::E_LOG_MT_Player, cond, ## __VA_ARGS__)
#define PLAYER_LOG_WARN_IF(idx, cond, ...)      LOG_WARN_M_IF(ELogModuleType::E_LOG_MT_Player, cond, ## __VA_ARGS__)
#define PLAYER_LOG_ERROR_IF(idx, cond, ...)     LOG_ERROR_M_IF(ELogModuleType::E_LOG_MT_Player, cond, ## __VA_ARGS__)
#define PLAYER_LOG_ERROR_PS_IF(idx, cond, ...)  LOG_ERROR_PS_M_IF(ELogModuleType::E_LOG_MT_Player, cond, ## __VA_ARGS__)
#define PLAYER_LOG_FATAL_IF(idx, cond, ...)     LOG_FATAL_M_IF(ELogModuleType::E_LOG_MT_Player, cond, ## __VA_ARGS__)

#endif

#ifndef NDEBUG

#define PLAYER_DLOG_TRACE                       PLAYER_LOG_TRACE
#define PLAYER_DLOG_INFO                        PLAYER_LOG_INFO
#define PLAYER_DLOG_WARN                        PLAYER_LOG_WARN
#define PLAYER_DLOG_ERROR                       PLAYER_LOG_ERROR
#define PLAYER_DLOG_FATAL                       PLAYER_LOG_FATAL

#define PLAYER_DLOG_TRACE_IF                    PLAYER_LOG_TRACE_IF
#define PLAYER_DLOG_INFO_IF                     PLAYER_LOG_INFO_IF
#define PLAYER_DLOG_WARN_IF                     PLAYER_LOG_WARN_IF
#define PLAYER_DLOG_ERROR_IF                    PLAYER_LOG_ERROR_IF
#define PLAYER_DLOG_FATAL_IF                    PLAYER_LOG_FATAL_IF

#else

#define PLAYER_DLOG_TRACE(...)                  (void)0
#define PLAYER_DLOG_INFO(...)                   (void)0
#define PLAYER_DLOG_WARN(...)                   (void)0
#define PLAYER_DLOG_ERROR(...)                  (void)0
#define PLAYER_DLOG_FATAL(...)                  (void)0

#define PLAYER_DLOG_TRACE_IF(...)               (void)0
#define PLAYER_DLOG_INFO_IF(...)                (void)0
#define PLAYER_DLOG_WARN_IF(...)                (void)0
#define PLAYER_DLOG_ERROR_IF(...)               (void)0
#define PLAYER_DLOG_FATAL_IF(...)               (void)0

#endif

// }}}

// {{{ LOG REGION

#if 0

#define REGION_LOG_TRACE(idx, ...)              LOG_TRACE_IDX_M(ELogModuleType::E_LOG_MT_Region, idx, ## __VA_ARGS__)
#define REGION_LOG_INFO(idx, ...)               LOG_INFO_IDX_M(ELogModuleType::E_LOG_MT_Region, idx, ## __VA_ARGS__)
#define REGION_LOG_WARN(idx, ...)               LOG_WARN_IDX_M(ELogModuleType::E_LOG_MT_Region, idx, ## __VA_ARGS__)
#define REGION_LOG_ERROR(idx, ...)              LOG_ERROR_IDX_M(ELogModuleType::E_LOG_MT_Region, idx, ## __VA_ARGS__)
#define REGION_LOG_FATAL(idx, ...)              LOG_FATAL_IDX_M(ELogModuleType::E_LOG_MT_Region, idx, ## __VA_ARGS__)

#define REGION_LOG_TRACE_IF(idx, cond, ...)     LOG_TRACE_IDX_M_IF(ELogModuleType::E_LOG_MT_Region, idx, cond, ## __VA_ARGS__)
#define REGION_LOG_INFO_IF(idx, cond, ...)      LOG_INFO_IDX_M_IF(ELogModuleType::E_LOG_MT_Region, idx, cond, ## __VA_ARGS__)
#define REGION_LOG_WARN_IF(idx, cond, ...)      LOG_WARN_IDX_M_IF(ELogModuleType::E_LOG_MT_Region, idx, cond, ## __VA_ARGS__)
#define REGION_LOG_ERROR_IF(idx, cond, ...)     LOG_ERROR_IDX_M_IF(ELogModuleType::E_LOG_MT_Region, idx, cond, ## __VA_ARGS__)
#define REGION_LOG_FATAL_IF(idx, cond, ...)     LOG_FATAL_IDX_M_IF(ELogModuleType::E_LOG_MT_Region, idx, cond, ## __VA_ARGS__)

#else

#define REGION_LOG_TRACE(idx, ...)              LOG_TRACE_M(ELogModuleType::E_LOG_MT_Region, ## __VA_ARGS__)
#define REGION_LOG_INFO(idx, ...)               LOG_INFO_M(ELogModuleType::E_LOG_MT_Region, ## __VA_ARGS__)
#define REGION_LOG_WARN(idx, ...)               LOG_WARN_M(ELogModuleType::E_LOG_MT_Region, ## __VA_ARGS__)
#define REGION_LOG_ERROR(idx, ...)              LOG_ERROR_M(ELogModuleType::E_LOG_MT_Region, ## __VA_ARGS__)
#define REGION_LOG_ERROR_PS(idx, ...)           LOG_ERROR_PS_M(ELogModuleType::E_LOG_MT_Region, ## __VA_ARGS__)
#define REGION_LOG_FATAL(idx, ...)              LOG_FATAL_M(ELogModuleType::E_LOG_MT_Region, ## __VA_ARGS__)

#define REGION_LOG_TRACE_IF(idx, cond, ...)     LOG_TRACE_M_IF(ELogModuleType::E_LOG_MT_Region, cond, ## __VA_ARGS__)
#define REGION_LOG_INFO_IF(idx, cond, ...)      LOG_INFO_M_IF(ELogModuleType::E_LOG_MT_Region, cond, ## __VA_ARGS__)
#define REGION_LOG_WARN_IF(idx, cond, ...)      LOG_WARN_M_IF(ELogModuleType::E_LOG_MT_Region, cond, ## __VA_ARGS__)
#define REGION_LOG_ERROR_IF(idx, cond, ...)     LOG_ERROR_M_IF(ELogModuleType::E_LOG_MT_Region, cond, ## __VA_ARGS__)
#define REGION_LOG_FATAL_IF(idx, cond, ...)     LOG_FATAL_M_IF(ELogModuleType::E_LOG_MT_Region, cond, ## __VA_ARGS__)

#endif

#ifndef NDEBUG

#define REGION_DLOG_TRACE                       REGION_LOG_TRACE
#define REGION_DLOG_INFO                        REGION_LOG_INFO
#define REGION_DLOG_WARN                        REGION_LOG_WARN
#define REGION_DLOG_ERROR                       REGION_LOG_ERROR
#define REGION_DLOG_FATAL                       REGION_LOG_FATAL

#define REGION_DLOG_TRACE_IF                    REGION_LOG_TRACE_IF
#define REGION_DLOG_INFO_IF                     REGION_LOG_INFO_IF
#define REGION_DLOG_WARN_IF                     REGION_LOG_WARN_IF
#define REGION_DLOG_ERROR_IF                    REGION_LOG_ERROR_IF
#define REGION_DLOG_FATAL_IF                    REGION_LOG_FATAL_IF

#else

#define REGION_DLOG_TRACE(...)                  (void)0
#define REGION_DLOG_INFO(...)                   (void)0
#define REGION_DLOG_WARN(...)                   (void)0
#define REGION_DLOG_ERROR(...)                  (void)0
#define REGION_DLOG_FATAL(...)                  (void)0

#define REGION_DLOG_TRACE_IF(...)               (void)0
#define REGION_DLOG_INFO_IF(...)                (void)0
#define REGION_DLOG_WARN_IF(...)                (void)0
#define REGION_DLOG_ERROR_IF(...)               (void)0
#define REGION_DLOG_FATAL_IF(...)               (void)0

#endif

// }}}

// }}}

// {{{ LOG
#define LOG_TRACE_M(m, ...)             LOG_FUNC(m, spdlog::level::trace, ## __VA_ARGS__)
#define LOG_INFO_M(m, ...)              LOG_FUNC(m, spdlog::level::info, ## __VA_ARGS__)
#define LOG_WARN_M(m, ...)              LOG_FUNC(m, spdlog::level::warn, ## __VA_ARGS__)
#define LOG_ERROR_M(m, ...)             LOG_FUNC(m, spdlog::level::err, ## __VA_ARGS__)
#define LOG_ERROR_PS_M(m, msg, ...)     do { STACK_TRACE_LOAD(); LOG_FUNC(m, spdlog::level::err, "{}", LogHelper::GetInstance()->StackString(fmt::format(msg, ## __VA_ARGS__), __internalStringStream__)); } while(0)
#define LOG_FATAL_M(m, msg, ...)        do { STACK_TRACE_LOAD(); LOG_FUNC(m, spdlog::level::critical, "{}", LogHelper::GetInstance()->StackString(fmt::format(msg, ## __VA_ARGS__), __internalStringStream__)); raise(SIGINT); } while(0)


#define LOG_TRACE(...)                  LOG_TRACE_M(ELogModuleType::E_LOG_MT_None, ## __VA_ARGS__)
#define LOG_INFO(...)                   LOG_INFO_M(ELogModuleType::E_LOG_MT_None, ## __VA_ARGS__)
#define LOG_WARN(...)                   LOG_WARN_M(ELogModuleType::E_LOG_MT_None, ## __VA_ARGS__)
#define LOG_ERROR(...)                  LOG_ERROR_M(ELogModuleType::E_LOG_MT_None, ## __VA_ARGS__)
#define LOG_ERROR_PS(...)               LOG_ERROR_PS_M(ELogModuleType::E_LOG_MT_None, ## __VA_ARGS__)
#define LOG_FATAL(...)                  LOG_FATAL_M(ELogModuleType::E_LOG_MT_None, ## __VA_ARGS__)

#define LOG_TRACE_IF(cond, ...)         if (cond) { LOG_TRACE_M(ELogModuleType::E_LOG_MT_None, ## __VA_ARGS__); }
#define LOG_INFO_IF(cond, ...)          if (cond) { LOG_INFO_M(ELogModuleType::E_LOG_MT_None, ## __VA_ARGS__); }
#define LOG_WARN_IF(cond, ...)          if (cond) { LOG_WARN_M(ELogModuleType::E_LOG_MT_None, ## __VA_ARGS__); }
#define LOG_ERROR_IF(cond, ...)         if (cond) { LOG_ERROR_M(ELogModuleType::E_LOG_MT_None, ## __VA_ARGS__); }
#define LOG_ERROR_PS_IF(cond, ...)      if (cond) { LOG_ERROR_PS_M(ELogModuleType::E_LOG_MT_None, ## __VA_ARGS__); }
#define LOG_FATAL_IF(cond, ...)         if (cond) { LOG_FATAL_M(ELogModuleType::E_LOG_MT_None, ## __VA_ARGS__); }

#define LOG_TRACE_V(v, ...)             if (FLAG_HAS(LogHelper::sLogFilterFlag, v)) LOG_TRACE(ELogModuleType::E_LOG_MT_None, ## __VA_ARGS__)
#define LOG_INFO_V(v, ...)              if (FLAG_HAS(LogHelper::sLogFilterFlag, v)) LOG_INFO(ELogModuleType::E_LOG_MT_None, ## __VA_ARGS__)
#define LOG_WARN_V(v, ...)              if (FLAG_HAS(LogHelper::sLogFilterFlag, v)) LOG_WARN(ELogModuleType::E_LOG_MT_None, ## __VA_ARGS__)
#define LOG_ERROR_V(v, ...)             if (FLAG_HAS(LogHelper::sLogFilterFlag, v)) LOG_ERROR(ELogModuleType::E_LOG_MT_None, ## __VA_ARGS__)
#define LOG_FATAL_V(v, ...)             if (FLAG_HAS(LogHelper::sLogFilterFlag, v)) LOG_FATAL(ELogModuleType::E_LOG_MT_None, ## __VA_ARGS__)

#define LOG_TRACE_IF_V(v, cond, ...)    if (cond && FLAG_HAS(LogHelper::sLogFilterFlag, v)) LOG_TRACE(ELogModuleType::E_LOG_MT_None, ## __VA_ARGS__)
#define LOG_INFO_IF_V(v, cond, ...)     if (cond && FLAG_HAS(LogHelper::sLogFilterFlag, v)) LOG_INFO(ELogModuleType::E_LOG_MT_None, ## __VA_ARGS__)
#define LOG_WARN_IF_V(v, cond, ...)     if (cond && FLAG_HAS(LogHelper::sLogFilterFlag, v)) LOG_WARN(ELogModuleType::E_LOG_MT_None, ## __VA_ARGS__)
#define LOG_ERROR_IF_V(v, cond, ...)    if (cond && FLAG_HAS(LogHelper::sLogFilterFlag, v)) LOG_ERROR(ELogModuleType::E_LOG_MT_None, ## __VA_ARGS__)
#define LOG_FATAL_IF_V(v, cond, ...)    if (cond && FLAG_HAS(LogHelper::sLogFilterFlag, v)) LOG_FATAL(ELogModuleType::E_LOG_MT_None, ## __VA_ARGS__)

#define LOG_TRACE_M_IF(m, cond, ...)    if (cond) { LOG_TRACE_M(m, ## __VA_ARGS__); }
#define LOG_INFO_M_IF(m, cond, ...)     if (cond) { LOG_INFO_M(m, ## __VA_ARGS__); }
#define LOG_WARN_M_IF(m, cond, ...)     if (cond) { LOG_WARN_M(m, ## __VA_ARGS__); }
#define LOG_ERROR_M_IF(m, cond, ...)    if (cond) { LOG_ERROR_M(m, ## __VA_ARGS__); }
#define LOG_ERROR_PS_M_IF(m, cond, ...) if (cond) { LOG_ERROR_PS_M(m, ## __VA_ARGS__); }
#define LOG_FATAL_M_IF(m, cond, ...)    if (cond) { LOG_FATAL_M(m, ## __VA_ARGS__); }

#define LOG_TRACE_M_V(m, ...)           if (FLAG_HAS(LogHelper::sLogFilterFlag, GetLogModuleFilterLevel<m>())) LOG_TRACE_M(m, ## __VA_ARGS__)
#define LOG_INFO_M_V(m, ...)            if (FLAG_HAS(LogHelper::sLogFilterFlag, GetLogModuleFilterLevel<m>())) LOG_INFO_M(m, ## __VA_ARGS__)
#define LOG_WARN_M_V(m, ...)            if (FLAG_HAS(LogHelper::sLogFilterFlag, GetLogModuleFilterLevel<m>())) LOG_WARN_M(m, ## __VA_ARGS__)
#define LOG_ERROR_M_V(m, ...)           if (FLAG_HAS(LogHelper::sLogFilterFlag, GetLogModuleFilterLevel<m>())) LOG_ERROR_M(m, ## __VA_ARGS__)
#define LOG_ERROR_PS_M_V(m, ...)        if (FLAG_HAS(LogHelper::sLogFilterFlag, GetLogModuleFilterLevel<m>())) LOG_ERROR_PS_M(m, ## __VA_ARGS__)
#define LOG_FATAL_M_V(m, ...)           if (FLAG_HAS(LogHelper::sLogFilterFlag, GetLogModuleFilterLevel<m>())) LOG_FATAL_M(m, ## __VA_ARGS__)

#define LOG_TRACE_M_IF_V(m, cond, ...)    if (cond && FLAG_HAS(LogHelper::sLogFilterFlag, GetLogModuleFilterLevel<m>())) LOG_TRACE_M(m, ## __VA_ARGS__)
#define LOG_INFO_M_IF_V(m, cond, ...)     if (cond && FLAG_HAS(LogHelper::sLogFilterFlag, GetLogModuleFilterLevel<m>())) LOG_INFO_M(m, ## __VA_ARGS__)
#define LOG_WARN_M_IF_V(m, cond, ...)     if (cond && FLAG_HAS(LogHelper::sLogFilterFlag, GetLogModuleFilterLevel<m>())) LOG_WARN_M(m, ## __VA_ARGS__)
#define LOG_ERROR_M_IF_V(m, cond, ...)    if (cond && FLAG_HAS(LogHelper::sLogFilterFlag, GetLogModuleFilterLevel<m>())) LOG_ERROR_M(m, ## __VA_ARGS__)
#define LOG_ERROR_PS_M_IF_V(m, cond, ...) if (cond && FLAG_HAS(LogHelper::sLogFilterFlag, GetLogModuleFilterLevel<m>())) LOG_ERROR_PS_M(m, ## __VA_ARGS__)
#define LOG_FATAL_M_IF_V(m, cond, ...)    if (cond && FLAG_HAS(LogHelper::sLogFilterFlag, GetLogModuleFilterLevel<m>())) LOG_FATAL_M(m, ## __VA_ARGS__)

#define LOG_FILTER(v, ...)              if (FLAG_HAS(LogHelper::sLogFilterFlag, v)) { LOG_INFO_M(ELogModuleType::E_LOG_MT_None, ## __VA_ARGS__); }
#define LOG_FILTER_M(m, ...)            if (FLAG_HAS(LogHelper::sLogFilterFlag, GetLogModuleFilterLevel<m>())) { LOG_INFO_M(m, ## __VA_ARGS__); }
// }}}

// {{{ LOG DEBUG
#ifndef NDEBUG

#define DLOG_TRACE_M            LOG_TRACE_M
#define DLOG_INFO_M             LOG_INFO_M
#define DLOG_WARN_M             LOG_WARN_M
#define DLOG_ERROR_M            LOG_ERROR_M
#define DLOG_ERROR_PS_M         LOG_ERROR_PS_M
#define DLOG_FATAL_M            LOG_FATAL_M

#define DLOG_TRACE              LOG_TRACE
#define DLOG_INFO               LOG_INFO
#define DLOG_WARN               LOG_WARN
#define DLOG_ERROR              LOG_ERROR
#define DLOG_FATAL              LOG_FATAL

#define DLOG_TRACE_V            LOG_TRACE_V
#define DLOG_INFO_V             LOG_INFO_V
#define DLOG_WARN_V             LOG_WARN_V
#define DLOG_ERROR_V            LOG_ERROR_V
#define DLOG_FATAL_V            LOG_FATAL_V

#define DLOG_TRACE_IF_V         LOG_TRACE_IF_V
#define DLOG_INFO_IF_V          LOG_INFO_IF_V
#define DLOG_WARN_IF_V          LOG_WARN_IF_V
#define DLOG_WARN_IF_V          LOG_WARN_IF_V
#define DLOG_FATAL_IF_V         LOG_FATAL_IF_V

#define DLOG_TRACE_M_IF         LOG_TRACE_M_IF
#define DLOG_INFO_M_IF          LOG_INFO_M_IF
#define DLOG_WARN_M_IF          LOG_WARN_M_IF
#define DLOG_ERROR_M_IF         LOG_ERROR_M_IF
#define DLOG_FATAL_M_IF         LOG_FATAL_M_IF

#define DLOG_TRACE_IF           LOG_TRACE_IF
#define DLOG_INFO_IF            LOG_INFO_IF
#define DLOG_WARN_IF            LOG_WARN_IF
#define DLOG_ERROR_IF           LOG_ERROR_IF
#define DLOG_FATAL_IF           LOG_FATAL_IF

#define DLOG_TRACE_M_V          LOG_TRACE_M_V
#define DLOG_INFO_M_V           LOG_INFO_M_V
#define DLOG_WARN_M_V           LOG_WARN_M_V
#define DLOG_ERROR_M_V          LOG_ERROR_M_V
#define DLOG_FATAL_M_V          LOG_FATAL_M_V

#define DLOG_TRACE_M_IF_V       LOG_TRACE_M_IF_V
#define DLOG_INFO_M_IF_V        LOG_INFO_M_IF_V
#define DLOG_WARN_M_IF_V        LOG_WARN_M_IF_V
#define DLOG_ERROR_M_IF_V       LOG_ERROR_M_IF_V
#define DLOG_FATAL_M_IF_V       LOG_FATAL_M_IF_V

#define DLOG_FILTER             LOG_FILTER
#define DLOG_FILTER_M           LOG_FILTER_M

#else

#define DLOG_TRACE_M(...)       (void)0
#define DLOG_INFO_M(...)        (void)0
#define DLOG_WARN_M(...)        (void)0
#define DLOG_ERROR_M(...)       (void)0
#define DLOG_FATAL_M(...)       (void)0

#define DLOG_TRACE(...)         (void)0
#define DLOG_INFO(...)          (void)0
#define DLOG_WARN(...)          (void)0
#define DLOG_ERROR(...)         (void)0
#define DLOG_FATAL(...)         (void)0

#define DLOG_TRACE_V(...)       (void)0
#define DLOG_INFO_V(...)        (void)0
#define DLOG_WARN_V(...)        (void)0
#define DLOG_ERROR_V(...)       (void)0
#define DLOG_FATAL_V(...)       (void)0

#define DLOG_TRACE_IF_V(...)    (void)0
#define DLOG_INFO_IF_V(...)     (void)0
#define DLOG_WARN_IF_V(...)     (void)0
#define DLOG_WARN_IF_V(...)     (void)0
#define DLOG_FATAL_IF_V(...)    (void)0

#define DLOG_TRACE_M_IF(...)    (void)0
#define DLOG_INFO_M_IF(...)     (void)0
#define DLOG_WARN_M_IF(...)     (void)0
#define DLOG_ERROR_M_IF(...)    (void)0
#define DLOG_FATAL_M_IF(...)    (void)0

#define DLOG_TRACE_IF(...)      (void)0
#define DLOG_INFO_IF(...)       (void)0
#define DLOG_WARN_IF(...)       (void)0
#define DLOG_ERROR_IF(...)      (void)0
#define DLOG_FATAL_IF(...)      (void)0

#define DLOG_TRACE_M_V(...)     (void)0
#define DLOG_INFO_M_V(...)      (void)0
#define DLOG_WARN_M_V(...)      (void)0
#define DLOG_ERROR_M_V(...)     (void)0
#define DLOG_FATAL_M_V(...)     (void)0

#define DLOG_TRACE_M_IF_V(...)  (void)0
#define DLOG_INFO_M_IF_V(...)   (void)0
#define DLOG_WARN_M_IF_V(...)   (void)0
#define DLOG_ERROR_M_IF_V(...)  (void)0
#define DLOG_FATAL_M_IF_V(...)  (void)0

#define DLOG_FILTER(...)        (void)0
#define DLOG_FILTER_M(...)      (void)0

#endif
// }}}

class LogHelper final : public Singleton<LogHelper>
{
        LogHelper();
        ~LogHelper();
        friend class Singleton<LogHelper>;

public:
        bool Init();
        std::vector<spdlog::sink_ptr> CreateSinkList(const std::string& mName);
        std::string StackString(const std::string& msg, const std::stringstream& ss);
        void Flush();

        template <ELogModuleType _My>
        FORCE_INLINE std::shared_ptr<spdlog::logger> GetLogger()
        {
                static std::shared_ptr<spdlog::logger> logger = [this]() -> std::shared_ptr<spdlog::logger> {
                        std::lock_guard l(_commonSinkListMutex);
                        if (_appName.empty())
                        {
                                STACK_TRACE_LOAD();
                                StackString("create logger before init!!!", __internalStringStream__);
                                raise(SIGINT);
                                return nullptr;
                        }

                        std::string mName = GetLogModuleName<_My>();
                        switch (_My)
                        {
                        case ELogModuleType::E_LOG_MT_None : break;
                        default : mName += " "; break;
                        }

                        auto logger = std::make_shared<spdlog::logger>(mName, _commonSinkList.begin(), _commonSinkList.end());
                        spdlog::register_logger(logger);
                        switch (_My)
                        {
                        case ELogModuleType::E_LOG_MT_None :
                                spdlog::set_default_logger(logger);
                                break;
                        default : break;
                        }
                        return logger;
                }();
                return logger;
        }

        template <ELogModuleType _My>
        FORCE_INLINE std::shared_ptr<spdlog::logger> GetLogger(uint64_t id)
        {
                static constexpr int64_t arrSize = GetLogModuleParallelCnt<_My>() - 1;
                static auto loggerArr = [this]() {
                        static std::shared_ptr<spdlog::logger> loggerArr[arrSize + 1];
                        std::lock_guard l(_commonSinkListMutex);
                        if (!CHECK_2N(arrSize+1) || _appName.empty())
                        {
                                STACK_TRACE_LOAD();
                                StackString("create logger before init!!!", __internalStringStream__);
                                raise(SIGINT);
                                return loggerArr;
                        }

                        for (int64_t i=0; i<arrSize+1; ++i)
                        {
                                std::string mName = fmt::format("{}_{}", GetLogModuleName<_My>(), i);
                                auto sinkList = CreateSinkList(mName);
                                auto logger = std::make_shared<spdlog::logger>(mName, sinkList.begin(), sinkList.end());
                                spdlog::register_logger(logger);
                                loggerArr[i] = logger;
                        }
                        return loggerArr;
                }();
                return loggerArr[id & arrSize];
        }

        std::mutex _commonSinkListMutex;
        std::vector<spdlog::sink_ptr> _commonSinkList;
        static int64_t sLogLevel;
        static int64_t sLogV;
        static std::string sLogDir;
        static uint64_t sLogFilterFlag;
        static uint64_t sLogMaxFiles;
        static uint64_t sLogMaxSize;

private:
        std::string _appName;
};

// vim: fenc=utf8:expandtab:ts=8:noma
