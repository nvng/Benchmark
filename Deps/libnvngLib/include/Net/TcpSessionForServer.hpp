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
        , typename _Tag = stDefaultTag>
class TcpSession : public SessionImpl<_Ty, _Sy, _My, _Ct, _Tag>
{
        typedef SessionImpl<_Ty, _Sy, _My, _Ct, _Tag> SuperType;
        typedef TcpSession<_Ty, _Sy, _My, _Ct, _Tag> ThisType;
        typedef _Ty ImplType;

public :
        typedef boost::asio::ip::tcp::socket SocketType;
        typedef boost::asio::ip::tcp::endpoint EndPointType;
        typedef _My MsgHeaderType;
        typedef typename SuperType::Tag Tag;
        constexpr static Compress::ECompressType CompressType = _Ct;
        constexpr static bool IsServer = _Sy;
        constexpr static std::size_t cRecvChannelSize = 1 << 10;
        constexpr static std::size_t cSendChannelSize = 1 << 15; // 上限，超过效率明显降低。
        // constexpr static std::size_t cSendChannelSize = 1 << 12; // 下限

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
                        SuperType::SetAutoReconnect();
        }

        ~TcpSession() override
        {
        }

        void OnConnect() override
        {
                SuperType::OnConnect();

                boost::system::error_code ec;
                _socket.set_option(boost::asio::socket_base::reuse_address(true), ec);
                _socket.set_option(boost::asio::ip::tcp::no_delay(true), ec);

                boost::asio::ip::tcp::endpoint localEndPoint = _socket.local_endpoint(ec);
                boost::asio::ip::tcp::endpoint remoteEndPoint = _socket.remote_endpoint(ec);

                LOG_WARN("连接成功!!! local[{}:{}] remote[{}:{}] ec[{}]",
                         localEndPoint.address().to_string(),
                         localEndPoint.port(),
                         remoteEndPoint.address().to_string(),
                         remoteEndPoint.port(),
                         boost::system::errc::success == ec.value() ? "success" : ec.what());

                DoRecv();
                auto ses = ThisType::shared_from_this();
                GetAppBase()->_mainChannel.push([ses]() {
                        boost::fibers::fiber([ses]() {
                                while (!ses->IsTerminate())
                                {
                                        auto recvBuf = ses->_recvChannel.value_pop();
                                        if (recvBuf)
                                        {
                                                // 不需要 crc32 进行包体校验，同一子网内，数据链路层已经保证包体完整性。
                                                // 若中途需经过路由器，则需要进行校验，应用层不保证包体完整。
                                                auto totalHead = *reinterpret_cast<MsgTotalHeadType*>(recvBuf.get());
                                                auto buf = recvBuf.get() + sizeof(MsgTotalHeadType);
                                                while (totalHead._size - (int64_t)(buf - recvBuf.get()) >= sizeof(MsgHeaderType))
                                                {
                                                        // 顺序不能改变，msgHead 所指向内存可能在后面被修改。
                                                        auto msgHead = *reinterpret_cast<MsgHeaderType*>(buf);
                                                        buf += msgHead._size;
                                                        ses->OnRecvInternal(buf-msgHead._size, recvBuf);
                                                }
                                        }
                                }

                                std::weak_ptr<ThisType> weakSes = std::move(ses);
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

                        boost::fibers::fiber([ses]() {
                                constexpr std::size_t cSendBufInitSize = 1024 * 1024;
                                auto sendBufRef = std::make_shared<char[]>(cSendBufInitSize);
                                auto sendBuf = sendBufRef.get() + sizeof(MsgTotalHeadType);
                                std::size_t _bufSize = cSendBufInitSize;
                                std::size_t _end = sizeof(MsgTotalHeadType);
                                std::size_t _totalMsgSize = _end;

                                auto allocCacheBufferFunc = [&](std::size_t needSize) {
                                        // LOG_FATAL_IF(_totalMsgSize > _bufSize, "222222222222222222222 _totalMsgSize[{}] _bufSize[{}]", _totalMsgSize, _bufSize);
                                        new (sendBufRef.get()) MsgTotalHeadType(_totalMsgSize);
                                        ses->DoSend(sendBufRef, sendBufRef.get(), _totalMsgSize);

                                        // 网络压力很大的情况下，一次性分配8M。
                                        if (0 != needSize)
                                                needSize = std::max<int64_t>(needSize, cSendBufInitSize * 8);
                                        _bufSize = cSendBufInitSize > needSize ? cSendBufInitSize : needSize;
                                        sendBufRef = std::make_shared<char[]>(_bufSize);
                                        sendBuf = sendBufRef.get() + sizeof(MsgTotalHeadType);
                                        _end = sendBuf - sendBufRef.get();
                                        _totalMsgSize = _end;
                                };

                                stSessionSendBufInfo<MsgHeaderType> bufInfo;
                                while (!ses->IsTerminate())
                                {
                                        if (boost::fibers::channel_op_status::success == ses->_sendChannel.try_pop(bufInfo))
                                        {
__direct_deal__ :
                                                const auto msgHead = bufInfo._head;
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

                                                                std::size_t idsMsgSendSize = 0;
                                                                Compress::ECompressType sct = Compress::ECompressType::E_CT_None;
                                                                if (pbInfo->_msgExtra)
                                                                        std::tie(idsMsgSendSize, sct) = SuperType::SerializeAndCompress(*std::reinterpret_pointer_cast<google::protobuf::MessageLite>(pbInfo->_msgExtra), sendBuf+sizeof(typename MsgHeaderType::MsgMultiCastHeader)+msgSendSize+sizeof(uint64_t));
                                                                else
                                                                        LOG_WARN("E_SSBT_MultiCast pbInfo->_msgExtra is nullptr!!!");

                                                                *reinterpret_cast<uint64_t*>(sendBuf + sizeof(typename MsgHeaderType::MsgMultiCastHeader) + msgSendSize) = msgHead._to;
                                                                realSendSize = sizeof(typename MsgHeaderType::MsgMultiCastHeader) + msgSendSize + sizeof(uint64_t) + idsMsgSendSize;
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
                                std::weak_ptr<ThisType> weakSes = std::move(ses);
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

        void OnClose(int32_t reasonType) override
        {
                SuperType::OnClose(reasonType);

                boost::system::error_code ec;
                _socket.shutdown(boost::asio::socket_base::shutdown_both, ec);

                _recvChannel.push(nullptr);
                _sendChannel.push(stSessionSendBufInfo<MsgHeaderType>{});
        }

        void DoSend(const typename SuperType::BuffTypePtr& bufRef = nullptr, typename SuperType::BuffTypePtr::element_type* buf = nullptr, std::size_t size = 0)
        {
                std::lock_guard l(_bufListMutex);
                if (nullptr != buf && 0 != size)
                {
                        _bufList.emplace_back(boost::asio::const_buffer{ buf, size });
                        _bufRefList.emplace_back(bufRef);
                }

                // TODO:
                if (!_bufList.empty() && !SuperType::IsInSend())
                {
                        SuperType::SetInSend();
                        auto ses = ThisType::shared_from_this();
                        // _bufList 使用 std::move 无效。
                        boost::asio::async_write(_socket, _bufList, [ses, brl{ std::move(_bufRefList) }](const auto& ec, std::size_t size) {
                                /*
                                 * async_write 内部异步多部操作，因此不能同时存在多个 async_write 操作，
                                 * 必须等待返回后再次调用 async_write。
                                 */

                                ses->DelInSend();
                                if (!ec)
                                        ses->DoSend();
                                else
                                        ses->OnError(ec);
                        });

                        CLEAR_AND_CHECK_SIZE(_bufList, 4);
                        _bufRefList.reserve(4);
                }
        }

        void DoRecv()
        {
                auto ses = ThisType::shared_from_this();
                boost::asio::async_read(_socket, boost::asio::buffer((char*)&_msgTotalHead, sizeof(_msgTotalHead)),
                                        // boost::asio::transfer_at_least(sizeof(_totalMsgHead)),
                                        [ses](const auto& ec, std::size_t size) {
                                                if (!ec && sizeof(ses->_msgTotalHead) == size && ses->_msgTotalHead._size >= sizeof(MsgTotalHeadType))
                                                {
                                                        auto buf = std::make_shared<char[]>(ses->_msgTotalHead._size);
                                                        *reinterpret_cast<MsgTotalHeadType*>(buf.get()) = ses->_msgTotalHead;
                                                        boost::asio::async_read(ses->_socket, boost::asio::buffer(buf.get() + sizeof(MsgTotalHeadType), ses->_msgTotalHead._size - sizeof(MsgTotalHeadType)),
                                                                                // boost::asio::transfer_at_least(ses->_totalMsgHead._size - sizeof(MsgTotalHeadType)),
                                                                                [ses, buf](const auto& ec, std::size_t size) {
                                                                                        if (!ec && size == ses->_msgTotalHead._size - sizeof(MsgTotalHeadType))
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
                                                                                                LOG_WARN_IF(size>0, "async read body error ses id[{}] recvSize[{}] needSize[{}]"
                                                                                                            , ses->GetID(), size, ses->_msgTotalHead._size);
                                                                                                ses->OnError(ec);
                                                                                        }
                                                                                });
                                                }
                                                else
                                                {
                                                        LOG_WARN_IF(size>0, "async read head error ses id[{}] recvSize[{}] needSize[{}]"
                                                                    , ses->GetID(), size, ses->_msgTotalHead._size);
                                                        ses->OnError(ec);
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
                                    uint64_t mt, uint64_t st, uint64_t from, uint64_t to)
        {
                if (!idsMsg || idsMsg->id_list_size() <= 0)
                        return;

                auto msgByteSize = msg ? msg->ByteSizeLong() : 0;
                auto idsMsgByteSize = idsMsg->ByteSizeLong();

                auto info = std::make_shared<stSendRefWapper>();
                info->_msg = msg;
                info->_msgExtra = idsMsg;
                _sendChannel.push({
                        MsgHeaderType(sizeof(typename MsgHeaderType::MsgMultiCastHeader) + msgByteSize + Compress::CompressedSize<CompressType>(msgByteSize) + sizeof(uint64_t) + idsMsgByteSize + Compress::CompressedSize<CompressType>(idsMsgByteSize),
                                      Compress::ECompressType::E_CT_None, mt, st, MsgHeaderType::E_RMT_Send, 0, from, to),
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

private :
        friend class ::nl::net::NetMgrBase<_Tag>;
        SocketType _socket;

        boost::fibers::buffered_channel<ISession::BuffTypePtr> _recvChannel;
        boost::fibers::buffered_channel<stSessionSendBufInfo<MsgHeaderType>> _sendChannel;

        SpinLock _bufListMutex;
        std::vector<boost::asio::const_buffer> _bufList;
        std::vector<typename SuperType::BuffTypePtr> _bufRefList;
        
        MsgTotalHeadType _msgTotalHead;

        DECLARE_SHARED_FROM_THIS(ThisType);
};

}; // end of namespace nl::net::server

// vim: fenc=utf8:expandtab:ts=8:noma
