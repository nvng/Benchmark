#pragma once

#include "boost/asio/signal_set.hpp"
#include <boost/asio/ssl/context.hpp>
#include <boost/url.hpp>

namespace nl::net
{

#define NET_MODEL_NAME          E_LOG_MT_Net
#define HTTP_NET_MODEL_NAME     E_LOG_MT_NetHttp

struct stHttpReq
{
        stHttpReq() = default;

        explicit stHttpReq(auto& ctx)
                : _socket(std::make_shared<boost::asio::ip::tcp::socket>(ctx))
        {
        }

        ~stHttpReq()
        {
                Reply("{}");
        }

        bool Reply(std::string_view data)
        {
                if (_socket)
                {
                        _response.set(boost::beast::http::field::content_type, "application/json;charset=UTF-8");
                        _response.set(boost::beast::http::field::content_length, fmt::format_int(data.length()).str());
                        boost::beast::ostream(_response.body()) << data;

                        boost::beast::http::async_write(*_socket, _response, [s{std::move(_socket)}](auto ec, std::size_t size) mutable {
                                // LOG_INFO("1111111 size:{} socket:{} ec[{}]", size, s->is_open(), ec.what());
                                s->shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
                                // LOG_INFO("333333333 ec[{}]", ec.what());
                        });
                        return true;
                }
                return false;
        }

        std::shared_ptr<boost::asio::ip::tcp::socket> _socket;
        boost::beast::flat_buffer _buf;
        boost::beast::http::request<boost::beast::http::string_body> _req;
        boost::beast::http::response<boost::beast::http::dynamic_body> _response;
};
typedef std::shared_ptr<stHttpReq> stHttpReqPtr;

struct stMailHttpReq : public stActorMailBase
{
        stHttpReqPtr _httpReq;
};

enum ESessionFlagType
{
        E_SFT_AutoReconnect = 1L << 0,
        E_SFT_RemoteCrash = 1L << 2,
        E_SFT_AutoRebind = 1L << 3,
        E_SFT_InSend = 1L << 4,
        E_SFT_Terminate = 1L << 5,
};

#define DEFINE_SESSION_FLAG(flag) \
        FORCE_INLINE void Set##flag() { FLAG_ADD(_internalFlag, E_SFT_##flag); } \
        FORCE_INLINE void Del##flag() { FLAG_DEL(_internalFlag, E_SFT_##flag); } \
        FORCE_INLINE bool Is##flag() const { return FLAG_HAS(_internalFlag, E_SFT_##flag); }

class ISession
{
public :
        typedef std::shared_ptr<char[]> BuffTypePtr;

public :
        ISession() = default;
        virtual ~ISession()
        {
                LOG_WARN_IF(0 != GetID(), "ISession::~ISession this:{} id:{}", fmt::ptr(this), GetID());
        }

        virtual bool Init()
        {
                static uint64_t sGuid = 0;
                _id = ++sGuid;
                return true;
        }

        virtual void OnEstablish() { OnConnect(); }
        virtual void OnConnect() { }
        virtual void Close(int32_t reasonType)
        {
                SetTerminate();
        }

        virtual void OnClose(int32_t reasonType) { }

        virtual void OnError(const boost::system::error_code& ec)
        {
                switch (ec.value())
                {
                case boost::asio::error::connection_refused :
                case boost::asio::error::operation_aborted :
                case boost::asio::error::connection_reset :
                case boost::asio::error::eof :
                        break;
                default :
                        // LOG_WARN("333333333333333 ec[{}] what[{}]", ec.value(), ec.what());
                        break;
                }

                Close(ec.value());
        }

        DEFINE_SESSION_FLAG(AutoReconnect);
        DEFINE_SESSION_FLAG(RemoteCrash);
        DEFINE_SESSION_FLAG(AutoRebind);
        DEFINE_SESSION_FLAG(InSend);
        DEFINE_SESSION_FLAG(Terminate);

        FORCE_INLINE bool IsCrash() const { return GetAppBase()->IsInitSuccess() ? false : GetAppBase()->IsCrash(); }

public :
        FORCE_INLINE uint64_t GetSID() const { return _sid; }
        FORCE_INLINE void SetSID(uint64_t sid) { _sid = sid; }
        FORCE_INLINE uint64_t GetRemoteID() const { return _remoteID; }
        FORCE_INLINE void SetRemoteID(int64_t remoteID) { _remoteID = remoteID; }
        FORCE_INLINE uint64_t GetID() const { return _id; }

public :
        uint32_t _sid = 0;
        uint32_t _remoteID = 0;

private :
        uint64_t _id = 0;
        std::atomic_uint64_t _internalFlag = 0;
};

typedef std::shared_ptr<ISession> ISessionPtr;
typedef std::weak_ptr<ISession> ISessionWeakPtr;

template <typename _Tag>
class NetMgrBase : public Singleton<NetMgrBase<_Tag>>
{
        typedef boost::asio::ip::tcp::acceptor AcceptorType;
        typedef std::shared_ptr<AcceptorType> AcceptorTypePtr;
        typedef std::weak_ptr<AcceptorType> AcceptorTypeWeakPtr;

public :
        NetMgrBase() : _sesList("NetMgrBase_sesList") { }
        virtual ~NetMgrBase() = default;

        bool Init(int64_t threadCnt = 1, const std::string& flagName = "default")
        {
                LOG_FATAL_IF(threadCnt<=0 || !CHECK_2N(threadCnt),
                             "threadCnt[{}] 必须设置为 2^N !!!",
                             threadCnt);

                _distIOCtxParam = threadCnt - 1;
                _threadList.reserve(threadCnt);
                _ioCtxArr.reserve(threadCnt);
                for (int64_t i=0; i<threadCnt; ++i)
                {
                        _ioCtxArr.emplace_back(std::make_shared<boost::asio::io_context>(1));
                        std::thread t([this, i, flagName]() {
                                SetThreadName("net_{}_{}", flagName, i);
                                auto w = boost::asio::make_work_guard(*_ioCtxArr[i]);
                                _ioCtxArr[i]->run();
                                LOG_WARN("net_{} thread exit success!!!", i);
                        });
                        t.detach();
                        _threadList.emplace_back(std::move(t));
                }

                return true;
        }

        template <typename _Fy, typename ... Args>
        void RegistSignals(const _Fy& cb, Args ... args)
        {
                auto sig = std::make_shared<boost::asio::signal_set>(*DistCtx(++_distIOCtxIdx), std::forward<Args>(args)...);
                sig->async_wait([sig, cb{std::move(cb)}](const auto& ec, auto no) {
                        if (!ec)
                                cb(no);
                });
        }

        void Connect(const std::string& ip, uint16_t port, const auto& cb)
        {
                auto ses = cb(boost::asio::ip::tcp::socket(boost::asio::make_strand(*DistCtx(++_distIOCtxIdx))));
                ses->_createSession = std::move(cb);
                ses->_connectEndPoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(ip), port);
                ses->_socket.async_connect(ses->_connectEndPoint, [ses](const auto& ec) {
                        if (ec
                            || !ses->Init()
                            || !NetMgrBase::GetInstance()->_sesList.Add(ses->GetID(), ses))
                        {
                                auto ctx = ses->_socket.get_executor();
                                ::nl::util::SteadyTimer::StaticStart(ctx, std::chrono::milliseconds(100), [ses]() {
                                        NetMgrBase::GetInstance()->Connect(ses->_connectEndPoint.address().to_string()
                                                                           , ses->_connectEndPoint.port()
                                                                           , std::move(ses->_createSession));
                                });
                        }
                        else
                        {
                                // LOG_INFO("NetMgrBase::Connect async_connect ec[{}] what[{}]", ec.value(), ec.what());
                                ses->OnEstablish();
                        }
                });
        }

        bool Listen(uint16_t port, const auto& cb)
        {
                std::lock_guard l(_acceptorListMutex);

                auto key = std::make_pair("0.0.0.0", port);
                auto ac = _acceptorList.Get(key);
                if (ac)
                        return false;

                auto acceptor = std::make_shared<AcceptorType>(*DistCtx(++_distIOCtxIdx), boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port));
                auto ret = _acceptorList.Add(key, acceptor);
                if (ret)
                {
                        boost::system::error_code ec;
                        acceptor->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true), ec);
                        DoAccept(acceptor, std::move(cb));
                }
                LOG_WARN("tcp listen ip[{}] port[{}]", key.first, key.second);
                return ret;
        }

        bool StopAccepter(uint16_t port)
        {
                auto key = std::make_pair("0.0.0.0", port);
                std::lock_guard l(_acceptorListMutex);
                auto ac = _acceptorList.Get(key);
                if (!ac)
                        return true;

                boost::system::error_code ec;
                ac->close(ec);
                return boost::system::errc::success == ec.value();
        }

        void StopAllAcceptor()
        {
                std::lock_guard l(_acceptorListMutex);
                _acceptorList.Foreach([](const auto& ac) {
                        boost::system::error_code ec;
                        ac->close(ec);
                        LOG_WARN_IF(boost::system::errc::success != ec.value(),
                                    "acceptor close error!!! ec[{}]", ec.what());
                });
                _acceptorList.Clear();
        }

private :
        // 在网络线程中执行。
        void DoAccept(const AcceptorTypePtr& acceptor, const auto& cb)
        {
                AcceptorTypeWeakPtr weakAcceptor = acceptor;
                acceptor->async_accept(boost::asio::make_strand(*DistCtx(++_distIOCtxIdx)), [weakAcceptor, cb{std::move(cb)}](const auto& ec, auto&& s) {
                        if (!ec)
                        {
                                auto ses = cb(std::move(s), NetMgrBase::GetInstance()->_sslCtx);
                                if (ses->Init() && NetMgrBase::GetInstance()->_sesList.Add(ses->GetID(), ses))
                                        ses->OnEstablish();
                        }

                        auto acceptor = weakAcceptor.lock();
                        if (acceptor)
                                NetMgrBase::GetInstance()->DoAccept(acceptor, std::move(cb));
                });
        }

public :
        void HttpReq(std::string_view urlStr, std::string_view body, const auto& cb, std::string_view contentType = "application/json;charset=UTF-8")
        {
                boost::urls::url_view url(urlStr);
                std::string host = url.host_address();
                std::string_view target = url.encoded_target().data();
                std::string_view port = url.port();
                if (port.empty())
                {
                        switch (url.scheme_id())
                        {
                        case boost::urls::scheme::http : port = "80"; break;
                        case boost::urls::scheme::https : port = "443"; break;
                        default : break;
                        }
                }

                // boost::asio::ssl::context sslCtx{ boost::asio::ssl::context::sslv23_client };
                // sslCtx.use_certificate_chain_file("");
                // sslCtx.use_private_key_file("", boost::asio::ssl::context::pem);
                // sslCtx.use_tmp_dh_file("");
                // auto s = std::make_shared<boost::beast::ssl_stream<boost::beast::tcp_stream>>(*NetMgr::GetInstance()->_ioCtx, sslCtx);

                auto s = std::make_shared<boost::beast::tcp_stream>(*NetMgrBase::GetInstance()->DistCtx(++NetMgrBase::GetInstance()->_distIOCtxIdx));
                s->expires_after(std::chrono::seconds(5));

                auto _req = std::make_shared<boost::beast::http::request<boost::beast::http::string_body>>();
                _req->method(body.empty() ? boost::beast::http::verb::get : boost::beast::http::verb::post);
                _req->target(target.empty() ? "/" : target);
                _req->set(boost::beast::http::field::host, host);
                _req->set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);
                if (!body.empty())
                {
                        // _req->set(boost::beast::http::field::content_type, "application/json;charset=UTF-8");

                        // body 格式：times=123&token=abc
                        // Note: 字符串不需要双引号。
                        // _req->set(boost::beast::http::field::content_type, "application/x-www-form-urlencoded;charset=UTF-8"); // form-data

                        _req->set(boost::beast::http::field::content_type, contentType);
                        _req->set(boost::beast::http::field::content_length, fmt::format_int(body.size()).c_str());
                        // _req->set(boost::beast::http::field::body, body); 无法传递 body
                        _req->body() = body;
                        _req->prepare_payload();
                }

                auto _resolver = std::make_shared<boost::asio::ip::tcp::resolver>(*DistCtx(++_distIOCtxIdx));
                _resolver->async_resolve(host, port, [s, _resolver, _req, cb{std::move(cb)}](const auto& ec, const auto& results) {
                        if (ec)
                        {
                                cb("", ec);
                                return;
                        }

                        s->async_connect(results, [s, _req, cb{std::move(cb)}](const auto& ec, auto) {
                                if (ec)
                                {
                                        cb("", ec);
                                        return;
                                }
                                boost::beast::http::async_write(*s, *_req, [s, _req, cb{std::move(cb)}](const auto& ec, auto bytesTransferred) {
                                        if (ec)
                                        {
                                                cb("", ec);
                                                return;
                                        }

                                        auto _buf = std::make_shared<boost::beast::flat_buffer>();
                                        auto _res = std::make_shared<boost::beast::http::response<boost::beast::http::string_body>>();
                                        boost::beast::http::async_read(*s, *_buf, *_res, [s, _buf, _req, _res, cb{std::move(cb)}](auto ec, auto bytesTransferred) {
                                                if (ec)
                                                {
                                                        cb("", ec);
                                                        return;
                                                }

                                                cb(_res->body(), ec);
                                                s->socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                                        });
                                });
                        });
                });
        }

        void HttpListen(uint16_t port, const auto& cb)
        {
                std::lock_guard l(_acceptorListMutex);
                auto key = std::make_pair("0.0.0.0", port);
                auto acceptor = std::make_shared<AcceptorType>(*DistCtx(++_distIOCtxIdx), boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port));
                auto ret = _acceptorList.Add(key, acceptor);
                if (ret)
                {
                        boost::system::error_code ec;
                        acceptor->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true), ec);
                        HttpAccept(acceptor, cb);
                }

                LOG_WARN("http listen ip[{}] port[{}]", key.first, key.second);
        }

private :

        void HttpAccept(const AcceptorTypePtr& acceptor, const auto& cb)
        {
                auto req = std::make_shared<stHttpReq>(*DistCtx(++_distIOCtxIdx));
                acceptor->async_accept(*req->_socket, [req, acceptor, cb](const auto& ec) {
                        boost::beast::http::async_read(*req->_socket, req->_buf, req->_req, [req, cb](const auto& ec, auto bytesTransferred) mutable {
                                req->_response.version(req->_req.version());
                                req->_response.keep_alive(false);

                                switch (req->_req.method())
                                {
                                case boost::beast::http::verb::get :
                                        req->_response.result(boost::beast::http::status::ok);
                                        req->_response.set(boost::beast::http::field::server, "Beast");
                                        break;
                                case boost::beast::http::verb::post :
                                        break;
                                default :
                                        break;
                                }

                                cb(std::move(req));
                        });

                        NetMgrBase::GetInstance()->HttpAccept(std::move(acceptor), cb);
                });
        }

public :
        virtual void Terminate()
        {
                StopAllAcceptor();

                _sesList.Foreach([](const auto& ses) {
                        ses->Close(0);
                });

                for (auto& ctx : _ioCtxArr)
                {
                        if (ctx && !ctx->stopped())
                                ctx->stop();
                }
        }

        virtual void WaitForTerminate()
        {
                for (auto& th : _threadList)
                {
                        if (th.joinable())
                                th.join();
                }
        }

public :
        ThreadSafeUnorderedMap<uint64_t, ISessionPtr> _sesList;

private :
        FORCE_INLINE std::shared_ptr<boost::asio::io_context> DistCtx(uint64_t idx)
        { return _ioCtxArr[idx & _distIOCtxParam]; }

        std::vector<std::shared_ptr<boost::asio::io_context>> _ioCtxArr;
        std::shared_ptr<boost::asio::ssl::context> _sslCtx;
        std::vector<std::thread> _threadList;
        int64_t _distIOCtxParam = 0;
        std::atomic_uint64_t _distIOCtxIdx = -1;

        SpinLock _acceptorListMutex;
        Map<std::pair<std::string, uint64_t>, AcceptorTypePtr> _acceptorList;
};

typedef NetMgrBase<stDefaultTag> NetMgr;

}; // end of namespace nl::net
