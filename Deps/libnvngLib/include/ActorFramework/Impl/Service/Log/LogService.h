#pragma once

#include "ActorFramework/ServiceExtra.hpp"

#include "msg_log.pb.h"

SPECIAL_ACTOR_DEFINE_BEGIN(LogActor, E_MIMT_Log);

public :
        explicit LogActor(uint64_t idx)
                : _idx(idx)
        {
        }

        bool Init() override;
        void InitSaveTimer();
        void DealTimeout();

        const uint64_t _idx = 0;

#ifdef LOG_SERVICE_SERVER
        std::shared_ptr<MsgLogServiceRegisterItem> _sqlPrefixArr[ELogServiceLogMainType_ARRAYSIZE];
        std::vector<std::shared_ptr<MsgLogServiceLog>> _logList;
#endif

#ifdef LOG_SERVICE_CLIENT
        uint64_t _sesIdx = 0;
        std::shared_ptr<MsgLogServiceLog> ResetCache();
        SpinLock _cacheMutex;
        std::shared_ptr<MsgLogServiceLog> _cache;
#endif

SPECIAL_ACTOR_DEFINE_END(LogActor);

template <ELogServiceLogMainType LogType>
consteval const char* LogServiceTableCreate() { return ""; }

template <ELogServiceLogMainType LogType>
consteval const char* LogServiceSqlPrefix() { return ""; }

template <ELogServiceLogMainType LogType>
consteval const char* LogServiceFmt() { return ""; }

template <>
consteval const char* LogServiceTableCreate<E_LSLMT_Content>()
{
        return "CREATE TABLE IF NOT EXISTS `log_content_0` ( \
                `id` bigint(0) NOT NULL AUTO_INCREMENT COMMENT '唯一ID', \
                `guid` bigint(0) NULL DEFAULT NULL COMMENT '玩家ID', \
                `t` bigint(0) NULL DEFAULT NULL COMMENT '类型', \
                `p` bigint(0) NULL DEFAULT NULL COMMENT '参数', \
                `content` text CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL COMMENT '内容', \
                `time` bigint(0) NULL DEFAULT NULL COMMENT '操作时间', \
                `lt` bigint(0) NULL DEFAULT NULL COMMENT '操作原因', \
                `group` bigint(0) NULL DEFAULT NULL COMMENT '组ID', \
                PRIMARY KEY (`id`) USING BTREE, \
                INDEX `idx_guid`(`guid`) USING BTREE, \
                INDEX `idx_guid_t`(`guid`, `t`) USING BTREE, \
                INDEX `idx_lt`(`lt`) USING BTREE, \
                INDEX `idx_guid_t_p`(`guid`, `t`, `p`) USING BTREE, \
                INDEX `idx_lt_time`(`guid`, `lt`, `time`) USING BTREE, \
                INDEX `idx_group`(`group`) USING BTREE \
                ) ENGINE = InnoDB AUTO_INCREMENT = 6 CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci ROW_FORMAT = Dynamic;";
}

template <>
consteval const char* LogServiceSqlPrefix<E_LSLMT_Content>() { return "INSERT INTO log_content_{}(guid, content, t, p, time, lt, group) VALUES"; }

#ifdef LOG_SERVICE_CLIENT
struct stSnowflakeLogGuidTag;
typedef Snowflake<stSnowflakeLogGuidTag, uint64_t, 1692871881000> SnowflakeLogGuid;
#endif

DECLARE_SERVICE_BASE_BEGIN(Log, SessionDistributeMod, ServiceSession);

public :
        static constexpr int64_t scMgrActorCnt = (1 << 6) - 1;

private :
        LogServiceBase() : SuperType("LogService") { }
        ~LogServiceBase() override { }

public :
        bool Init() override;

#ifdef LOG_SERVICE_SERVER

        template <typename ... Args>
        bool StartLocal(int64_t actCnt, Args ... args)
        {
                actCnt = scMgrActorCnt + 1;

                LOG_FATAL_IF(!CHECK_2N(actCnt), "actCnt 必须设置为 2^N !!!", actCnt);
                for (int64_t i=0; i<actCnt; ++i)
                {
                        auto act = std::make_shared<typename SuperType::ActorType>(i);
                        SuperType::_actorArr.emplace_back(act);
                        act->Start();
                }

                return true;
        }

        FORCE_INLINE auto GetActor(uint64_t id)
        { return SuperType::_actorArr[id & scMgrActorCnt].lock(); }
#endif

#ifdef LOG_SERVICE_CLIENT

        template <typename ... Args>
        bool Start(Args ... args)
        {
                if (_logActArr[0].empty())
                        StartLocal(scMgrActorCnt + 1, args...);

                ::nl::net::NetMgrBase<typename SessionType::Tag>::GetInstance()->Connect(args..., [](auto&& s) {
                        return std::make_shared<SessionType>(std::move(s));
                });
                return true;
        }

        template <typename ... Args>
        bool StartLocal(int64_t actCnt, Args ... args)
        {
                LOG_FATAL_IF(!CHECK_2N(actCnt), "actCnt 必须设置为 2^N !!!", actCnt);
                for (int64_t i=0; i<actCnt; ++i)
                {
                        _sesCnt = ServerListCfgMgr::GetInstance()->GetSize<stLogServerInfo>() - 1;
                        _logActArr[i].resize(_sesCnt+1);
                        for (int64_t s=0; s<_sesCnt+1; ++s)
                        {
                                auto act = std::make_shared<typename SuperType::ActorType>(i);
                                act->_sesIdx = s;
                                _logActArr[i][s] = act;
                                act->Start();
                        }
                }

                return true;
        }

        FORCE_INLINE auto GetActor(uint64_t id)
        { return _logActArr[id & scMgrActorCnt][id & _sesCnt].lock(); }

        bool AddSession(const ::nl::net::ISessionPtr& s) override;

        FORCE_INLINE SnowflakeLogGuid::GuidType GenGuid()
        { return SnowflakeLogGuid::Gen(); }

        template <ELogServiceLogMainType LogType, typename ... Args>
        void Log(uint64_t playerGuid, Args ... args)
        {
                auto act = GetActor(playerGuid);
                if (ELogServiceLogMainType_IsValid(LogType) && act)
                {
                        std::lock_guard l(act->_cacheMutex);
                        fmt::format_to(std::back_inserter(const_cast<std::string&>(act->_cache->data_list(LogType))), fmt::runtime(LogServiceFmt<LogType>()), playerGuid, std::forward<Args>(args)...);
                }
        }

        template <>
        void Log<E_LSLMT_Content, const std::string&, uint64_t, uint64_t, time_t, ELogServiceOrigType, uint64_t>(uint64_t playerGuid, const std::string& content, uint64_t t, uint64_t p, time_t time, ELogServiceOrigType ot, uint64_t group)
        {
                auto act = GetActor(playerGuid);
                if (act)
                {
                        std::lock_guard l(act->_cacheMutex);
                        fmt::format_to(std::back_inserter(const_cast<std::string&>(act->_cache->data_list(E_LSLMT_Content)))
                                       , "({}, '{}', {}, {}, {}, {}, {}),"
                                       , playerGuid, content, t, p, time, ot, group);
                }
        }

        uint64_t _sesCnt = 0;
        std::vector<std::weak_ptr<typename SuperType::ActorType>> _logActArr[scMgrActorCnt+1];

#endif

DECLARE_SERVICE_BASE_END(Log);

#ifdef LOG_SERVICE_SERVER
typedef LogServiceBase<E_ServiceType_Server, stLobbyServerInfo> LogService;
#endif

#ifdef LOG_SERVICE_CLIENT
typedef LogServiceBase<E_ServiceType_Client, stLogServerInfo> LogService;
#endif

#ifdef LOG_SERVICE_LOCAL
typedef LogServiceBase<E_ServiceType_Local, stServerInfoBase> LogService;
#endif

// vim: fenc=utf8:expandtab:ts=8
