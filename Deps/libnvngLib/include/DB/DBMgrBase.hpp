#pragma once

namespace nl::db
{

#define DEFINE_CONN_WAPPER(mgr) \
        struct stDBConnWapper { \
                stDBConnWapper() { mgr::GetInstance()->_connQueue->pop(_conn); } \
                ~stDBConnWapper() { if (_conn) mgr::GetInstance()->_connQueue->push(_conn); } \
                void Reconnect() { mgr::GetInstance()->CreateConn(); mgr::GetInstance()->_connQueue->pop(_conn); } \
                std::shared_ptr<mgr::ConnType> _conn; \
        }; \
        FORCE_INLINE std::shared_ptr<stDBConnWapper> GetConn() { return std::make_shared<stDBConnWapper>(); }

enum EDBMgrFlagType
{
        E_DBMFT_Terminate = 0,
};

template <typename ConnType>
class DBMgrBase
{
        typedef boost::fibers::buffered_channel<std::shared_ptr<ConnType>> ConnQueueType;
protected :
        DBMgrBase() = default;
        virtual ~DBMgrBase() { }

        virtual bool Init(std::size_t thCnt, std::size_t connCnt)
        {
                LOG_FATAL_IF(!CHECK_2N(connCnt), "conn cnt must 2^N !!! connCnt[{}]", connCnt);

                _ioCtx = std::make_shared<boost::asio::io_context>(thCnt);
                _connQueue = std::make_shared<ConnQueueType>(connCnt * 2);

                for (decltype(thCnt) i=0; i<thCnt; ++i)
                {
                        std::thread t([this, i]() {
                                SetThreadName("{}_{}", _markString, i);
                                auto w = boost::asio::make_work_guard(*_ioCtx);
                                boost::system::error_code ec;
                                _ioCtx->run(ec);
                                LOG_WARN_IF(boost::system::errc::success != ec.value(),
                                            "结束 {} _ioCtx 错误!!! ec[{}]", _markString, ec.what());
                        });
                        t.detach();
                        _threadList.emplace_back(std::move(t));
                }

                for (int64_t i=0; i<connCnt; ++i)
                {
                        if (!CreateConn())
                        {
                                LOG_WARN("conn queue 已经满!!! size[{}]", connCnt);
                                break;
                        }
                        else
                        {
                                LOG_INFO("conn success!!! idx[{}]", i);
                        }
                }

                return true;
        }

        virtual bool CreateConn() = 0;

        FORCE_INLINE static constexpr uint64_t GenDataKey(int64_t prefix) { return prefix * 1000 * 1000 * 1000 * 1000LL; }
        FORCE_INLINE bool IsTerminate() const { return FLAG_HAS(_internalFlag, 1 << E_DBMFT_Terminate); }

public :
        virtual void Terminate()
        {
                _ioCtx->stop();
                FLAG_ADD(_internalFlag, 1 << E_DBMFT_Terminate);
        }

        virtual void WaitForTerminate()
        {
                for (auto& th : _threadList)
                {
                        if (th.joinable())
                                th.join();
                }
        }

protected :
        std::shared_ptr<boost::asio::io_context> _ioCtx;
        std::shared_ptr<ConnQueueType> _connQueue;
        std::atomic_uint64_t _internalFlag = 0;
        std::string _markString;

private :
        std::vector<std::thread> _threadList;
};

}; // end of namespace nl::db

// vim: fenc=utf8:expandtab:ts=8:noma
