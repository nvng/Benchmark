#pragma once

namespace nl::net::server
{

enum ESessionSendBufType
{
        E_SSBT_None = 0,
        E_SSBT_GoogleProtobuf = 1,
        E_SSBT_GoogleProtobufExtra = 2,
        E_SSBT_CopyDelete = 3,
        E_SSBT_Copy = 4,
        E_SSBT_MultiCast = 5,
        E_SSBT_BroadCast = 6,
};

struct stSendRefWapper
{
        std::shared_ptr<google::protobuf::MessageLite> _msg;
        std::shared_ptr<void> _msgExtra;
        int64_t _extraSize = 0;
};

template <typename _Hy>
struct stSessionSendBufInfo
{
        _Hy _head;
        ESessionSendBufType _type = E_SSBT_None;
        const char* _buf = nullptr;
        std::shared_ptr<void> _ptr;
};

template <typename _Ty
        , bool _Sy = true
        , typename _My = MsgActorAgentHeaderType
        , Compress::ECompressType _Ct = Compress::ECompressType::E_CT_Zstd
        , typename Flag = stDefaultFlag>
class TcpSession : public SessionImpl<_Ty, _Sy, _My, _Ct, Flag>
{
        typedef SessionImpl<_Ty, _Sy, _My, _Ct, Flag> SuperType;
        typedef TcpSession<_Ty, _Sy, _My, _Ct, Flag> ThisType;
        typedef _Ty ImplType;

public :
        typedef boost::asio::ip::tcp::socket SocketType;
        typedef boost::asio::ip::tcp::endpoint EndPointType;
        typedef _My MsgHeaderType;
        constexpr static Compress::ECompressType CompressType = _Ct;
        constexpr static bool IsServer = _Sy;
        constexpr static std::size_t cRecvChannelSize = 1 << 10;
        constexpr static std::size_t cSendChannelSize = 1 << 16;

public :
        TcpSession(SocketType&& s
                   , std::size_t scSize = cSendChannelSize
                   , std::size_t rcSize = cRecvChannelSize)
                : SuperType(s.get_executor())
                  , _socket(std::move(s))
                  , _recvChannel(rcSize)
                  , _sendChannel(scSize)
        {
                if constexpr (!IsServer)
                        FLAG_ADD(SuperType::_internalFlag, E_SFT_AutoReconnect);
        }

        ~TcpSession() override
        {
                boost::system::error_code ec;
                _socket.close(ec);
        }

        void OnConnect() override
        {
                SuperType::OnConnect();
                LOG_WARN("连接成功!!! local[{}:{}] remote[{}:{}]",
                         _socket.local_endpoint().address().to_string(),
                         _socket.local_endpoint().port(),
                         _socket.remote_endpoint().address().to_string(),
                         _socket.remote_endpoint().port());

                boost::system::error_code ec;
                _socket.set_option(boost::asio::socket_base::reuse_address(true), ec);
                _socket.set_option(boost::asio::ip::tcp::no_delay(true), ec);

                DoRecv();
                std::weak_ptr<ThisType> weakSes = ThisType::shared_from_this();
                GetAppBase()->_mainChannel.push([weakSes]() {
                        auto ses = weakSes.lock();
                        if (!ses)
                                return;

                        boost::fibers::fiber(
                             std::allocator_arg,
                             boost::fibers::fixedsize_stack{ 32 * 1024 },
                             [weakSes]() {
                                auto ses = weakSes.lock();
                                if (!ses)
                                        return;

                                while (!ses->IsTerminate())
                                {
                                        auto recvBuf = ses->_recvChannel.value_pop();
                                        if (recvBuf)
                                        {
                                                const int64_t totalSize = reinterpret_cast<MsgTotalHeadType*>(recvBuf.get())->_size;
                                                auto buf = recvBuf.get() + sizeof(MsgTotalHeadType);
                                                while (totalSize - (int64_t)(buf - recvBuf.get()) >= sizeof(MsgHeaderType))
                                                {
                                                        // 顺序不能改变，msgHead 所指向内存可能在后面被修改。
                                                        auto msgHead = *reinterpret_cast<MsgHeaderType*>(buf);
                                                        buf += msgHead._size;
                                                        ses->OnRecvInternal(buf-msgHead._size, recvBuf);
                                                }
                                        }
                                }

                                ses.reset();
                                while (true)
                                {
                                        boost::this_fiber::sleep_for(std::chrono::milliseconds(1));
                                        auto ts = weakSes.lock();
                                        if (!ts)
                                                break;

                                        ISession::BuffTypePtr tBuf;
                                        while (boost::fibers::channel_op_status::success == ts->_recvChannel.try_pop(tBuf)) { }
                                }
                        }).detach();

                        boost::fibers::fiber(
                             std::allocator_arg,
                             boost::fibers::fixedsize_stack{ 32 * 1024 },
                             [weakSes]() {
                                auto ses = weakSes.lock();
                                if (!ses)
                                        return;

                                constexpr std::size_t cSendBufInitSize = 1024 * 1024;
                                auto sendBufBase = new typename ISession::BuffTypePtr::element_type[cSendBufInitSize];
                                auto sendBuf = sendBufBase + sizeof(MsgTotalHeadType);
                                std::size_t _bufSize = cSendBufInitSize;
                                std::size_t _end = sizeof(MsgTotalHeadType);
                                std::size_t _totalMsgSize = _end;

                                auto allocCacheBufferFunc = [&](std::size_t needSize) {
                                        reinterpret_cast<MsgTotalHeadType*>(sendBufBase)->_size = _totalMsgSize;
                                        ses->DoSend(sendBufBase, _totalMsgSize);

                                        // 网络压力很大的情况下，一次性分配8M。
                                        if (0 != needSize && needSize < cSendBufInitSize / 2)
                                                needSize = cSendBufInitSize * 8;
                                        _bufSize = cSendBufInitSize > needSize ? cSendBufInitSize : needSize;
                                        sendBufBase = new typename ISession::BuffTypePtr::element_type[_bufSize];
                                        sendBuf = sendBufBase + sizeof(MsgTotalHeadType);
                                        _end = sendBuf - sendBufBase;
                                        _totalMsgSize = _end;
                                };

                                stSessionSendBufInfo<MsgHeaderType> bufInfo;
                                while (!ses->IsTerminate())
                                {
                                        if (boost::fibers::channel_op_status::success == ses->_sendChannel.try_pop(bufInfo))
                                        {
__direct_deal__ :
                                                const auto& msgHead = bufInfo._head;
                                                if (_bufSize - _end < msgHead._size)
                                                        allocCacheBufferFunc(msgHead._size + sizeof(MsgTotalHeadType));

                                                *reinterpret_cast<MsgHeaderType*>(sendBuf) = msgHead;
                                                std::size_t realSendSize = msgHead._size;
                                                switch (bufInfo._type)
                                                {
                                                case E_SSBT_None :
                                                        goto __end__;
                                                case E_SSBT_CopyDelete :
                                                        memcpy(sendBuf+sizeof(MsgHeaderType), bufInfo._buf, msgHead._size-sizeof(MsgHeaderType));
                                                        delete[] bufInfo._buf;
                                                        bufInfo._buf = nullptr;
                                                        break;
                                                case E_SSBT_Copy :
                                                        memcpy(sendBuf+sizeof(MsgHeaderType), bufInfo._buf, msgHead._size-sizeof(MsgHeaderType));
                                                        break;
                                                case E_SSBT_GoogleProtobuf :
                                                        if (bufInfo._ptr)
                                                        {
                                                                auto pb = std::reinterpret_pointer_cast<google::protobuf::MessageLite>(bufInfo._ptr);
                                                                auto [sendSize, ct] = SuperType::SerializeAndCompress(*pb, sendBuf+sizeof(MsgHeaderType));
                                                                reinterpret_cast<MsgHeaderType*>(sendBuf)->_ct = ct;
                                                                realSendSize = sendSize + sizeof(MsgHeaderType);
                                                                reinterpret_cast<MsgHeaderType*>(sendBuf)->_size = realSendSize;
                                                        }
                                                        break;
                                                case E_SSBT_GoogleProtobufExtra :
                                                        if (bufInfo._ptr)
                                                        {
                                                                auto pbInfo = std::reinterpret_pointer_cast<stSendRefWapper>(bufInfo._ptr);
                                                                std::size_t msgSendSize = 0;
                                                                Compress::ECompressType ct = Compress::ECompressType::E_CT_None;
                                                                if (pbInfo->_msg)
                                                                        std::tie(msgSendSize, ct) = SuperType::SerializeAndCompress(*pbInfo->_msg, sendBuf+sizeof(MsgHeaderType)+sizeof(uint32_t));
                                                                memcpy(sendBuf+sizeof(MsgHeaderType)+sizeof(uint32_t)+msgSendSize, bufInfo._buf, pbInfo->_extraSize);
                                                                *reinterpret_cast<uint32_t*>(sendBuf+sizeof(MsgHeaderType)) = msgSendSize;
                                                                auto tmpMsgHead = reinterpret_cast<MsgHeaderType*>(sendBuf);
                                                                realSendSize = sizeof(MsgHeaderType) + sizeof(uint32_t) + msgSendSize + pbInfo->_extraSize;
                                                                tmpMsgHead->_size = realSendSize;
                                                                tmpMsgHead->_ct = ct;
                                                        }
                                                        break;
                                                case E_SSBT_MultiCast :
                                                        {
                                                                auto pbInfo = std::reinterpret_pointer_cast<stSendRefWapper>(bufInfo._ptr);
                                                                std::size_t msgSendSize = 0;
                                                                Compress::ECompressType ct = Compress::ECompressType::E_CT_None;
                                                                if (pbInfo->_msg)
                                                                        std::tie(msgSendSize, ct) = SuperType::SerializeAndCompress(*pbInfo->_msg, sendBuf+sizeof(typename MsgHeaderType::MsgMultiCastHeader));
                                                                auto [idsMsgSendSize, sct] = SuperType::SerializeAndCompress(*std::reinterpret_pointer_cast<google::protobuf::MessageLite>(pbInfo->_msgExtra), sendBuf+sizeof(typename MsgHeaderType::MsgMultiCastHeader)+msgSendSize);
                                                                realSendSize = sizeof(typename MsgHeaderType::MsgMultiCastHeader) + msgSendSize + idsMsgSendSize;
                                                                new (sendBuf) typename MsgHeaderType::MsgMultiCastHeader(realSendSize,
                                                                                                                         E_MIMT_Internal, E_MIIST_MultiCast, msgHead.MainType(), msgHead.SubType(),
                                                                                                                         ct, sct, idsMsgSendSize, msgHead._from);
                                                        }
                                                        break;
                                                case E_SSBT_BroadCast :
                                                        new (sendBuf) typename MsgHeaderType::MsgMultiCastHeader(sizeof(typename MsgHeaderType::MsgMultiCastHeader), E_MIMT_Internal, E_MIIST_BroadCast, msgHead.MainType(), msgHead.SubType(),
                                                                                                                 Compress::ECompressType::E_CT_None, Compress::ECompressType::E_CT_None, 0, msgHead._from);
                                                        if (bufInfo._ptr)
                                                        {
                                                                auto pb = std::reinterpret_pointer_cast<google::protobuf::MessageLite>(bufInfo._ptr);
                                                                auto [sendSize, ct] = SuperType::SerializeAndCompress(*pb, sendBuf + sizeof(typename MsgHeaderType::MsgMultiCastHeader));
                                                                reinterpret_cast<typename MsgHeaderType::MsgMultiCastHeader*>(sendBuf)->_ct = ct;
                                                                realSendSize = sendSize + sizeof(typename MsgHeaderType::MsgMultiCastHeader);
                                                                reinterpret_cast<typename MsgHeaderType::MsgMultiCastHeader*>(sendBuf)->_size = realSendSize;
                                                        }
                                                        break;
                                                default :
                                                        break;
                                                }

                                                bufInfo._type = E_SSBT_None;
                                                sendBuf += realSendSize;
                                                _end += realSendSize;
                                                _totalMsgSize += realSendSize;
                                        }
                                        else
                                        {
                                                if (_totalMsgSize > sizeof(MsgTotalHeadType))
                                                {
                                                        allocCacheBufferFunc(0);
                                                        boost::this_fiber::sleep_for(std::chrono::milliseconds(1));
                                                }
                                                else
                                                {
                                                        ses->_sendChannel.pop(bufInfo);
                                                        goto __direct_deal__;
                                                }
                                        }
                                }
__end__ :
                                delete[] sendBufBase;

                                ses.reset();
                                while (true)
                                {
                                        boost::this_fiber::sleep_for(std::chrono::milliseconds(1));
                                        auto ts = weakSes.lock();
                                        if (!ts)
                                                break;

                                        while (boost::fibers::channel_op_status::success == ts->_sendChannel.try_pop(bufInfo)) { }
                                }
                        }).detach();
                });
        }

        void Close(int32_t reasonType) override
        {
                SuperType::Close(reasonType);

                _recvChannel.push(nullptr);
                _sendChannel.push(stSessionSendBufInfo<MsgHeaderType>{});
        }

        void DoSend(typename SuperType::BuffTypePtr::element_type* buf = nullptr, std::size_t size = 0)
        {
                std::lock_guard l(_bufListMutex);
                if (nullptr != buf && 0 != size)
                        _bufList.emplace_back(boost::asio::const_buffer{ buf, size });

                if (!FLAG_HAS(SuperType::_internalFlag, E_SFT_InSend))
                {
                        FLAG_ADD(SuperType::_internalFlag, E_SFT_InSend);
                        std::weak_ptr<TcpSession> weakSes = ThisType::shared_from_this();
                        auto bufList = std::make_shared<std::vector<boost::asio::const_buffer>>(std::move(_bufList));
                        boost::asio::async_write(_socket, *bufList, [weakSes, bufList](const auto& ec, std::size_t size) {
                                /*
                                 * async_write 内部异步多部操作，因此不能同时存在多个 async_write 操作，
                                 * 必须等待返回后再次调用 async_write。
                                 */

                                for (auto& val : *bufList)
                                        delete[] (typename ISession::BuffTypePtr::element_type*)val.data();

                                auto ses = weakSes.lock();
                                if (!ses)
                                        return;

                                FLAG_DEL(ses->_internalFlag, E_SFT_InSend);
                                if (!ec)
                                {
                                        bool ret = true;
                                        {
                                                std::lock_guard l(ses->_bufListMutex);
                                                ret = ses->_bufList.empty();
                                        }

                                        if (!ret)
                                                ses->DoSend();
                                }
                                else
                                {
                                        ses->OnError(ec);
                                }
                        });
                }
        }

        void DoRecv()
        {
                std::weak_ptr<TcpSession> weakSes = ThisType::shared_from_this();
                boost::asio::async_read(_socket, boost::asio::buffer((char*)&_msgTotalHead, sizeof(_msgTotalHead)),
                                        // boost::asio::transfer_at_least(sizeof(_totalMsgHead)),
                                        [weakSes](const auto& ec, std::size_t size) {
                                                auto ses = weakSes.lock();
                                                if (!ses)
                                                        return;

                                                if (!ec)
                                                {
                                                        auto buf = std::make_shared<char[]>(ses->_msgTotalHead._size);
                                                        *reinterpret_cast<MsgTotalHeadType*>(buf.get()) = ses->_msgTotalHead;
                                                        boost::asio::async_read(ses->_socket, boost::asio::buffer(buf.get() + sizeof(MsgTotalHeadType), ses->_msgTotalHead._size - sizeof(MsgTotalHeadType)),
                                                                                // boost::asio::transfer_at_least(ses->_totalMsgHead._size - sizeof(MsgTotalHeadType)),
                                                                                [weakSes, buf](const auto& ec, std::size_t size) {
                                                                                        auto ses = weakSes.lock();
                                                                                        if (!ses)
                                                                                                return;

                                                                                        if (!ec)
                                                                                        {
                                                                                                if (boost::fibers::channel_op_status::success != ses->_recvChannel.try_push(buf))
                                                                                                {
                                                                                                        LOG_WARN("收消息时，_recvChannel 阻塞!!!");
                                                                                                        ses->_recvChannel.push(buf);
                                                                                                }
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

        template<typename ... Args>
        FORCE_INLINE void SendBuf(const typename SuperType::BuffTypePtr& bufRef,
                                  typename SuperType::BuffTypePtr::element_type* buf,
                                  std::size_t bufSize,
                                  Args ... args)
        {
                _sendChannel.push({
                        MsgHeaderType{ bufSize + sizeof(MsgHeaderType), std::forward<Args>(args)... },
                        E_SSBT_Copy,
                        buf,
                        bufRef});
        }

        template <typename ... Args>
        FORCE_INLINE void SendPB(const std::shared_ptr<google::protobuf::MessageLite>& pb,
                                 const Args ... args)
        {
                // {} 初始化会做严格类型检查。
                auto pbSize = pb ? pb->ByteSizeLong() : 0;
                _sendChannel.push({
                        MsgHeaderType(SuperType::SerializeAndCompressNeedSize(pbSize), Compress::ECompressType::E_CT_None, std::forward<const Args>(args)...),
                                E_SSBT_GoogleProtobuf,
                                nullptr,
                                pb});
        }

        template <typename ... Args>
        FORCE_INLINE void SendPBExtra(const std::shared_ptr<google::protobuf::MessageLite>& pb,
                                      const std::shared_ptr<void>& bufRef,
                                      const char* buf,
                                      std::size_t bufSize,
                                      const Args ... args)
        {
                // {} 初始化会做严格类型检查。
                auto pbSize = pb ? pb->ByteSizeLong() : 0;
                auto info = std::make_shared<stSendRefWapper>();
                info->_msg = pb;
                info->_msgExtra = bufRef;
                info->_extraSize = bufSize;
                _sendChannel.push({
                        MsgHeaderType(sizeof(MsgHeaderType) + sizeof(uint32_t) + pbSize + Compress::CompressedSize<CompressType>(pbSize) + bufSize,
                                      Compress::ECompressType::E_CT_None, std::forward<const Args>(args)...),
                                E_SSBT_GoogleProtobufExtra,
                                buf,
                                info});
        }

        FORCE_INLINE void MultiCast(const std::shared_ptr<google::protobuf::MessageLite>& msg,
                                    const std::shared_ptr<MsgMultiCastInfo>& idsMsg,
                                    uint64_t mt, uint64_t st, uint64_t from)
        {
                if (!idsMsg || idsMsg->id_list_size() <= 0)
                        return;

                auto msgByteSize = msg ? msg->ByteSizeLong() : 0;
                auto idsMsgByteSize = idsMsg->ByteSizeLong();

                auto info = std::make_shared<stSendRefWapper>();
                info->_msg = msg;
                info->_msgExtra = idsMsg;
                _sendChannel.push({
                        MsgHeaderType(sizeof(typename MsgHeaderType::MsgMultiCastHeader) + msgByteSize + Compress::CompressedSize<CompressType>(msgByteSize) + idsMsgByteSize + Compress::CompressedSize<CompressType>(idsMsgByteSize),
                                      Compress::ECompressType::E_CT_None, mt, st, MsgHeaderType::E_RMT_Send, 0, from, 0),
                                E_SSBT_MultiCast,
                                nullptr,
                                info
                });
        }

        FORCE_INLINE void BroadCast(const std::shared_ptr<google::protobuf::MessageLite>& pb,
                                    uint64_t mt, uint64_t st, uint64_t from)
        {
                auto pbSize = pb ? pb->ByteSizeLong() : 0;
                _sendChannel.push({
                        MsgHeaderType(SuperType::SerializeAndCompressNeedSize(pbSize), Compress::ECompressType::E_CT_None, mt, st, MsgHeaderType::E_RMT_Send, 0, from, 0),
                        E_SSBT_BroadCast,
                        nullptr,
                        pb
                });
        }

public :
        SocketType _socket;

        boost::fibers::buffered_channel<ISession::BuffTypePtr> _recvChannel;
        boost::fibers::buffered_channel<stSessionSendBufInfo<MsgHeaderType>> _sendChannel;

        SpinLock _bufListMutex;
        std::vector<boost::asio::const_buffer> _bufList;
        
        MsgTotalHeadType _msgTotalHead;

        DECLARE_SHARED_FROM_THIS(ThisType);
};

}; // end of namespace nl::net::server

// vim: fenc=utf8:expandtab:ts=8:noma
