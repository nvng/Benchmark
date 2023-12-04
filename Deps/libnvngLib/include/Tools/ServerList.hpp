#pragma once

#include <fstream>
#include <rapidjson/document.h>
#include <vector>

#define PARSE_PORT_FROM_JSON(p, m) \
        p = item[m].GetInt64(); \
        serverInfo->_portList.emplace_back(p)

#define DIST_PORT_IMPL(p) \
        p = start + (idx++); \
        _portList.emplace_back(p)

template <typename _Ty> inline EServerType CalServerType() { LOG_ERROR(""); return E_ST_None; }

struct stLobbyServerInfo : public stServerInfoBase
{
        stLobbyServerInfo() : stServerInfoBase(E_ST_Lobby) { }
        void DistPort(int64_t start, int64_t& idx) override
        {
                DIST_PORT_IMPL(_gate_port);
                DIST_PORT_IMPL(_game_port);
        }

        uint16_t _gate_port = 0;
        uint16_t _game_port = 0;
};
typedef std::shared_ptr<stLobbyServerInfo> stLobbyServerInfoPtr;
typedef std::weak_ptr<stLobbyServerInfo> stLobbyServerInfoWeakPtr;
template <> inline EServerType CalServerType<stLobbyServerInfo>() { return E_ST_Lobby; }

struct stGateServerInfo : public stServerInfoBase
{
        stGateServerInfo() : stServerInfoBase(E_ST_Gate) { }
        void DistPort(int64_t start, int64_t& idx) override
        {
                // DIST_PORT_IMPL(_client_port);
        }

        uint16_t _client_port = 0;
};
typedef std::shared_ptr<stGateServerInfo> stGateServerInfoPtr;
typedef std::weak_ptr<stGateServerInfo> stGateServerInfoWeakPtr;
template <> inline EServerType CalServerType<stGateServerInfo>() { return E_ST_Gate; }

struct stLoginServerInfo : public stServerInfoBase
{
        stLoginServerInfo() : stServerInfoBase(E_ST_Login) { }
        void DistPort(int64_t start, int64_t& idx) override
        {
                DIST_PORT_IMPL(_gate_port);
        }
        uint16_t _gate_port = 0;
};
typedef std::shared_ptr<stLoginServerInfo> stLoginServerInfoPtr;
typedef std::weak_ptr<stLoginServerInfo> stLoginServerInfoWeakPtr;
template <> inline EServerType CalServerType<stLoginServerInfo>() { return E_ST_Login; }

struct stPayServerInfo : public stServerInfoBase
{
        stPayServerInfo() : stServerInfoBase(E_ST_Pay) { }
        void DistPort(int64_t start, int64_t& idx) override
        {
                DIST_PORT_IMPL(_lobby_port);
                // DIST_PORT_IMPL(_web_port);
        }
        uint16_t _lobby_port = 0;
        uint16_t _web_port = 0;
};
typedef std::shared_ptr<stPayServerInfo> stPayServerInfoPtr;
typedef std::weak_ptr<stPayServerInfo> stPayServerInfoWeakPtr;
template <> inline EServerType CalServerType<stPayServerInfo>() { return E_ST_Pay; }

struct stGameServerInfo : public stServerInfoBase
{
        stGameServerInfo() : stServerInfoBase(E_ST_Game) { }
        void DistPort(int64_t start, int64_t& idx) override
        {
                DIST_PORT_IMPL(_gate_port);
        }
        uint16_t _gate_port = 0;
};
typedef std::shared_ptr<stGameServerInfo> stGameServerInfoPtr;
typedef std::weak_ptr<stGameServerInfo> stGameServerInfoWeakPtr;
template <> inline EServerType CalServerType<stGameServerInfo>() { return E_ST_Game; }

struct stGameMgrServerInfo : public stServerInfoBase
{
        stGameMgrServerInfo() : stServerInfoBase(E_ST_GameMgr) { }
        void DistPort(int64_t start, int64_t& idx) override
        {
                DIST_PORT_IMPL(_lobby_port);
                DIST_PORT_IMPL(_game_port);
        }
        uint16_t _lobby_port = 0;
        uint16_t _game_port = 0;
};
typedef std::shared_ptr<stGameMgrServerInfo> stGameMgrServerInfoPtr;
typedef std::weak_ptr<stGameMgrServerInfo> stGameMgrServerInfoWeakPtr;
template <> inline EServerType CalServerType<stGameMgrServerInfo>() { return E_ST_GameMgr; }

struct stDBServerInfo : public stServerInfoBase
{
        stDBServerInfo() : stServerInfoBase(E_ST_DB) { }
        void DistPort(int64_t start, int64_t& idx) override
        {
                DIST_PORT_IMPL(_lobby_port);
                DIST_PORT_IMPL(_login_port);
                DIST_PORT_IMPL(_gen_guid_service_port);
        }
        uint16_t _lobby_port = 0;
        uint16_t _login_port = 0;
        uint16_t _gen_guid_service_port = 0;
};
typedef std::shared_ptr<stDBServerInfo> stDBServerInfoPtr;
typedef std::weak_ptr<stDBServerInfo> stDBServerInfoWeakPtr;
template <> inline EServerType CalServerType<stDBServerInfo>() { return E_ST_DB; }

struct stGMServerInfo : public stServerInfoBase
{
        stGMServerInfo() : stServerInfoBase(E_ST_GM) { }
        void DistPort(int64_t start, int64_t& idx) override
        {
                DIST_PORT_IMPL(_lobby_port);
                // DIST_PORT_IMPL(_http_gm_port);
                // DIST_PORT_IMPL(_http_activity_port);
                DIST_PORT_IMPL(_cdkey_port);
                DIST_PORT_IMPL(_gm_port);
                DIST_PORT_IMPL(_announcement_port);
        }
        uint16_t _lobby_port = 0;
        uint16_t _http_gm_port = 0;
        uint16_t _http_activity_port = 0;
        uint16_t _cdkey_port = 0;
        uint16_t _gm_port = 0;
        uint16_t _announcement_port = 0;
};
typedef std::shared_ptr<stGMServerInfo> stGMServerInfoPtr;
template <> inline EServerType CalServerType<stGMServerInfo>() { return E_ST_GM; }

struct stLogServerInfo : public stServerInfoBase
{
        stLogServerInfo() : stServerInfoBase(E_ST_Log) { }
        void DistPort(int64_t start, int64_t& idx) override
        {
                DIST_PORT_IMPL(_lobby_port);
        }
        uint16_t _lobby_port = 0;
};
typedef std::shared_ptr<stLogServerInfo> stLogServerInfoPtr;
template <> inline EServerType CalServerType<stLogServerInfo>() { return E_ST_Log; }

struct stRobotMgrServerInfo : public stServerInfoBase
{
        stRobotMgrServerInfo() : stServerInfoBase(E_ST_RobotMgr) { }
        void DistPort(int64_t start, int64_t& idx) override
        {
                DIST_PORT_IMPL(_game_port);
        }
        uint16_t _game_port = 0;
};
typedef std::shared_ptr<stRobotMgrServerInfo> stRobotMgrServerInfoPtr;
template <> inline EServerType CalServerType<stRobotMgrServerInfo>() { return E_ST_RobotMgr; }

class ServerListCfgMgr : public Singleton<ServerListCfgMgr>
{
public :
        bool Init(std::string cfgFile)
        {
                std::ifstream ifs;
                ifs.open(cfgFile);
                if (!ifs.is_open())
                        return false;

                ifs.seekg(0, std::ios::end);
                int64_t length = ifs.tellg();
                ifs.seekg(0, std::ios::beg);
                char* buffer = new char[length];
                bzero(buffer, length);
                ifs.read(buffer, length);
                ifs.close();

                rapidjson::Document root;
                if (root.Parse(buffer, length).HasParseError())
                        return false;
                delete[] buffer;

                _rid = root["rid"].GetInt64();
                _startPort = root["start_port"].GetInt64();
                _endPort = root["end_port"].GetInt64();
                std::set<int64_t> sidList;
                auto checkSID = [&sidList](uint64_t sid) {
                        LOG_FATAL_IF(!sidList.emplace(sid).second, "sid[{}] 重复!!!", sid);
                };

                auto parseBaseFunc = [this, &checkSID](const stServerInfoBasePtr& serverInfo, const auto& item) {
                        assert(EServerType_IsValid(serverInfo->_st));

                        serverInfo->_sid = item["sid"].GetInt64();
                        checkSID(serverInfo->_sid);
                        serverInfo->_faName = item["fa_name"].GetString();
                        serverInfo->_ip = item["ip"].GetString();

                        if (item.HasMember("workers_cnt"))
                                serverInfo->_workersCnt = item["workers_cnt"].GetInt64();
                        if (-1 == serverInfo->_workersCnt)
                                serverInfo->_workersCnt = std::thread::hardware_concurrency();

                        if (item.HasMember("actor_cnt_per_workers"))
                                serverInfo->_actorCntPerWorkers = item["actor_cnt_per_workers"].GetInt64();

                        if (item.HasMember("net_proc_cnt"))
                                serverInfo->_netProcCnt = item["net_proc_cnt"].GetInt64();

                        if (!_serverInfoListByType[serverInfo->_st].emplace(serverInfo->_sid, serverInfo).second)
                                LOG_FATAL("st[{}] sid重复!!! sid[{}]", serverInfo->_st, serverInfo->_sid);

                        int64_t idx = -1;
                        for (auto& val : _serverInfoListByType[serverInfo->_st])
                                val.second->_idx = ++idx;
                };

                /*
                   auto& controlServer = root["control_server"];
                   SuperServer_.sid_ = controlServer["sid"].GetInt64();
                   SuperServer_.addr_.ip_ = controlServer["ip"].GetString();
                   SuperServer_.addr_.port_ = controlServer["port"].GetInt64();
                   checkSID(SuperType_.sid_);
                   */

                if (root.HasMember("lobby_server_list"))
                {
                        auto& serverArr = root["lobby_server_list"];
                        LOG_FATAL_IF(!CHECK_2N(serverArr.Size()), "");
                        for (int32_t i=0; i<static_cast<int32_t>(serverArr.Size()); ++i)
                        {
                                auto& item = serverArr[i];
                                auto serverInfo = std::make_shared<stLobbyServerInfo>();
                                parseBaseFunc(serverInfo, item);
                        }
                }

                if (root.HasMember("gate_server_list"))
                {
                        auto& serverArr = root["gate_server_list"];
                        LOG_FATAL_IF(!CHECK_2N(serverArr.Size()), "");
                        for (int32_t i=0; i<static_cast<int32_t>(serverArr.Size()); ++i)
                        {
                                auto& item = serverArr[i];
                                auto serverInfo = std::make_shared<stGateServerInfo>();
                                parseBaseFunc(serverInfo, item);
                                PARSE_PORT_FROM_JSON(serverInfo->_client_port, "client_port");
                        }
                }

                if (root.HasMember("login_server_list"))
                {
                        auto& serverArr = root["login_server_list"];
                        LOG_FATAL_IF(!CHECK_2N(serverArr.Size()), "");
                        for (int32_t i=0; i<static_cast<int32_t>(serverArr.Size()); ++i)
                        {
                                auto& item = serverArr[i];
                                auto serverInfo = std::make_shared<stLoginServerInfo>();
                                parseBaseFunc(serverInfo, item);
                        }
                }

                if (root.HasMember("pay_server_list"))
                {
                        auto& serverArr = root["pay_server_list"];
                        LOG_FATAL_IF(!CHECK_2N(serverArr.Size()), "");
                        for (int32_t i=0; i<static_cast<int32_t>(serverArr.Size()); ++i)
                        {
                                auto& item = serverArr[i];

                                auto serverInfo = std::make_shared<stPayServerInfo>();
                                parseBaseFunc(serverInfo, item);
                                PARSE_PORT_FROM_JSON(serverInfo->_web_port, "http_port");
                        }
                }

                if (root.HasMember("game_server_list"))
                {
                        auto& serverArr = root["game_server_list"];
                        LOG_FATAL_IF(!CHECK_2N(serverArr.Size()), "");
                        for (int32_t i=0; i<static_cast<int32_t>(serverArr.Size()); ++i)
                        {
                                auto& item = serverArr[i];
                                auto serverInfo = std::make_shared<stGameServerInfo>();
                                parseBaseFunc(serverInfo, item);
                        }
                }

                if (root.HasMember("game_mgr_server"))
                {
                        auto& item = root["game_mgr_server"];
                        auto serverInfo = std::make_shared<stGameMgrServerInfo>();
                        parseBaseFunc(serverInfo, item);
                }

                if (root.HasMember("db_server_list"))
                {
                        auto& serverArr = root["db_server_list"];
                        LOG_FATAL_IF(!CHECK_2N(serverArr.Size()), "");
                        for (int32_t i=0; i<static_cast<int32_t>(serverArr.Size()); ++i)
                        {
                                auto& item = serverArr[i];
                                auto serverInfo = std::make_shared<stDBServerInfo>();
                                parseBaseFunc(serverInfo, item);
                        }
                }

                if (root.HasMember("log_server_list"))
                {
                        auto& serverArr = root["log_server_list"];
                        LOG_FATAL_IF(!CHECK_2N(serverArr.Size()), "");
                        for (int32_t i=0; i<static_cast<int32_t>(serverArr.Size()); ++i)
                        {
                                auto& item = serverArr[i];
                                auto serverInfo = std::make_shared<stLogServerInfo>();
                                parseBaseFunc(serverInfo, item);
                        }
                }

                if (root.HasMember("gm_server"))
                {
                        auto& item = root["gm_server"];
                        auto serverInfo = std::make_shared<stGMServerInfo>();
                        parseBaseFunc(serverInfo, item);
                        PARSE_PORT_FROM_JSON(serverInfo->_http_gm_port, "http_gm_port");
                        PARSE_PORT_FROM_JSON(serverInfo->_http_activity_port, "http_activity_port");
                }

                if (root.HasMember("robot_mgr_server"))
                {
                        auto& item = root["robot_mgr_server"];
                        auto serverInfo = std::make_shared<stRobotMgrServerInfo>();
                        parseBaseFunc(serverInfo, item);
                }

                std::map<std::string, std::map<EServerType, std::map<int64_t, stServerInfoBasePtr>>> serverInfoList;
                for (auto& val : _serverInfoListByType)
                {
                        const EServerType st = val.first;
                        for (auto& innVal : val.second)
                        {
                                auto sid = innVal.first;
                                auto info = innVal.second;

                                auto& ipList = serverInfoList[info->_ip];
                                auto& stList = ipList[st];
                                stList[sid] = info;
                        }
                }

                for (auto& val : serverInfoList)
                {
                        // IP不同，从头开始分配。
                        int64_t idx = 0;
                        for (auto& val_1 : val.second)
                        {
                                for (auto& val_2 : val_1.second)
                                {
                                        val_2.second->DistPort(_startPort, idx);
                                        assert(_startPort+idx-1 <= _endPort);
                                        LOG_FATAL_IF(_startPort+idx-1 > _endPort, "");
                                }
                        }
                }

                std::set<std::pair<std::string, uint16_t>> ipPortList;
                for (auto& val : _serverInfoListByType)
                {
                        for (auto& val_1 : val.second)
                        {
                                auto info = val_1.second;
                                for (auto port : info->_portList)
                                {
                                        // LOG_INFO("sid[{}] st[{}] ip[{}] port[{}]", info->_sid, val.first, info->_ip, port);
                                        LOG_FATAL_IF(0 == port || !ipPortList.emplace(std::make_pair(info->_ip, port)).second,
                                                     "rid[{}] sid[{}] ip[{}] port[{}] 重复!!!", _rid, info->_sid, info->_ip, port);
                                }
                        }
                }

                return true;
        }

        void PrintPortDistInfo()
        {
                std::map<std::pair<std::string, int64_t>, stServerInfoBasePtr> tmpList;
                for (auto& val : ServerListCfgMgr::GetInstance()->_serverInfoListByType)
                {
                        for (auto& val_1 : val.second)
                        {
                                auto info = val_1.second;
                                for (auto port : info->_portList)
                                {
                                        auto key = std::make_pair(info->_ip, port);
                                        tmpList.emplace(key, info);
                                }
                        }
                }

                for (auto& val : tmpList)
                {
                        auto info = val.second;
                        LOG_INFO("rid[{}] st[{}] sid[{}] ip[{}] port[{}]"
                                 , _rid, info->_st, info->_sid, info->_ip, val.first.second);
                }
        }

        void ForeachInternal(EServerType type, const auto& cb)
        {
                for (auto& val : _serverInfoListByType[type])
                        cb(val.second);
        }

        template <typename _Ty>
                void Foreach(const auto& cb)
                {
                        for (auto& val : _serverInfoListByType[CalServerType<_Ty>()])
                                cb(std::dynamic_pointer_cast<_Ty>(val.second));
                }

        template <typename _Ty>
                FORCE_INLINE int64_t GetSize() { return _serverInfoListByType[CalServerType<_Ty>()].size(); }

        template <typename _Ty>
                FORCE_INLINE std::shared_ptr<_Ty> GetFirst()
                {
                        auto& l = _serverInfoListByType[CalServerType<_Ty>()];
                        return !l.empty() ? std::dynamic_pointer_cast<_Ty>(l.begin()->second) : std::shared_ptr<_Ty>();
                }

        template <typename _Ty>
                int64_t CalSidIdx(int64_t sid)
                {
                        int64_t i = -1;
                        for (auto& val : _serverInfoListByType[CalServerType<_Ty>()])
                        {
                                ++i;
                                if (sid == val.second->_sid)
                                        return i;
                        }
                        return INVALID_SERVER_IDX;
                }

public :
        int64_t _rid = -1;
        int64_t _startPort = -1;
        int64_t _endPort = -1;
        std::map<EServerType, std::map<int64_t, stServerInfoBasePtr>> _serverInfoListByType;
};

struct stRedisCfg
{
        std::string _ip;
        uint16_t    _port = 0;
        std::string _pwd;
        std::string _dbIdx;
        int32_t     _connCnt = 2;
        int32_t     _thCnt = 1;
};

struct stMySqlConfig
{
        std::string _host;
        std::string _user;
        std::string _pwd;
        std::string _charset = "utf8";
        std::string _dbName;
        uint16_t    _port = 3306;
        int32_t     _connCnt = 2;
        int32_t     _thCnt = 1;
};

struct stGateServerCfg
{
        std::string _crt;
        std::string _key;
};
typedef std::shared_ptr<stGateServerCfg> stGateServerCfgPtr;

struct stDBGenGuidItem
{
        int64_t _idx = 0;
        uint64_t _minGuid = 0;
        uint64_t _maxGuid = 0;
};
typedef std::shared_ptr<stDBGenGuidItem> stDBGenGuidItemPtr;

struct stDBServerCfg
{
        std::map<int64_t, stDBGenGuidItemPtr> _genGuidItemList;
};
typedef std::shared_ptr<stDBServerCfg> stDBServerCfgPtr;

class ServerCfgMgr : public Singleton<ServerCfgMgr>
{
public :
        bool Init(std::string cfgFile)
        {
                LOG_FATAL_IF(-1 == ServerListCfgMgr::GetInstance()->_rid, "");

                std::ifstream ifs;
                ifs.open(cfgFile);
                if (!ifs.is_open())
                        return false;

                ifs.seekg(0, std::ios::end);
                int64_t length = ifs.tellg();
                ifs.seekg(0, std::ios::beg);
                char* buffer = new char[length];
                bzero(buffer, length);
                ifs.read(buffer, length);
                ifs.close();

                rapidjson::Document root;
                if (root.Parse(buffer, length).HasParseError())
                        return false;
                delete[] buffer;

                if (root.HasMember("gate_server"))
                {
                        auto& gateCfg = root["gate_server"];
                        _gateCfg = std::make_shared<stGateServerCfg>();
                        _gateCfg->_crt = gateCfg["crt"].GetString();
                        _gateCfg->_key = gateCfg["key"].GetString();
                }

                if (root.HasMember("db_server"))
                {
                        auto& dbCfg = root["db_server"];
                        _dbCfg = std::make_shared<stDBServerCfg>();
                        if (dbCfg.HasMember("gen_guid") && dbCfg["gen_guid"].IsArray())
                        {
                                auto& arr = dbCfg["gen_guid"];
                                for (int64_t i=0; i<arr.Size(); ++i)
                                {
                                        auto& item = arr[i];
                                        auto info = std::make_shared<stDBGenGuidItem>();
                                        info->_idx = item["idx"].GetInt64();
                                        info->_minGuid = item["min_guid"].GetUint64();
                                        info->_maxGuid = item["max_guid"].GetUint64();
                                        LOG_FATAL_IF(!_dbCfg->_genGuidItemList.emplace(info->_idx, info).second, "gen_guid idx[{}] 重复!!!", info->_idx);
                                }
                        }
                }

                if (root.HasMember("redis"))
                {
                        auto& redisCfg = root["redis"];
                        if (redisCfg.HasMember("game_db"))
                        {
                                auto& gameDB = redisCfg["game_db"];
                                _redisCfg._ip = gameDB["ip"].GetString();
                                _redisCfg._port = gameDB["port"].GetInt64();
                                _redisCfg._pwd = gameDB["pwd"].GetString();
                                _redisCfg._dbIdx = gameDB["db"].GetString();
                                _redisCfg._connCnt = gameDB["conn_cnt"].GetInt64();
                                _redisCfg._thCnt = gameDB["thread_cnt"].GetInt64();
                                if ("-1" == _redisCfg._dbIdx)
                                        _redisCfg._dbIdx = fmt::format_int(ServerListCfgMgr::GetInstance()->_rid).str();
                        }
                }

                if (root.HasMember("mysql"))
                {
                        auto& mysqlCfg = root["mysql"];
                        auto readMysqlCfgFunc = [&mysqlCfg](stMySqlConfig& cfg, const char* dbName) mutable {
                                auto& db = mysqlCfg[dbName];
                                cfg._host = db["host"].GetString();
                                cfg._port = db["port"].GetInt();
                                cfg._user = db["user"].GetString();
                                cfg._pwd = db["pwd"].GetString();
                                cfg._connCnt = db["conn_cnt"].GetInt64();
                                cfg._thCnt = db["thread_cnt"].GetInt64();
                                cfg._dbName = db["db"].GetString();
                                if ("-1" == cfg._dbName)
                                        cfg._dbName = fmt::format("{}_{}", dbName, ServerListCfgMgr::GetInstance()->_rid);
                        };

                        if (mysqlCfg.HasMember("game_db"))
                                readMysqlCfgFunc(_mysqlCfg, "game_db");

                        if (mysqlCfg.HasMember("game_log"))
                                readMysqlCfgFunc(_mysqlLogCfg, "game_log");
                }

                return true;
        }

        FORCE_INLINE std::string GetConfigAbsolute(const std::string& path)
        { return _baseCfgDir + "/config_ch/" + path; }

public :
        stRedisCfg _redisCfg;

        stMySqlConfig _mysqlCfg;
        stMySqlConfig _mysqlLogCfg;

        stGateServerCfgPtr _gateCfg;
        stDBServerCfgPtr _dbCfg;
        std::string _baseCfgDir;
};

// vim: fenc=utf8:expandtab:ts=8:noma
