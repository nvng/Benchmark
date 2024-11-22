#pragma once

#include "DB/DBMgrBase.hpp"
#include "DB/bredis.hpp"
#include "ActorFramework/IActor.h"

namespace nl::db
{

using namespace nl::af;

using Buffer = boost::asio::dynamic_string_buffer<
std::string::value_type,
        std::string::traits_type,
        std::string::allocator_type
        >;

using Iterator = typename bredis::to_iterator<Buffer>::iterator_t;

struct stReplayMailInfo : public stActorMailBase, public boost::static_visitor<bredis::extracts::extraction_result_t>
{
        using Iterator = typename bredis::to_iterator<Buffer>::iterator_t;

        using int_t = int64_t;
        struct error_t { std::string_view _str; };
        struct nil_t { };
        using array_holder_t = bredis::markers::array_holder_t<Iterator>;

        using markers_result_t = boost::variant<int_t,
              std::string_view,
              error_t,
              nil_t,
              array_holder_t>;

        template <typename ... Args>
        stReplayMailInfo(const IActorPtr& act, const Args& ... args)
                : _act(act)
                  , _cmd(std::forward<const Args&>(args)...)
        {
        }

        FORCE_INLINE bool IsNil()
        {
                try { boost::get<nil_t>(_result); return true; }
                catch (...) { return false; }
        }

        FORCE_INLINE std::pair<std::string_view, bool> GetErr()
        {
                try { return { boost::get<error_t>(_result)._str, false }; }
                catch (...) { static std::string staticStr{ "nil" }; return { staticStr, true }; }
        }

        FORCE_INLINE std::pair<int64_t, bool> GetInt()
        {
                try { return { boost::get<bredis::extracts::int_t>(_result), false }; }
                catch (...) { return { 0, true }; }
        }
        
        FORCE_INLINE std::pair<std::string_view, bool> GetStr()
        {
                try { return { boost::get<std::string_view>(_result), false }; }
                catch (...) { return { GetErr().first, true }; }
        }

        FORCE_INLINE bool IsArr()
        {
                try { boost::get<array_holder_t>(_result); return true; }
                catch (...) { return false; }
        }

        template <typename _Fy>
        FORCE_INLINE void ForeachArr(const _Fy& cb)
        {
                try
                {
                        auto& arr = boost::get<array_holder_t>(_result);
                        for (int64_t i=0; i<arr.elements.size(); i += 2)
                        {
                                auto& markKey = boost::get<bredis::markers::string_t<Iterator>>(arr.elements[i]);
                                auto keyBegin = &(*(markKey.from));
                                auto keyEnd   = &(*(markKey.to));

                                auto& markVal = boost::get<bredis::markers::string_t<Iterator>>(arr.elements[i]);
                                auto valBegin = &(*(markVal.from));
                                auto valEnd   = &(*(markVal.to));

                                cb(std::string_view(keyBegin, std::size_t(keyEnd-keyBegin)),
                                   std::string_view(valBegin, std::size_t(valEnd-valBegin)));
                        }
                }
                catch (...)
                {
                }
        }

        IActorWeakPtr _act;
        bredis::single_command_t _cmd = { "" };
        markers_result_t _result;
        std::shared_ptr<std::string> _bufRef;
};
typedef std::shared_ptr<stReplayMailInfo> stReplayMailInfoPtr;

using namespace bredis;

template <typename Iterator>
struct OuterExtractor : public boost::static_visitor<stReplayMailInfo::markers_result_t>
{
        stReplayMailInfo::markers_result_t operator()(const markers::string_t<Iterator> &value) const {
                auto begin = &(*(value.from));
                auto end = &(*(value.to));
                return std::string_view{ begin, std::size_t(end-begin) };
        }

        stReplayMailInfo::markers_result_t operator()(const markers::error_t<Iterator> &value) const {
                auto begin = &(*(value.string.from));
                auto end = &(*(value.string.to));
                LOG_ERROR("redis error[{}]", std::string(value.string.from, value.string.to));
                return stReplayMailInfo::error_t{ ._str{ begin, std::size_t(end-begin) } };
        }

        stReplayMailInfo::markers_result_t operator()(const markers::int_t<Iterator> &value) const {
                std::string str;
                auto size = std::distance(value.string.from, value.string.to);
                str.reserve(size);
                str.append(value.string.from, value.string.to);
                return extracts::int_t{boost::lexical_cast<extracts::int_t>(str)};
        }

        stReplayMailInfo::markers_result_t operator()(const markers::nil_t<Iterator> & /*value*/) const {
                return stReplayMailInfo::nil_t{};
        }

        stReplayMailInfo::markers_result_t operator()(const markers::array_holder_t<Iterator> &value) const {
                return value;
        }
};

template <typename _Tag>
class RedisMgrBase : public DBMgrBase<bredis::Connection<boost::asio::ip::tcp::socket>>, public Singleton<RedisMgrBase<_Tag>>
{
        typedef bredis::Connection<boost::asio::ip::tcp::socket> ConnType;
        typedef DBMgrBase<ConnType> SuperType;
        typedef boost::fibers::buffered_channel<stReplayMailInfoPtr> CmdQueueType;

protected :
        friend class Singleton<RedisMgrBase>;

public :
        RedisMgrBase()
        {
        }

        ~RedisMgrBase() override
        {
                delete[] _cmdQueueArr;
                _cmdQueueArr = nullptr;
        }

        using SuperType::Init;
        bool Init(const stRedisCfg& cfg, const std::string& flagName = "default")
        {
                if (sRedisMgrInit.test_and_set())
                {
                        LOG_FATAL("RedisMgrBase init twice!!!");
                        return false;
                }

                _cfg = cfg;
                _markString = "redis_" + flagName;
                if (!SuperType::Init(_cfg._thCnt, _cfg._connCnt))
                        return false;

                _cmdQueueArr = new std::shared_ptr<CmdQueueType>[_cfg._connCnt];
                for (int64_t i=0; i<_cfg._connCnt; ++i)
                {
                _cmdQueueArr[i] = std::make_shared<CmdQueueType>(1 << 15);

                GetAppBase()->_mainChannel.push([idx{i}]() {
                boost::fibers::fiber(
                     std::allocator_arg,
                     boost::fibers::protected_fixedsize_stack{ 32 * 1024 },
                     // boost::fibers::segmented_stack{},
                     [idx]() {
                        std::vector<stReplayMailInfoPtr> mailList;
                        mailList.reserve(1024);
                        bredis::command_container_t cmdList;
                        cmdList.reserve(1024);
                        auto runFunc = [&mailList, &cmdList]() {
                                auto conn = RedisMgrBase::GetInstance()->GetConn();
                                while (!RedisMgrBase::GetInstance()->IsTerminate())
                                {
                                        auto rxBackend = std::make_shared<std::string>();
                                        rxBackend->reserve(1024 * 1024 * 8);
                                        Buffer txBuf(*rxBackend);

                                        boost::system::error_code ec;
                                        conn->_conn->async_write(txBuf, std::move(cmdList), boost::fibers::asio::yield_t(ec));
                                        if (ec)
                                        {
                                                LOG_INFO("redis async_write error!!! ec[{}] mailCnt[{}]", ec.what(), mailList.size());
                                                conn->Reconnect();
                                                continue;
                                        }

                                        rxBackend->clear();
                                        Buffer rxBuf(*rxBackend);
                                        auto [r, readSize] = conn->_conn->async_read(rxBuf, boost::fibers::asio::yield_t(ec), mailList.size());
                                        if (ec)
                                        {
                                                LOG_INFO("redis async_read error!!! ec[{}] mailCnt[{}]", ec.what(), mailList.size());
                                                conn->Reconnect();
                                                continue;
                                        }
                                        LOG_FATAL_IF(rxBackend->size() != readSize, "");
                                        // LOG_INFO("1111111 [{}]", fmt::ptr(rxBackend->c_str()));
                                        // PrintBit(rxBackend->c_str(), 10);

                                        if (1 != mailList.size())
                                        {
                                                auto& arr = boost::get<bredis::markers::array_holder_t<Iterator>>(r);
                                                for (int64_t i=0; i<arr.elements.size(); ++i)
                                                {
                                                        auto& mail = mailList[i];
                                                        auto act = mail->_act.lock();
                                                        if (act)
                                                        {
                                                                mail->_result = boost::apply_visitor(OuterExtractor<Iterator>(), arr.elements[i]);
                                                                mail->_bufRef = rxBackend;
                                                                act->CallRet(mail, 0, 0, 0);
                                                        }
                                                }
                                        }
                                        else
                                        {
                                                auto& mail = mailList[0];
                                                auto act = mail->_act.lock();
                                                if (act)
                                                {
                                                        mail->_result = boost::apply_visitor(OuterExtractor<Iterator>(), r);
                                                        mail->_bufRef = rxBackend;
                                                        act->CallRet(mail, 0, 0, 0);
                                                }
                                        }

                                        std::exchange(mailList, {});
                                        break;
                                }
                        };

                        stReplayMailInfoPtr cmdInfo;
                        while (!FLAG_HAS(RedisMgrBase::GetInstance()->_internalFlag, 1 << E_DBMFT_Terminate))
                        {
                                if (boost::fibers::channel_op_status::success == RedisMgrBase::GetInstance()->_cmdQueueArr[idx]->try_pop(cmdInfo))
                                {
__direct_deal__ :
                                        if (cmdInfo && !cmdInfo->_cmd.arguments.empty())
                                        {
                                                mailList.emplace_back(cmdInfo);
                                                cmdList.emplace_back(std::move(cmdInfo->_cmd));
                                        }
                                }
                                else
                                {
                                        if (!cmdList.empty())
                                        {
                                                runFunc();
                                                boost::this_fiber::sleep_for(std::chrono::microseconds(100));
                                        }
                                        else
                                        {
                                                RedisMgrBase::GetInstance()->_cmdQueueArr[idx]->pop(cmdInfo);
                                                goto __direct_deal__;
                                        }
                                }
                        }
                }).detach();
                });

                if (!_cfg._pwd.empty())
                        _cmdQueueArr[i]->push(std::make_shared<stReplayMailInfo>(nullptr, "AUTH", _cfg._pwd));
                _cmdQueueArr[i]->push(std::make_shared<stReplayMailInfo>(nullptr, "SELECT", _cfg._dbIdx));
                }

                return true;
        }

        bool CreateConn() override
        {
                boost::system::error_code ec;
                auto ep = boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(_cfg._ip), _cfg._port);

                while (true)
                {
                        boost::asio::ip::tcp::socket s(*SuperType::_ioCtx);
                        s.async_connect(ep, boost::fibers::asio::yield_t(ec));
                        if (!ec)
                        {
                                auto conn = std::make_shared<ConnType>(std::move(s));
                                return boost::fibers::channel_op_status::success == SuperType::_connQueue->try_push(conn);
                        }
                        else
                        {
                                LOG_WARN("redis 连接失败!!! ec[{}]", ec.what());
                                boost::this_fiber::sleep_for(std::chrono::milliseconds(100));
                                ec.clear();
                        }
                }
                return false;
        }

        template <typename ... Args>
        FORCE_INLINE void Exec(const IActorPtr& act, const Args& ... args)
        {
                const auto idx = act ? (act->GetID() & (_cfg._connCnt-1)) : 0;
                _cmdQueueArr[idx]->push(std::make_shared<stReplayMailInfo>(act, std::forward<const Args&>(args)...));
        }

        DEFINE_CONN_WAPPER(RedisMgrBase);

private :
        stRedisCfg _cfg;
        std::shared_ptr<CmdQueueType>* _cmdQueueArr = nullptr;
        static std::atomic_flag sRedisMgrInit;
};

template <typename _Tag>
std::atomic_flag RedisMgrBase<_Tag>::sRedisMgrInit = ATOMIC_FLAG_INIT;

typedef RedisMgrBase<stDefaultTag> RedisMgr;

}; // end of namespace nl::db

// vim: fenc=utf8:expandtab:ts=8:noma
