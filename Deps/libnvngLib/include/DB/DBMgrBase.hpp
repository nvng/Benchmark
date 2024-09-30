#pragma once

namespace nl::db
{

#define DEFINE_CONN_WAPPER(mgr) \
        struct stDBConnWapper { \
                stDBConnWapper() { mgr::GetInstance()->_connQueue->pop(_conn); } \
                ~stDBConnWapper() { if (_conn) mgr::GetInstance()->_connQueue->try_push(_conn); } \
                void Reconnect() { mgr::GetInstance()->CreateConn(); mgr::GetInstance()->_connQueue->pop(_conn); } \
                std::shared_ptr<mgr::ConnType> _conn; \
        }; \
        FORCE_INLINE std::shared_ptr<stDBConnWapper> GetConn() { return std::make_shared<stDBConnWapper>(); }

enum EDBMgrFlagType
{
        E_DBMFT_Terminate = 0x0,
        E_DBMFT_Terminated = 0x1,
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

                _distIOCtxParam = thCnt - 1;
                _threadList.reserve(thCnt);
                _ioCtxArr.reserve(thCnt);
                _connQueue = std::make_shared<ConnQueueType>(connCnt * 2);

                for (decltype(thCnt) i=0; i<thCnt; ++i)
                {
                        _ioCtxArr.emplace_back(std::make_shared<boost::asio::io_context>(1));
                        std::thread t([this, i]() {
                                SetThreadName("{}_{}", _markString, i);
                                auto w = boost::asio::make_work_guard(*_ioCtxArr[i]);
                                boost::system::error_code ec;
                                _ioCtxArr[i]->run(ec);
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

        FORCE_INLINE bool IsTerminate() const { return FLAG_HAS(_internalFlag, 1 << E_DBMFT_Terminate); }

public :
        FORCE_INLINE static constexpr uint64_t GenDataKey(int64_t prefix, int64_t id = 0) { return prefix * 1000 * 1000 * 1000 * 1000LL + id; }
        virtual void Terminate()
        {
                FLAG_ADD(_internalFlag, 1 << E_DBMFT_Terminate);
        }

        virtual void WaitForTerminate()
        {
                while (!FLAG_HAS(_internalFlag, 1 << E_DBMFT_Terminated))
                {
                        LOG_WARN("not terminated!!! left cnt[{}]", _initCnt);
                        boost::this_fiber::sleep_for(std::chrono::milliseconds(100));
                }

                for (auto& ctx : _ioCtxArr)
                {
                        if (ctx && !ctx->stopped())
                                ctx->stop();
                }

                for (auto& th : _threadList)
                {
                        if (th.joinable())
                                th.join();
                }
        }

protected :
        FORCE_INLINE std::shared_ptr<boost::asio::io_context> DistCtx()
        {
                static std::atomic_int64_t idx = -1;
                return _ioCtxArr[++idx & _distIOCtxParam];
        }

        int64_t _distIOCtxParam = 0;
        std::atomic_uint64_t _distIOCtxIdx = -1;
        std::vector<std::shared_ptr<boost::asio::io_context>> _ioCtxArr;
        std::shared_ptr<ConnQueueType> _connQueue;
        std::atomic_uint64_t _internalFlag = 0;
        std::atomic_int64_t _initCnt = 0;
        std::string _markString;

private :
        std::vector<std::thread> _threadList;
};

}; // end of namespace nl::db

// vim: fenc=utf8:expandtab:ts=8:noma
