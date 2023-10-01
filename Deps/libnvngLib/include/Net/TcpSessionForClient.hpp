#pragma once

#include "Tools/Util.h"
namespace nl::net::client
{

template <typename _Sy, bool server, bool ssl>
class SocketWapper
{
public :
        template <typename ... Args>
        SocketWapper(Args ... args)
                : _socket(std::forward<Args>(args)...)
        {
        }

        FORCE_INLINE void close(auto& ec)
        {
                if constexpr (IsWebsocket())
                        _socket.close(boost::beast::websocket::close_code::normal, ec);
                else
                        _socket.close(ec);
                LOG_ERROR_IF(ec, "1111111111111111111 ec[{}]", ec.what());
        }

        template <typename _Oy>
        FORCE_INLINE void set_option(_Oy o)
        { _socket.set_option(std::forward<_Oy>(o)); }

        FORCE_INLINE auto get_executor()
        { return _socket.get_executor(); }

        template <typename ... Args>
        FORCE_INLINE void async_connect(Args ... args)
        { _socket.async_connect(std::forward<Args>(args)...); }

        template <typename ... Args>
        FORCE_INLINE void async_write(const Args& ... args)
        {
                if constexpr (IsWebsocket())
                        _socket.async_write(std::forward<const Args&>(args)...);
                else
                        boost::asio::async_write(_socket, std::forward<const Args&>(args)...);
        }

        template <typename _By, typename _Fy>
        FORCE_INLINE void async_read(const _By& buf, const _Fy& cb)
        {
                if constexpr (IsWebsocket())
                        _socket.async_read(buf, std::move(cb));
                else
                        boost::asio::async_read(_socket, buf, cb);
        }

        virtual void OnEstablish(const ISessionPtr& ses)
        {
                ses->OnConnect();
        }

public :
        constexpr static bool IsWebsocket()
        { return std::is_same<_Sy, boost::beast::websocket::stream<boost::beast::tcp_stream>>::value; }

public :
        _Sy _socket;
};

template <bool server, bool ssl>
class WebsocketWapper : public SocketWapper<boost::beast::websocket::stream<boost::beast::tcp_stream>, server, ssl>
{
        typedef SocketWapper<boost::beast::websocket::stream<boost::beast::tcp_stream>, server, ssl> SuperType;
        typedef WebsocketWapper<server, ssl> ThisType;
public :
        template <typename ... Args>
        WebsocketWapper(Args ... args)
                : SuperType(std::forward<Args>(args)...)
        {
        }

        void OnEstablish(const ISessionPtr& ses) override
        {
                if constexpr (server)
                {
                        // Set suggested timeout settings for the websocket
                        ThisType::set_option(boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::server));

                        // Set a decorator to change the Server of the handshake
                        ThisType::set_option(boost::beast::websocket::stream_base::decorator([](boost::beast::websocket::response_type& res) {
                                res.set(boost::beast::http::field::server,
                                        std::string(BOOST_BEAST_VERSION_STRING) +
                                        " websocket-server-async");
                        }));

                        // Accept the websocket handshake
                        ISessionWeakPtr weakSes = ses;
                        ThisType::_socket.async_accept([weakSes](const auto& ec) {
                                auto ses = weakSes.lock();
                                if (ses)
                                {
                                        if (!ec)
                                                ses->OnConnect();
                                        else
                                                ses->OnError(ec);
                                }
                        });
                }
                else
                {
                }
        }

        template <typename _Ey, typename _Fy>
        FORCE_INLINE void async_connect(_Ey&& ep, const _Fy& cb)
        {
                // ThisType::_socket.async_connect(std::forward<Args>(args)...);
                boost::system::error_code ec;
                cb(ec);
        }
};

struct stClientSessionTag;
typedef NetMgrBase<stClientSessionTag> ClientNetMgr;

template <typename _Iy, typename _Sy, bool _ISy, typename _Hy, Compress::ECompressType _Ct>
class TcpSession : public SessionImpl<_Iy, _ISy, _Hy, _Ct, stClientSessionTag>
{
protected :
        typedef SessionImpl<_Iy, _ISy, _Hy, _Ct, stClientSessionTag> SuperType;
        typedef TcpSession<_Iy, _Sy, _ISy, _Hy, _Ct> ThisType;
        typedef _Iy ImplType;
        typedef _Sy SocketType;
        typedef typename SuperType::BuffTypePtr BuffTypePtr;
        typedef typename SuperType::Tag Tag;
        constexpr static bool IsServer = _ISy;

public :
        typedef _Hy MsgHeaderType;
        constexpr static Compress::ECompressType CompressType = _Ct;

public :
        template <typename ... Args>
        TcpSession(auto&& s, Args ... args)
                : SuperType(s._socket.get_executor())
                 , _socket(std::move(s), std::forward<Args>(args)...)
        {
        }

        ~TcpSession() override
        {
                // boost::system::error_code ec;
                // _socket.close(ec);
        }

        void OnEstablish() override
        {
                _socket.OnEstablish(shared_from_this());
        }

        void OnConnect() override
        {
                SuperType::OnConnect();
                DoRecv();
        }

        void OnClose(int32_t reasonType) override
        {
                SuperType::OnClose(reasonType);

                auto thisPtr = shared_from_this();
                // ::nl::util::SteadyTimer::StaticStart(_socket._socket.get_executor(), std::chrono::seconds(0), [thisPtr]() {
                // boost::asio::post(boost::asio::make_strand(_socket._socket.get_executor()), [thisPtr]() {
                        boost::system::error_code ec;
                        thisPtr->_socket._socket.shutdown(boost::asio::socket_base::shutdown_both, ec);
                // });
        }

private :
        void DoSend(const typename SuperType::BuffTypePtr& bufRef = nullptr, typename SuperType::BuffTypePtr::element_type* buf = nullptr, std::size_t size = 0)
        {
                LOCK_GUARD(_bufListMutex, 0, "DoSend");
                if (nullptr != buf || 0 != size)
                {
                        _bufList.emplace_back(boost::asio::const_buffer{ buf, size });
                        _bufRefList.emplace_back(bufRef);
                }

                if (!_bufList.empty() && !SuperType::IsInSend())
                {
                        SuperType::SetInSend();

                        // std::weak_ptr<ThisType> weakSes = shared_from_this();
                        auto ses = shared_from_this();
                        _socket.async_write(_bufList, [ses, refList{ std::move(_bufRefList) }](const auto& ec, std::size_t size) {
                                /*
                                 * async_write 内部异步多部操作，因此不能同时存在多个 async_write 操作，
                                 * 必须等待返回后再次调用 async_write。
                                 */

                                // auto ses = weakSes.lock();
                                if (ses)
                                {
                                        ses->DelInSend();
                                        if (!ec)
                                                ses->DoSend();
                                        else
                                                ses->OnError(ec);
                                }
                        });

                        if (_bufList.capacity() <= 4)
                        {
                                _bufList.clear();
                        }
                        else
                        {
                                std::exchange(_bufList, decltype(_bufList){});
                                _bufList.reserve(4);
                        }
                        _bufRefList.reserve(4);
                }
        }

        virtual void DoRecv()
        {
                if constexpr (!SocketType::IsWebsocket())
                {
                        // std::weak_ptr<ThisType> weakSes = shared_from_this();
                        auto ses = shared_from_this();
                        _socket.async_read(boost::asio::buffer((char*)&_msgHead, sizeof(_msgHead)),
                                           // boost::asio::transfer_at_least(sizeof(_msgHead)),
                                           [ses](const auto& ec, std::size_t size) {
                                                   // auto ses = weakSes.lock();
                                                   if (!ses)
                                                           return;

                                                   if (!ec)
                                                   {
                                                           auto buf = std::make_shared<char[]>(ses->_msgHead._size);
                                                           *reinterpret_cast<MsgHeaderType*>(buf.get()) = ses->_msgHead;
                                                           ses->_socket.async_read(boost::asio::buffer(buf.get() + sizeof(MsgHeaderType), ses->_msgHead._size - sizeof(MsgHeaderType)),
                                                                                   // boost::asio::transfer_at_least(ses->_msgHead._size - sizeof(MsgHeaderType)),
                                                                                   [ses, buf](const auto& ec, std::size_t size) {
                                                                                           // auto ses = weakSes.lock();
                                                                                           if (!ses)
                                                                                                   return;

                                                                                           if (!ec)
                                                                                           {
                                                                                                   ses->OnRecv(buf.get(), buf);
                                                                                                   ses->DoRecv();
                                                                                           }
                                                                                           else
                                                                                           {
                                                                                                   ses->OnError(ec);
                                                                                           }
                                                                                   });
                                                   }
                                                   else
                                                   {
                                                           ses->OnError(ec);
                                                           return;
                                                   }
                                           });
                }
                else
                {
                        std::weak_ptr<ThisType> weakSes = ThisType::shared_from_this();
                        ThisType::_socket._socket.async_read(_buf, [weakSes](const auto& ec, std::size_t size) {
                                auto ses = weakSes.lock();
                                if (!ses)
                                        return;

                                if (!ec)
                                {
                                        auto buf = std::make_shared<char[]>(size);
                                        memcpy(buf.get(), ses->_buf.data().data(), size);
                                        ses->_buf.consume(ses->_buf.size());
                                        ses->OnRecv(buf.get(), buf);
                                        ses->DoRecv();
                                }
                                else
                                {
                                        ses->OnError(ec);
                                }
                        });
                }
        }

public :
        template<typename _FHy, typename ... Args>
        FORCE_INLINE void SendBuf(const typename SuperType::BuffTypePtr& bufRef,
                                  typename SuperType::BuffTypePtr::element_type* buf,
                                  std::size_t bufSize, // 包括 from 的头。
                                  Args ... args)
        {
                assert(sizeof(MsgHeaderType) <= sizeof(_FHy));
                auto sendBuf = buf + sizeof(_FHy) - sizeof(MsgHeaderType);
                auto sendSize = bufSize - sizeof(_FHy) + sizeof(MsgHeaderType);
                new (sendBuf) MsgHeaderType(sendSize, std::forward<Args>(args)...);
                DoSend(bufRef, sendBuf, sendSize);
        }

        template <typename ... Args>
        FORCE_INLINE void SendPB(const std::shared_ptr<google::protobuf::MessageLite>& pb, Args ... args)
        {
                if (pb)
                {
                        auto buf = std::make_shared<char[]>(SuperType::SerializeAndCompressNeedSize(pb->ByteSizeLong()));
                        auto [sendSize, ct] = SuperType::SerializeAndCompress(*pb, buf.get()+sizeof(MsgHeaderType));
                        new (buf.get()) MsgHeaderType(sendSize+sizeof(MsgHeaderType), ct, std::forward<Args>(args)...);
                        DoSend(buf, buf.get(), sendSize+sizeof(MsgHeaderType));
                }
                else
                {
                        auto sendBuf = std::make_shared<char[]>(sizeof(MsgHeaderType));
                        new (sendBuf.get()) MsgHeaderType(sizeof(MsgHeaderType), Compress::ECompressType::E_CT_None, std::forward<Args>(args)...);
                        DoSend(sendBuf, sendBuf.get(), sizeof(MsgHeaderType));
                }
        }

private :
        MsgHeaderType _msgHead;
        boost::asio::streambuf _buf;

        SpinLock _bufListMutex;
        std::vector<boost::asio::const_buffer> _bufList;
        std::vector<typename SuperType::BuffTypePtr> _bufRefList;

public :
        SocketType _socket;

        DECLARE_SHARED_FROM_THIS(ThisType);
};

}; // end of namespace nl::net::client

// vim: fenc=utf8:expandtab:ts=8:noma
