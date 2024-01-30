#pragma once

#include "ServiceImpl.hpp"

#include "Tools/ThreadSafeMap.hpp"
#include "Net/NetProcMgr.h"

uint32_t GenGuid()
{
        int64_t clientID = RandInRange(0, 1<<5);
        return RandInRange(0, 390624) << 8 | clientID << 3 | RandInRange(0, 1<<3);
}

template <typename _Sy, typename _My>
class ServiceAgent : public ServiceImpl
{
        typedef _Sy SesType;
        typedef _My ServiceMgrActorType;
        constexpr static int64_t scMgrActorCnt = 8;
        // F8    32个服务器
public :
        bool Init() override
        {
                for (int64_t i=0; i<scMgrActorCnt; ++i)
                {
                        _mgrActorArr[i] = std::make_shared<ServiceMgrActorType>();
                        _mgrActorArr[i]->Start();
                }
                return true;
        }

        void AddClient(const std::string& ip, uint16_t port)
        {
                auto proc = NetProcMgr::GetInstance()->Dist(++_idx);
                proc->Connect(ip, port, false, []() { return CreateSession<SesType>(); });
        }

        void AddServer(const std::string& ip, uint16_t port)
        {
                auto proc = NetProcMgr::GetInstance()->Dist(++_idx);
                proc->StartListener(ip, port, "", "", []() { return CreateSession<SesType>(); });

                auto clientID = id & 0xF8;
                (void)clientID;
        }

        virtual IActorPtr GetActor(uint64_t id)
        {
                // 本地找。
                // 所有client中找。
                return IActorPtr();
        }

        int64_t _idx = 0;
        ThreadSafeUnorderedMap<uint64_t, std::weak_ptr<SesType>> _sesList;

public :
        void Terminate() override
        {
                for (int64_t i=0; i<scMgrActorCnt; ++i)
                        _mgrActorArr[i]->Terminate();
                SuperType::Terminate();
        }

        void WaitForTerminate() override
        {
                for (int64_t i=0; i<scMgrActorCnt; ++i)
                        _mgrActorArr[i]->WaitForTerminate();
                SuperType::WaitForTerminate();
        }

public :
        __attribute__((always_inline)) std::shared_ptr<ServiceMgrActorType> GetMgrActor(int64_t id)
        { return _mgrActorArr[id & (scMgrActorCnt-1)]; }
private :
        std::shared_ptr<ServiceMgrActorType> _mgrActorArr[scMgrActorCnt];
};

// vim: fenc=utf8:expandtab:ts=8:noma
