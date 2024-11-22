#pragma once

#include "ActorFramework/ServiceExtra.hpp"

#define GEN_GUID_SERVICE_INVALID_GUID UINT64_MAX

#ifdef GEN_GUID_SERVICE_SERVER

#include "msg_db.pb.h"

struct stGenGuidItem
{
        uint64_t _idx = 0;
        uint64_t _cur = 0;
        uint64_t _max = 0;

        void Pack(auto& msg)
        {
                msg.set_idx(_idx);
                msg.set_cur(_cur);
                msg.set_max(_max);
        }

        void UnPack(const auto& msg)
        {
                _idx = msg.idx();
                _cur = msg.cur();
                _max = msg.max();
        }
};
typedef std::shared_ptr<stGenGuidItem> stGenGuidItemPtr;

#endif

SPECIAL_ACTOR_DEFINE_BEGIN(GenGuidActor, E_MIMT_GenGuid);

#ifdef GEN_GUID_SERVICE_SERVER

public :
        GenGuidActor(int64_t idx, uint64_t minGuid, uint64_t maxGuid)
                : _idx(idx)
                  , _minGuid(minGuid)
                  , _maxGuid(maxGuid)
        {
                const uint64_t step = (maxGuid - minGuid + 1) / scInitArrSize;
                for (int64_t i=0; i<scInitArrSize; ++i)
                {
                        auto item = std::make_shared<stGenGuidItem>();
                        item->_idx = i;
                        item->_cur = std::min(_minGuid + i*step, _maxGuid);
                        if (scInitArrSize - 1 == i)
                                item->_max = _maxGuid;
                        else
                                item->_max = std::min(_minGuid + (i+1)*step, _maxGuid+1) - 1;
                        if (item->_cur < _maxGuid)
                        {
                                _itemList.emplace_back(item);
                                /*
                                LOG_INFO("1111111111 i[{}] idx[{}] _cur[{}] _max[{}] next[{}]"
                                         , i, item->_idx, item->_cur, item->_max, _minGuid+(i+1)*step);
                                         */
                        }

                }

                /*
                std::set<uint64_t> guidList;
                for (int64_t i=0; i<100000; ++i)
                {
                        auto guid = GenGuid();
                        if (GEN_GUID_SERVICE_INVALID_GUID == guid)
                                break;
                        if (!guidList.emplace(guid).second)
                                LOG_FATAL("2222222222222222 idx[{}] guid[{}]", idx, guid);
                }
                LOG_INFO("3333333333333333 size[{}] _itemList.size[{}]", guidList.size(), _itemList.size());
                for (auto& item : _itemList)
                        LOG_INFO("4444444444444 idx[{}] _cur[{}]", item->_idx, item->_cur);

                for (auto id : guidList)
                        LOG_INFO("ppppppppp id[{}]", id);
                        */
        }

        bool Init() override;
        void Save2DB();
        void Flush2DB();
        uint64_t GenGuid();

        const int64_t _idx = 0;
        const uint64_t _minGuid = 0;
        const uint64_t _maxGuid = 0;
        bool _inTimer = false;
        ::nl::util::SteadyTimer _timer;
        constexpr static int64_t scInitArrSize = 19;
        std::vector<stGenGuidItemPtr> _itemList;

#endif

SPECIAL_ACTOR_DEFINE_END(GenGuidActor);

DECLARE_SERVICE_BASE_BEGIN(GenGuid, SessionDistributeNull, ServiceSession);

private :
        GenGuidServiceBase() : SuperType("GenGuidService") { }
        ~GenGuidServiceBase() override { }

public :
        bool Init() override;

#ifdef GEN_GUID_SERVICE_SERVER

        template <typename ... Args>
        bool StartLocal(int64_t actCnt, Args ... args)
        {
                const auto& dbCfg = ServerCfgMgr::GetInstance()->_dbCfg;
                for (auto& item : dbCfg->_genGuidItemList)
                {
                        auto act = std::make_shared<typename SuperType::ActorType>(item->_idx, item->_minGuid, item->_maxGuid);
                        SuperType::_actorArr.emplace_back(act);
                        act->Start();
                }

                return true;
        }

        FORCE_INLINE auto GetActor(EGenGuidType t)
        { return SuperType::_actorArr[t].lock(); }
#endif

#ifdef GEN_GUID_SERVICE_CLIENT

        template <EGenGuidType _Type>
        FORCE_INLINE uint64_t GenGuid(const IActorPtr& act)
        {
                auto ses = SuperType::DistSession(0);
                if (!ses)
                        return GEN_GUID_SERVICE_INVALID_GUID;

                auto agent = std::make_shared<typename SessionType::ActorAgentType>(0, ses);
                agent->BindActor(act);
                ses->AddAgent(agent);

                auto msg = std::make_shared<MsgGenGuid>();
                msg->set_type(_Type);
                auto callRet = Call(MsgGenGuid, agent, E_MIMT_GenGuid, E_MIGGST_Req, msg);
                if (!callRet || E_IET_Success != callRet->error_type())
                {
                        LOG_WARN("actor[{}] 请求 GenGuid 失败，err[{}]"
                                 , act->GetID(), callRet ? callRet->error_type() : E_IET_CallTimeOut);
                        return GEN_GUID_SERVICE_INVALID_GUID;
                }
                return callRet->id();
        }

#endif

DECLARE_SERVICE_BASE_END(GenGuid);

#ifdef GEN_GUID_SERVICE_SERVER
typedef GenGuidServiceBase<E_ServiceType_Server, stLobbyServerInfo> GenGuidService;
#endif

#ifdef GEN_GUID_SERVICE_CLIENT
typedef GenGuidServiceBase<E_ServiceType_Client, stDBServerInfo> GenGuidService;
#endif

#ifdef GEN_GUID_SERVICE_LOCAL
typedef GenGuidServiceBase<E_ServiceType_Local, stServerInfoBase> GenGuidService;
#endif

// vim: fenc=utf8:expandtab:ts=8
