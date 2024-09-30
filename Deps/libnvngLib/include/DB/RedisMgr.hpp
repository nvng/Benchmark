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
                  , _cmdList{bredis::single_command_t(std::forward<const Args&>(args)...)}
        {
        }

        stReplayMailInfo(const IActorPtr& act, bredis::command_container_t&& cmdList)
                : _act(act)
                  , _cmdList{std::move(cmdList)}
        {
        }

        FORCE_INLINE bool IsNil()
        {
                try
                {
                        if (_idx<0 || _cmdList.size()<=_idx)
                                return true;
                        boost::get<nil_t>(_resultList[_idx]);
                        ++_idx;
                        return true;
                }
                catch (...) { return false; }
        }

        FORCE_INLINE std::pair<std::string_view, bool> GetErr()
        {
                try
                {
                        if (_idx<0 || _cmdList.size()<=_idx)
                                return { "", true };
                        std::pair<std::string_view, bool> ret = { boost::get<error_t>(_resultList[_idx])._str, false };
                        ++_idx;
                        return ret;
                }
                catch (...) { return { "nil", true }; }
        }

        FORCE_INLINE std::pair<int64_t, bool> GetInt()
        {
                try
                {
                        if (_idx<0 || _cmdList.size()<=_idx)
                                return { 0, true };
                        std::pair<int64_t, bool> ret = { boost::get<bredis::extracts::int_t>(_resultList[_idx]), false };
                        ++_idx;
                        return ret;
                }
                catch (...) { return { 0, true }; }
        }
        
        FORCE_INLINE std::pair<std::string_view, bool> GetStr()
        {
                try
                {
                        if (_idx<0 || _cmdList.size()<=_idx)
                                return { "", true };
                        std::pair<std::string_view, bool> ret = { boost::get<std::string_view>(_resultList[_idx]), false };
                        ++_idx;
                        return ret;
                }
                catch (...) { return { GetErr().first, true }; }
        }

        FORCE_INLINE bool IsArr()
        {
                try { if (_idx<0 || _cmdList.size()<=_idx) return false; boost::get<array_holder_t>(_resultList[_idx]); return true; }
                catch (...) { return false; }
        }

        template <typename _Fy>
        FORCE_INLINE void ForeachArr(const _Fy& cb)
        {
                try
                {
                        if (_idx<0 || _cmdList.size()<=_idx)
                                return;

                        auto& arr = boost::get<array_holder_t>(_resultList[_idx]);
                        ++_idx;
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
        int64_t _idx = 0; // 操作失败，停留在原 idx 上，操作成功，后移。
        bredis::command_container_t _cmdList;
        std::vector<markers_result_t> _resultList;
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
        bool Init(const stRedisCfg& cfg, const std::string& flagName = "def")
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
                boost::fibers::fiber([idx]() {
                        ++RedisMgrBase::GetInstance()->_initCnt;

                        std::vector<stReplayMailInfoPtr> mailList;
                        mailList.reserve(10240);
                        bredis::command_container_t cmdList;
                        cmdList.reserve(10240);
                        auto runFunc = [&mailList, &cmdList]() {
                                auto conn = RedisMgrBase::GetInstance()->GetConn();
                                while (!RedisMgrBase::GetInstance()->IsTerminate())
                                {
                                        auto rxBackend = std::make_shared<std::string>();
                                        rxBackend->reserve(1024 * 1024 * 8);
                                        Buffer txBuf(*rxBackend);

                                        boost::system::error_code ec;
                                        conn->_conn->async_write(txBuf, cmdList, boost::fibers::asio::yield_t(ec));
                                        if (ec)
                                        {
                                                LOG_ERROR("redis async_write error!!! ec[{}] mailCnt[{}]", ec.what(), cmdList.size());
                                                conn->Reconnect();
                                                continue;
                                        }

                                        rxBackend->clear();
                                        Buffer rxBuf(*rxBackend);
                                        auto [r, readSize] = conn->_conn->async_read(rxBuf, boost::fibers::asio::yield_t(ec), cmdList.size());
                                        if (ec)
                                        {
                                                LOG_ERROR("redis async_read error!!! ec[{}] mailCnt[{}]", ec.what(), cmdList.size());
                                                conn->Reconnect();
                                                continue;
                                        }
                                        LOG_ERROR_IF(rxBackend->size() != readSize, "rxBackend size[{}] readSize[{}]", rxBackend->size(), readSize);

                                        if (1 != cmdList.size())
                                        {
                                                auto& arr = boost::get<bredis::markers::array_holder_t<Iterator>>(r);
                                                int64_t idx = 0;
                                                for (auto& mail : mailList)
                                                {
                                                        mail->_bufRef = rxBackend;
                                                        for (int64_t i=0; i<mail->_cmdList.size() && idx<arr.elements.size(); ++i, ++idx)
                                                                mail->_resultList.emplace_back(boost::apply_visitor(OuterExtractor<Iterator>(), arr.elements[idx]));

                                                        auto act = mail->_act.lock();
                                                        if (act)
                                                                act->CallRet(mail, 0, 0, 0);
                                                }
                                        }
                                        else
                                        {
                                                auto& mail = mailList[0];
                                                auto act = mail->_act.lock();
                                                if (act)
                                                {
                                                        mail->_resultList.emplace_back(boost::apply_visitor(OuterExtractor<Iterator>(), r));
                                                        mail->_bufRef = rxBackend;
                                                        act->CallRet(mail, 0, 0, 0);
                                                }
                                        }

                                        CLEAR_AND_CHECK_SIZE(cmdList, 10240);
                                        CLEAR_AND_CHECK_SIZE(mailList, 10240);

                                        break;
                                }
                        };

                        stReplayMailInfoPtr cmdInfo;
                        while (!RedisMgrBase::GetInstance()->IsTerminate())
                        {
                                if (boost::fibers::channel_op_status::success == RedisMgrBase::GetInstance()->_cmdQueueArr[idx]->try_pop(cmdInfo))
                                {
__direct_deal__ :
                                        if (cmdInfo)
                                        {
                                                mailList.emplace_back(cmdInfo);
                                                for (auto& cmd : cmdInfo->_cmdList)
                                                        cmdList.emplace_back(std::move(cmd));
                                        }
                                }
                                else
                                {
                                        if (!cmdList.empty())
                                        {
                                                runFunc();
                                                boost::this_fiber::sleep_for(std::chrono::microseconds(1));
                                        }
                                        else
                                        {
                                                RedisMgrBase::GetInstance()->_cmdQueueArr[idx]->pop(cmdInfo);
                                                goto __direct_deal__;
                                        }
                                }
                        }

                        if (--RedisMgrBase::GetInstance()->_initCnt <= 0)
                                FLAG_ADD(RedisMgrBase::GetInstance()->_internalFlag, 1 << E_DBMFT_Terminated);
                }).detach();
                });

                /*
                if (!_cfg._pwd.empty())
                        _cmdQueueArr[i]->push(std::make_shared<stReplayMailInfo>(nullptr, "AUTH", _cfg._pwd));
                _cmdQueueArr[i]->push(std::make_shared<stReplayMailInfo>(nullptr, "SELECT", _cfg._dbIdx));
                */
                }

                return true;
        }

        bool CreateConn() override
        {
                auto ep = boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(_cfg._ip), _cfg._port);
                while (true)
                {
                        boost::system::error_code ec;
                        boost::asio::ip::tcp::socket s(*DistCtx());
                        do
                        {
                                s.async_connect(ep, boost::fibers::asio::yield_t(ec));
                                if (ec)
                                {
                                        LOG_WARN("redis 连接失败!!! ec[{}]", ec.what());
                                        break;
                                }

                                auto conn = std::make_shared<ConnType>(std::move(s));

                                try
                                {
                                        if (!_cfg._pwd.empty())
                                        {
                                                if ("OK" != boost::get<std::string_view>(Exec(conn, {"AUTH", _cfg._pwd})))
                                                        break;
                                        }

                                        // LOG_INFO("11111111111111111111 idx[{}]", _cfg._dbIdx);
                                        if ("OK" != boost::get<std::string_view>(Exec(conn, {"SELECT", _cfg._dbIdx})))
                                                break;
                                }
                                catch (...)
                                {
                                        break;
                                }

                                return boost::fibers::channel_op_status::success == SuperType::_connQueue->try_push(conn);
                        } while (0);

                        boost::this_fiber::sleep_for(std::chrono::milliseconds(100));
                }
                return false;
        }

        FORCE_INLINE stReplayMailInfo::markers_result_t Exec(const bredis::single_command_t& cmd)
        {
                auto conn = RedisMgrBase::GetInstance()->GetConn();
                return Exec(conn->_conn, cmd);
        }

        template <typename ... Args>
        stReplayMailInfo::markers_result_t Exec(const auto& conn, const bredis::single_command_t& cmd)
        {
                try
                {
                        std::string rxBackend;
                        Buffer txBuf(rxBackend);
                        boost::system::error_code ec;
                        conn->async_write(txBuf, cmd, boost::fibers::asio::yield_t(ec));
                        if (ec)
                        {
                                return stReplayMailInfo::markers_result_t{};
                        }

                        rxBackend.clear();
                        Buffer rxBuf(rxBackend);
                        auto [r, readSize] = conn->async_read(rxBuf, boost::fibers::asio::yield_t(ec), 1);
                        if (ec)
                                return stReplayMailInfo::markers_result_t{};

                        return boost::apply_visitor(OuterExtractor<Iterator>(), r);
                }
                catch (...)
                {
                        return stReplayMailInfo::markers_result_t{};
                }
        }

        template <typename ... Args>
        FORCE_INLINE void Exec(const IActorPtr& act, const Args& ... args)
        {
                const auto idx = act ? (act->GetID() & (_cfg._connCnt-1)) : 0;
                _cmdQueueArr[idx]->push(std::make_shared<stReplayMailInfo>(act, std::forward<const Args&>(args)...));
        }

        FORCE_INLINE void Exec(const IActorPtr& act, bredis::command_container_t&& cmdList)
        {
                const auto idx = act ? (act->GetID() & (_cfg._connCnt-1)) : 0;
                _cmdQueueArr[idx]->push(std::make_shared<stReplayMailInfo>(act, std::move(cmdList)));
        }

        DEFINE_CONN_WAPPER(RedisMgrBase);

        void Terminate() override
        {
                SuperType::Terminate();
                for (int64_t i=0; i<_cfg._connCnt; ++i)
                        _cmdQueueArr[i]->push(nullptr);
        }

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
