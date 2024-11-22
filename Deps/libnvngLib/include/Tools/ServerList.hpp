#pragma once

#include <fstream>
#include <rapidjson/document.h>
#include <vector>


template <typename _Ty> inline EServerType CalServerType() { LOG_ERROR(""); return E_ST_None; }

struct stLobbyServerInfo : public stServerInfoBase
{
	uint16_t _gate_port = 0;
	uint16_t _game_port = 0;
};
typedef std::shared_ptr<stLobbyServerInfo> stLobbyServerInfoPtr;
typedef std::weak_ptr<stLobbyServerInfo> stLobbyServerInfoWeakPtr;
template <> inline EServerType CalServerType<stLobbyServerInfo>() { return E_ST_Lobby; }

struct stGateServerInfo : public stServerInfoBase
{
	uint16_t _client_port = 0;
};
typedef std::shared_ptr<stGateServerInfo> stGateServerInfoPtr;
typedef std::weak_ptr<stGateServerInfo> stGateServerInfoWeakPtr;
template <> inline EServerType CalServerType<stGateServerInfo>() { return E_ST_Gate; }

struct stLoginServerInfo : public stServerInfoBase
{
	uint16_t _gate_port = 0;
};
typedef std::shared_ptr<stLoginServerInfo> stLoginServerInfoPtr;
typedef std::weak_ptr<stLoginServerInfo> stLoginServerInfoWeakPtr;
template <> inline EServerType CalServerType<stLoginServerInfo>() { return E_ST_Login; }

struct stPayServerInfo : public stServerInfoBase
{
	uint16_t _lobby_port = 0;
        uint16_t _web_port = 0;
};
typedef std::shared_ptr<stPayServerInfo> stPayServerInfoPtr;
typedef std::weak_ptr<stPayServerInfo> stPayServerInfoWeakPtr;
template <> inline EServerType CalServerType<stPayServerInfo>() { return E_ST_Pay; }

struct stGameServerInfo : public stServerInfoBase
{
	uint16_t _gate_port = 0;
};
typedef std::shared_ptr<stGameServerInfo> stGameServerInfoPtr;
typedef std::weak_ptr<stGameServerInfo> stGameServerInfoWeakPtr;
template <> inline EServerType CalServerType<stGameServerInfo>() { return E_ST_Game; }

struct stGameMgrServerInfo : public stServerInfoBase
{
	uint16_t _lobby_port = 0;
	uint16_t _game_port = 0;
};
typedef std::shared_ptr<stGameMgrServerInfo> stGameMgrServerInfoPtr;
typedef std::weak_ptr<stGameMgrServerInfo> stGameMgrServerInfoWeakPtr;
template <> inline EServerType CalServerType<stGameMgrServerInfo>() { return E_ST_GameMgr; }

struct stDBServerInfo : public stServerInfoBase
{
	uint16_t _lobby_port = 0;
        uint16_t _login_port = 0;
        uint16_t _gen_guid_service_port = 0;
};
typedef std::shared_ptr<stDBServerInfo> stDBServerInfoPtr;
typedef std::weak_ptr<stDBServerInfo> stDBServerInfoWeakPtr;
template <> inline EServerType CalServerType<stDBServerInfo>() { return E_ST_DB; }

struct stGMServerInfo : public stServerInfoBase
{
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
	uint16_t _lobby_port = 0;
};
typedef std::shared_ptr<stLogServerInfo> stLogServerInfoPtr;
template <> inline EServerType CalServerType<stLogServerInfo>() { return E_ST_Log; }

struct stRobotMgrServerInfo : public stServerInfoBase
{
	uint16_t _game_port = 0;
};
typedef std::shared_ptr<stRobotMgrServerInfo> stRobotMgrServerInfoPtr;
template <> inline EServerType CalServerType<stRobotMgrServerInfo>() { return E_ST_RobotMgr; }

struct stPkgStatusServerInfo : public stServerInfoBase
{
        uint16_t _http_port = 0;
};
typedef std::shared_ptr<stPkgStatusServerInfo> stPkgStatusServerInfoPtr;
template <> inline EServerType CalServerType<stPkgStatusServerInfo>() { return E_ST_PkgStatus; }

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
		std::set<int64_t> sidList;
		auto checkSID = [&sidList](uint64_t sid) {
			LOG_FATAL_IF(!sidList.emplace(sid).second, "sid[{}] 重复!!!", sid);
		};

		std::set<std::pair<std::string, uint16_t>> ipPortList;
		auto checkIPPort = [&ipPortList](const stServerInfoBasePtr& info, uint16_t port) {
			LOG_FATAL_IF(!ipPortList.emplace(std::make_pair(info->_ip, port)).second,
				     "ip[{}] port[{}] 重复!!!", info->_ip, port);
			info->_portList.emplace_back(port);
		};

                auto distPort = [this](int64_t sid, int64_t offset) -> int64_t {
                        switch (offset)
                        {
                        case 0 : return _rid * 1000 + sid; break;
                        case 1 : return _rid * 1000 + 20000 + sid; break;
                        case 2 : return _rid * 1000 + 40000 + sid; break;
                        default : LOG_FATAL("监听端口太多!!!"); return -1; break;
                        }
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
				serverInfo->_sid = item["sid"].GetInt64();
				serverInfo->_faName = item["fa_name"].GetString();
				serverInfo->_ip = item["ip"].GetString();
				serverInfo->_gate_port = distPort(serverInfo->_sid, 0);
				serverInfo->_game_port = distPort(serverInfo->_sid, 1);
				serverInfo->_workersCnt = item["workers_cnt"].GetInt64();
				serverInfo->_netProcCnt = item["net_proc_cnt"].GetInt64();

                                if (-1 == serverInfo->_workersCnt)
                                        serverInfo->_workersCnt = std::thread::hardware_concurrency();

				checkIPPort(serverInfo, serverInfo->_gate_port);
				checkIPPort(serverInfo, serverInfo->_game_port);
				checkSID(serverInfo->_sid);
				if (!_serverInfoListByType[E_ST_Lobby].emplace(serverInfo->_sid, serverInfo).second)
					LOG_FATAL("大厅服sid重复!!! sid[{}]", serverInfo->_sid);
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
				serverInfo->_sid = item["sid"].GetInt64();
				serverInfo->_faName = item["fa_name"].GetString();
				serverInfo->_ip = item["ip"].GetString();
				serverInfo->_client_port = item["client_port"].GetInt64();

				serverInfo->_netProcCnt = item["net_proc_cnt"].GetInt64();
				serverInfo->_workersCnt = item["workers_cnt"].GetInt64();

				checkIPPort(serverInfo, serverInfo->_client_port);
				checkSID(serverInfo->_sid);
				if (!_serverInfoListByType[E_ST_Gate].emplace(serverInfo->_sid, serverInfo).second)
					LOG_FATAL("网关服sid重复!!! sid[{}]", serverInfo->_sid);
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
				serverInfo->_sid = item["sid"].GetInt64();
				serverInfo->_faName = item["fa_name"].GetString();
				serverInfo->_ip = item["ip"].GetString();
				serverInfo->_gate_port = distPort(serverInfo->_sid, 0);

				serverInfo->_netProcCnt = item["net_proc_cnt"].GetInt64();
				serverInfo->_workersCnt = item["workers_cnt"].GetInt64();
				serverInfo->_actorCntPerWorkers = item["actor_cnt_per_workers"].GetInt64();

				checkIPPort(serverInfo, serverInfo->_gate_port);
				checkSID(serverInfo->_sid);
				if (!_serverInfoListByType[E_ST_Login].emplace(serverInfo->_sid, serverInfo).second)
					LOG_FATAL("登录服sid重复!!! sid[{}]", serverInfo->_sid);
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
				serverInfo->_sid = item["sid"].GetInt64();
				serverInfo->_faName = item["fa_name"].GetString();
				serverInfo->_ip = item["ip"].GetString();
				serverInfo->_lobby_port = distPort(serverInfo->_sid, 0);
				serverInfo->_web_port = item["http_port"].GetInt64();

				serverInfo->_netProcCnt = item["net_proc_cnt"].GetInt64();
				serverInfo->_workersCnt = item["workers_cnt"].GetInt64();
				serverInfo->_actorCntPerWorkers = item["actor_cnt_per_workers"].GetInt64();

				checkIPPort(serverInfo, serverInfo->_lobby_port);
				checkSID(serverInfo->_sid);
				if (!_serverInfoListByType[E_ST_Pay].emplace(serverInfo->_sid, serverInfo).second)
					LOG_FATAL("登录服sid重复!!! sid[{}]", serverInfo->_sid);
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
				serverInfo->_sid = item["sid"].GetInt64();
				serverInfo->_faName = item["fa_name"].GetString();
				serverInfo->_ip = item["ip"].GetString();
				serverInfo->_gate_port = distPort(serverInfo->_sid, 0);
				serverInfo->_workersCnt = item["workers_cnt"].GetInt64();
				serverInfo->_netProcCnt = item["net_proc_cnt"].GetInt64();

                                if (-1 == serverInfo->_workersCnt)
                                        serverInfo->_workersCnt = std::thread::hardware_concurrency();

				checkIPPort(serverInfo, serverInfo->_gate_port);
				checkSID(serverInfo->_sid);
				if (!_serverInfoListByType[E_ST_Game].emplace(serverInfo->_sid, serverInfo).second)
					LOG_FATAL("房间服sid重复!!! sid[{}]", serverInfo->_sid);
			}
		}

		if (root.HasMember("game_mgr_server"))
		{
			auto& item = root["game_mgr_server"];
			auto serverInfo = std::make_shared<stGameMgrServerInfo>();
			serverInfo->_sid = item["sid"].GetInt64();
			serverInfo->_faName = item["fa_name"].GetString();
			serverInfo->_ip = item["ip"].GetString();
			serverInfo->_game_port = distPort(serverInfo->_sid, 0);
			serverInfo->_lobby_port = distPort(serverInfo->_sid, 1);
			serverInfo->_workersCnt = item["workers_cnt"].GetInt64();
			serverInfo->_netProcCnt = item["net_proc_cnt"].GetInt64();
                        serverInfo->_actorCntPerWorkers = item["actor_cnt_per_workers"].GetInt64();

                        if (-1 == serverInfo->_workersCnt)
                                serverInfo->_workersCnt = std::thread::hardware_concurrency();

			checkIPPort(serverInfo, serverInfo->_game_port);
			checkIPPort(serverInfo, serverInfo->_lobby_port);
			checkSID(serverInfo->_sid);
			if (!_serverInfoListByType[E_ST_GameMgr].emplace(serverInfo->_sid, serverInfo).second)
				LOG_FATAL("房间服管理服sid重复!!! sid[{}]", serverInfo->_sid);
		}

		if (root.HasMember("db_server_list"))
		{
			auto& serverArr = root["db_server_list"];
			LOG_FATAL_IF(!CHECK_2N(serverArr.Size()), "");
			for (int32_t i=0; i<static_cast<int32_t>(serverArr.Size()); ++i)
			{
				auto& item = serverArr[i];
				auto serverInfo = std::make_shared<stDBServerInfo>();
				serverInfo->_sid = item["sid"].GetInt64();
				serverInfo->_faName = item["fa_name"].GetString();
				serverInfo->_ip = item["ip"].GetString();
				serverInfo->_lobby_port = distPort(serverInfo->_sid, 0);
				serverInfo->_login_port = distPort(serverInfo->_sid, 1);
				serverInfo->_gen_guid_service_port = item["gen_guid_service_port"].GetInt64();
				serverInfo->_workersCnt = item["workers_cnt"].GetInt64();
				serverInfo->_netProcCnt = item["net_proc_cnt"].GetInt64();

                                if (-1 == serverInfo->_workersCnt)
                                        serverInfo->_workersCnt = std::thread::hardware_concurrency();

				checkIPPort(serverInfo, serverInfo->_lobby_port);
				checkIPPort(serverInfo, serverInfo->_login_port);
				checkIPPort(serverInfo, serverInfo->_gen_guid_service_port);
				checkSID(serverInfo->_sid);
				if (!_serverInfoListByType[E_ST_DB].emplace(serverInfo->_sid, serverInfo).second)
					LOG_FATAL("DBServer sid重复!!! sid[{}]", serverInfo->_sid);
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
				serverInfo->_sid = item["sid"].GetInt64();
				serverInfo->_faName = item["fa_name"].GetString();
				serverInfo->_ip = item["ip"].GetString();
                                serverInfo->_lobby_port = item["lobby_port"].GetInt64();
				serverInfo->_workersCnt = item["workers_cnt"].GetInt64();

                                if (-1 == serverInfo->_workersCnt)
                                        serverInfo->_workersCnt = std::thread::hardware_concurrency();

				checkIPPort(serverInfo, serverInfo->_lobby_port);
				checkSID(serverInfo->_sid);
				if (!_serverInfoListByType[E_ST_Log].emplace(serverInfo->_sid, serverInfo).second)
					LOG_FATAL("LogServer sid重复!!! sid[{}]", serverInfo->_sid);
			}
		}

                if (root.HasMember("gm_server"))
		{
			auto& item = root["gm_server"];
			auto serverInfo = std::make_shared<stGMServerInfo>();
			serverInfo->_sid = item["sid"].GetInt64();
			serverInfo->_faName = item["fa_name"].GetString();
			serverInfo->_ip = item["ip"].GetString();
			serverInfo->_lobby_port = distPort(serverInfo->_sid, 0);
			serverInfo->_http_gm_port = item["http_gm_port"].GetInt64();
			serverInfo->_http_activity_port = item["http_activity_port"].GetInt64();
			serverInfo->_cdkey_port = item["cdkey_port"].GetInt64();
			serverInfo->_gm_port = item["gm_port"].GetInt64();
			serverInfo->_announcement_port = item["announcement_port"].GetInt64();

			checkIPPort(serverInfo, serverInfo->_lobby_port);
			checkIPPort(serverInfo, serverInfo->_http_gm_port);
			checkIPPort(serverInfo, serverInfo->_http_activity_port);
			checkIPPort(serverInfo, serverInfo->_cdkey_port);
			checkIPPort(serverInfo, serverInfo->_gm_port);
			checkIPPort(serverInfo, serverInfo->_announcement_port);
			checkSID(serverInfo->_sid);

			if (!_serverInfoListByType[E_ST_GM].emplace(serverInfo->_sid, serverInfo).second)
				LOG_FATAL("GMServer sid重复!!! sid[{}]", serverInfo->_sid);
		}

                if (root.HasMember("robot_mgr_server"))
                {
                        auto& item = root["robot_mgr_server"];
                        auto serverInfo = std::make_shared<stRobotMgrServerInfo>();
                        serverInfo->_sid = item["sid"].GetInt64();
                        serverInfo->_faName = item["fa_name"].GetString();
                        serverInfo->_ip = item["ip"].GetString();
                        serverInfo->_game_port = distPort(serverInfo->_sid, 0);

			checkIPPort(serverInfo, serverInfo->_game_port);
			checkSID(serverInfo->_sid);

			if (!_serverInfoListByType[E_ST_RobotMgr].emplace(serverInfo->_sid, serverInfo).second)
				LOG_FATAL("RobotMgrServer sid重复!!! sid[{}]", serverInfo->_sid);
                }

                if (root.HasMember("pkg_status_server"))
                {
                        auto& item = root["pkg_status_server"];
                        auto serverInfo = std::make_shared<stPkgStatusServerInfo>();
                        serverInfo->_sid = item["sid"].GetInt64();
                        serverInfo->_faName = item["fa_name"].GetString();
                        serverInfo->_ip = item["ip"].GetString();
                        serverInfo->_http_port = item["http_port"].GetInt64();

                        checkIPPort(serverInfo, serverInfo->_http_port);
                        checkSID(serverInfo->_sid);

                        if (!_serverInfoListByType[E_ST_PkgStatus].emplace(serverInfo->_sid, serverInfo).second)
                                LOG_FATAL("PkgStatusServer sid重复!!! sid[{}]", serverInfo->_sid);
                }

		return true;
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
        std::vector<stDBGenGuidItemPtr> _genGuidItemList;
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
                                        _dbCfg->_genGuidItemList.emplace_back(info);
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

                auto readMysqlCfgFunc = [](stMySqlConfig& cfg, auto& db, std::string_view dbName) {
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

		if (root.HasMember("mysql"))
                {
                        auto& mysqlCfg = root["mysql"];
                        if (mysqlCfg.HasMember("game_db"))
                                readMysqlCfgFunc(_mysqlCfg, mysqlCfg["game_db"], "game_db");

                        if (mysqlCfg.HasMember("game_log"))
                                readMysqlCfgFunc(_mysqlLogCfg, mysqlCfg["game_log"], "game_log");

                        if (mysqlCfg.HasMember("game_misc"))
                                readMysqlCfgFunc(_mysqlMiscCfg, mysqlCfg["game_misc"], "game_misc");
                }

		if (_baseCfgDir.empty())
			_baseCfgDir = root["cfg_dir"].GetString();

		return true;
	}

	std::string GetConfigAbsolute(const std::string& path)
	{
		std::string ret = _baseCfgDir;
		ret += "/";
		ret += path;
		return ret;
	}

public :
	stRedisCfg _redisCfg;

	stMySqlConfig _mysqlCfg;
	stMySqlConfig _mysqlLogCfg;
	stMySqlConfig _mysqlMiscCfg;

	stGateServerCfgPtr _gateCfg;
        stDBServerCfgPtr _dbCfg;
	std::string _baseCfgDir;
};

// vim: fenc=utf8:expandtab:ts=8:noma
