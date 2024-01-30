#pragma once

namespace nl::af::impl {

#if 0
SPECIAL_ACTOR_DEFINE_BEGIN(ServiceDistcoveryActor, 0xffe);

public :
        bool Init() override
        {
                if (!SuperType::Init())
                        return false;

                auto ret = RedisCmd("INCR", "sid");
                if (!ret->IsNil())
                {
                        // GetAppBase()->_sid = ret->integer_;
                        // LOG_INFO("load sid:{}", GetAppBase()->_sid);
                        GetAppBase()->_startPriorityTaskList->Finish(_priorityTaskKey);
                }
                else
                {
                        LOG_FATAL("load sid fail!!!");
                }

                return true;
        }

        void RegistService(int64_t t, const std::string& ip, uint16_t port)
        {
                std::string cmd = fmt::format("HSET service:{} {} {}", t, GetAppBase()->GetSID(), fmt::format("{}:{}", ip, port));
                auto msg = std::make_shared<MsgRedisCmd>();
                msg->set_cmd(cmd);
                SendPush(1, msg);

                std::lock_guard l(_registServiceListMutex);
                _registServiceList.emplace(t, std::make_pair(ip, port));
        }

        void UnRegistService(int64_t t)
        {
                std::string cmd = fmt::format("HDEL service:{} {}", t, GetAppBase()->GetSID());
                auto msg = std::make_shared<MsgRedisCmd>();
                msg->set_cmd(cmd);
                SendPush(2, msg);

                std::lock_guard l(_registServiceListMutex);
                _registServiceList.erase(t);
        }

private :
        void AddDiscoveryTimer()
        {
                std::weak_ptr<ServiceDistcoveryActor> weakActor = shared_from_this();
                _timer.Start(weakActor, std::chrono::seconds(3), [weakActor]() {
                        auto act = weakActor.lock();
                        if (!act)
                                return;

                        std::string cmd = fmt::format("HGET service:{}", 1);
                        auto ret = act->RedisCmd(cmd, false);
                        if (ret)
                        {
                        }

                        act->AddDiscoveryTimer();
                });
        }

private :
        nl::util::SteadyTimer _timer;
        SpinLock _registServiceListMutex;
        std::map<int64_t, std::pair<std::string, uint16_t>> _registServiceList;

public :
        static std::string _priorityTaskKey;

SPECIAL_ACTOR_DEFINE_END(ServiceDistcoveryActor);

#endif

}; // end of namespace nl::af::impl

// vim: fenc=utf8:expandtab:ts=8:noma
