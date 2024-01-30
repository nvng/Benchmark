#pragma once

#include "DB/DBMgrBase.hpp"

namespace nl::db
{

// {{{ SqlUpdateBatch

template <typename _Fy, typename _Ky, typename... Args>
class SqlUpdateBatch
{
public :
        template <typename T, typename ... InnerArgs>
        FORCE_INLINE void PushInernal(_Ky k, T v, InnerArgs ... args)
        {
                fmt::format_to(std::back_inserter(_bufArr[sizeof...(InnerArgs)]), sFmtArr[sizeof...(InnerArgs)].c_str(), k, v);
                PushInernal(k, args...);
        }

        template <typename T>
        FORCE_INLINE void PushInernal(_Ky k, T v)
        {
                fmt::format_to(std::back_inserter(_bufArr[0]), sFmtArr[0].c_str(), k, v);
        }

        FORCE_INLINE void PushValue(_Ky key, Args ... args)
        {
                if (_bufArr[0].empty())
                {
                        _bufArr[0] = sSqlHead;
                        for (int64_t i=static_cast<int64_t>(sizeof...(Args))-1; i>=0; --i)
                                _bufArr[sizeof...(Args)-1-i].reserve(sInitSizeList[i]);
                        _bufArr[sizeof...(Args)].reserve(sInitSizeList[sizeof...(Args)]);

                        for (uint64_t i=0; i<sizeof...(Args); ++i)
                                fmt::format_to(std::back_inserter(_bufArr[i]), " {}=CASE {}", sUpdateColNameList[sizeof...(Args)-1-i], sKeyColName);
                        fmt::format_to(std::back_inserter(_bufArr[sizeof...(Args)]), " WHERE {} IN(", sKeyColName);
                }
                PushInernal(key, args...);

                if constexpr (std::is_same<std::string_view, _Ky>::value)
                        fmt::format_to(std::back_inserter(_bufArr[sizeof...(Args)]), "'{}',", key);
                else
                        fmt::format_to(std::back_inserter(_bufArr[sizeof...(Args)]), "{},", key);
        }

        FORCE_INLINE std::string PackSql()
        {
                if (_bufArr[0].empty())
                        return std::string();

                std::string& ret = _bufArr[0];
                ret += 1 == sizeof...(Args) ? " END" : " END,";

                auto s = ret.size();
                for (uint64_t i=1; i<sizeof...(Args)+1; ++i)
                        s += _bufArr[i].size() + 10;
                if (ret.capacity() < s)
                        ret.reserve(s);

                for (uint64_t i=1; i<sizeof...(Args); ++i)
                {
                        _bufArr[i] += (sizeof...(Args) - 1 == i) ? " END" : " END,";
                        ret += std::move(_bufArr[i]);
                }
                ret += std::move(_bufArr[sizeof...(Args)]);

                ret.pop_back();
                ret += ");";
                return std::move(ret);
        }

private :
        template <typename T, typename ... InnerArgs>
        struct stFmtParse
        {
                FORCE_INLINE static void Parse(std::string* fmtArr)
                {
                        if constexpr (std::is_same<std::string_view, _Ky>::value)
                        {
                                if constexpr (std::is_same<std::string_view, T>::value)
                                        fmtArr[sizeof...(InnerArgs)] = " WHEN '{}' THEN '{}'";
                                else
                                        fmtArr[sizeof...(InnerArgs)] = " WHEN '{}' THEN {}";
                        }
                        else
                        {
                                if constexpr (std::is_same<std::string_view, T>::value)
                                        fmtArr[sizeof...(InnerArgs)] = " WHEN {} THEN '{}'";
                                else
                                        fmtArr[sizeof...(InnerArgs)] = " WHEN {} THEN {}";
                        }
                        if constexpr (0 != sizeof...(InnerArgs))
                                stFmtParse<InnerArgs...>::Parse(fmtArr);
                }
        };

public :
        FORCE_INLINE static bool Init(const std::string& sqlHead,
                                const std::string& keyColName,
                                const std::vector<std::string>& updateColNameList,
                                const std::vector<int64_t>& initSizeList=std::vector<int64_t>())
        {
                assert(!sqlHead.empty());
                assert(!keyColName.empty());
                assert(updateColNameList.size() == sizeof...(Args));

                sInitSizeList = initSizeList;
                if (sInitSizeList.empty() || sInitSizeList.size() != updateColNameList.size()+1)
                {
                        sInitSizeList.reserve(updateColNameList.size()+1);
                        for (int64_t i=0; i<static_cast<int64_t>(updateColNameList.size()+1); ++i)
                                sInitSizeList.emplace_back(1024);
                }
                for (auto cnt : sInitSizeList)
                {
                        (void)cnt;
                        assert(cnt > 0);
                }

                sSqlHead = sqlHead;
                sKeyColName = keyColName;
                sUpdateColNameList = updateColNameList;

                stFmtParse<Args...>::Parse(sFmtArr);
                return true;
        }

private :
        std::string _bufArr[sizeof...(Args) + 1];
        static std::string sSqlHead;
        static std::string sFmtArr[sizeof...(Args) + 1];
        static std::string sKeyColName;
        static std::vector<std::string> sUpdateColNameList; // 必须和 tuple 中一一对应
        static std::vector<int64_t> sInitSizeList;
};

template <typename _Fy, typename _Ky, typename... Args>
std::string SqlUpdateBatch<_Fy, _Ky, Args...>::sSqlHead;

template <typename _Fy, typename _Ky, typename... Args>
std::string SqlUpdateBatch<_Fy, _Ky, Args...>::sFmtArr[sizeof...(Args) + 1];

template <typename _Fy, typename _Ky, typename... Args>
std::string SqlUpdateBatch<_Fy, _Ky, Args...>::sKeyColName;

template <typename _Fy, typename _Ky, typename... Args>
std::vector<std::string> SqlUpdateBatch<_Fy, _Ky, Args...>::sUpdateColNameList;

template <typename _Fy, typename _Ky, typename... Args>
std::vector<int64_t> SqlUpdateBatch<_Fy, _Ky, Args...>::sInitSizeList;

// struct stPlayerUpdateFlag;
// typedef SqlUpdateBatch<stPlayerUpdateFlag, int64_t, int64_t, std::string_view> PlayerUpdateBatchType;
// PlayerUpdateBatchType::Init("UPDATE player_data_0 SET", "id", {"v", "data"}, {1024*1024, 1024*1024*64, 1024*1024});
// PlayerUpdateBatchType playerUpdate;
// playerUpdate.PushValue(id, v, data);
// std::string sql = playerUpdate.PackSql();
// MySql::executeUpdate(sql);
// }}}

// {{{ SqlInsertBatch
template <typename _Fy, typename... Args>
struct SqlInsertBatch
{
public :
        FORCE_INLINE void PushValue(Args ... args)
        {
                if (_buf.empty())
                {
                        _buf.reserve(sInitSize);
                        _buf += sSqlHead;
                }
                fmt::format_to(std::back_inserter(_buf), sFmt.c_str(), args...);
        }

        FORCE_INLINE std::string PackSql()
        {
                if (_buf.empty())
                        return std::string();

                _buf[_buf.size()-1] = ';';
                return std::move(_buf);
        }

private :
        template <typename T, typename ... InnerArgs>
        struct stFmtParse
        {
                FORCE_INLINE static std::string Parse(bool first)
                {
                        if constexpr (std::is_same<std::string_view, T>::value)
                                return std::string(first?"('{}'":",'{}'") + stFmtParse<InnerArgs...>::Parse(false);
                        else
                                return std::string(first?"({}":",{}") + stFmtParse<InnerArgs...>::Parse(false);
                }
        };

        template <typename T>
        struct stFmtParse<T>
        {
                FORCE_INLINE static std::string Parse(bool first)
                {
                        if constexpr (std::is_same<std::string_view, T>::value)
                                return first ? "('{}')," : ",'{}'),";
                        else
                                return first ? "({})," : ",{}),";
                }
        };

public :
        FORCE_INLINE static bool Init(const std::string& sqlHead, int64_t initSize=1024)
        {
                assert(!sqlHead.empty());
                assert(initSize > 0);

                sInitSize = initSize;
                sSqlHead = sqlHead;
                sFmt = stFmtParse<Args...>::Parse(true);
                return true;
        }

private :
        std::string _buf;
        static std::string sSqlHead;
        static std::string sFmt;
        static int64_t sInitSize;
};

template <typename _Fy, typename... Args>
std::string SqlInsertBatch<_Fy, Args...>::sSqlHead;

template <typename _Fy, typename... Args>
std::string SqlInsertBatch<_Fy, Args...>::sFmt;

template <typename _Fy, typename... Args>
int64_t SqlInsertBatch<_Fy, Args...>::sInitSize = 1024;

// struct stPlayerInsertFlag;
// typedef SqlInsertBatch<stPlayerInsertFlag, int64_t, int64_t, std::string_view> PlayerInsertBatchType;
// PlayerInsertBatchType::Init("INSERT INTO player_data_0(id, v, data) VALUES", 1024 * 1024 * 64);
// PlayerInsertBatchType playerInsert;
// playerInsert.PushValue(id, v, data);
// std::string sql = playerInsert.PackSql();
// MySql::execute(sql);
// }}}

// {{{ SqlSelectBatch
template <typename _Fy, typename _Ky>
struct SqlSelectBatch
{
public :
        FORCE_INLINE void PushValue(_Ky k)
        {
                if (_buf.empty())
                {
                        _buf.reserve(sInitSize);
                        _buf += sSqlHead;
                }
                if constexpr (std::is_same<std::string_view, _Ky>::value)
                        fmt::format_to(std::back_inserter(_buf), "'{}',", k);
                else
                        fmt::format_to(std::back_inserter(_buf), "{},", k);
        }

        FORCE_INLINE std::string PackSql()
        {
                if (_buf.empty())
                        return std::string();
                _buf.pop_back();
                _buf += ");";
                return std::move(_buf);
        }

public:
        FORCE_INLINE static bool Init(const std::string& sqlHead, int64_t initSize=1024)
        {
                assert(!sqlHead.empty());
                assert(initSize > 0);

                sInitSize = initSize;
                sSqlHead = sqlHead;
                return true;
        }

private :
        std::string _buf;
        static std::string sSqlHead;
        static int64_t sInitSize;
};

template <typename _Fy, typename _Ky>
std::string SqlSelectBatch<_Fy, _Ky>::sSqlHead;

template <typename _Fy, typename _Ky>
int64_t SqlSelectBatch<_Fy, _Ky>::sInitSize = 1024;

// struct stPlayerSelectFlag;
// typedef SqlSelectBatch<stPlayerSelectFlag, int64_t> PlayerSelectBatchType;
// PlayerSelectBatchType::Init("SELECT id,v,data FROM player_data_0 WHERE id IN(", 1024*1024*64);
// PlayerSelectBatchType playerSelect;
// playerSelect.PushValue(id);
// std::string sql = playerSelect.PackSql();
// }}}

static std::atomic_flag sMySqlInit = ATOMIC_FLAG_INIT;

template <typename _Cy>
class MySqlMgrWapper : public DBMgrBase<_Cy>, public Singleton<MySqlMgrWapper<_Cy>>
{
        typedef DBMgrBase<_Cy> SuperType;
        typedef _Cy ConnType;
        friend class Singleton<MySqlMgrWapper<_Cy>>;

protected :
        MySqlMgrWapper() = default;
        ~MySqlMgrWapper() override { }

public :
        using SuperType::Init;
        bool Init(const stMySqlConfig& cfg)
        {
                if (sMySqlInit.test_and_set())
                {
                        LOG_FATAL("MySqlMgr init twice!!!");
                        return false;
                }

                _cfg = cfg;
                SuperType::_markString = "mysql";
                if (!SuperType::Init(_cfg._thCnt, _cfg._connCnt))
                        return false;
                return true;
        }

        bool CreateConn() override
        {
                boost::system::error_code ec;
                boost::mysql::diagnostics diag;
                auto ep = boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(_cfg._host), _cfg._port);
                boost::mysql::handshake_params params(_cfg._user, _cfg._pwd, _cfg._dbName);

                while (true)
                {
                        auto conn = std::make_shared<boost::mysql::tcp_connection>(*SuperType::_ioCtx);
                        conn->async_connect(ep, params, boost::fibers::asio::yield_t(ec));
                        if (!ec)
                        {
                                return boost::fibers::channel_op_status::success == SuperType::_connQueue->try_push(conn);
                        }
                        else
                        {
                                LOG_WARN("mysql 连接失败!!! ec[{}]", ec.what());
                                boost::this_fiber::sleep_for(std::chrono::milliseconds(100));
                                ec.clear();
                                diag.clear();
                        }
                }
                return false;
        }

        FORCE_INLINE std::shared_ptr<boost::mysql::results> Exec(std::string_view sqlStr, bool retry = true)
        {
                auto conn = GetConn();
                do
                {
                        boost::system::error_code ec;
                        auto result = std::make_shared<boost::mysql::results>();
                        conn->_conn->async_execute(sqlStr, *result, boost::fibers::asio::yield_t(ec));
                        if (!ec)
                        {
                                return result;
                        }
                        else
                        {
                                LOG_WARN("mysql 连接执行失败!!! ec[{}] what[{}] sqlStr[{}]"
                                         , ec.value(), ec.what(), sqlStr.substr(0, 512));
                                boost::this_fiber::sleep_for(std::chrono::milliseconds(100));
                                conn->Reconnect();
                        }
                } while (retry);
                return nullptr;
        }

        DEFINE_CONN_WAPPER(MySqlMgrWapper<ConnType>);

private :
        stMySqlConfig _cfg;
};

typedef MySqlMgrWapper<boost::mysql::tcp_connection> MySqlMgr;

}; // end of namespace nl::db

// vim: fenc=utf8:expandtab:ts=8:noma
