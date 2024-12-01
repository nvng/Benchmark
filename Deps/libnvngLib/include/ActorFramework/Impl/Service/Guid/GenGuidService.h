#pragma once

#include "ActorFramework/ServiceExtra.hpp"

#define GEN_GUID_SERVICE_INVALID_GUID UINT64_MAX

#ifdef GEN_GUID_SERVICE_SERVER

#include "msg_db.pb.h"

struct stGenGuidItem
{
        int64_t _cur = 0;
        int64_t _max = 0;

        void Pack(auto& msg)
        {
                msg.set_cur(_cur);
                msg.set_max(_max);
                // LOG_INFO("22222222 Pack idx[{}] cur[{}] max[{}]", msg.idx(), msg.cur(), msg.max());
        }

        void UnPack(const auto& msg)
        {
                _cur = msg.cur();
                _max = msg.max();
                // LOG_INFO("33333333 UnPack idx[{}] cur[{}] max[{}]", _idx, _cur, _max);
        }
};
typedef std::shared_ptr<stGenGuidItem> stGenGuidItemPtr;

#endif

SPECIAL_ACTOR_DEFINE_BEGIN(GenGuidActor, E_MIMT_GenGuid);

#ifdef GEN_GUID_SERVICE_SERVER

public :
        GenGuidActor(int64_t idx, uint64_t minGuid, uint64_t maxGuid)
                : _idx(idx)
        {
                const int64_t step = (maxGuid - minGuid + 1) / scInitArrSize;
                for (int64_t i=0; i<scInitArrSize; ++i)
                {
                        auto item = std::make_shared<stGenGuidItem>();
                        item->_cur = std::min(minGuid + i*step, maxGuid);
                        if (scInitArrSize - 1 == i)
                                item->_max = maxGuid;
                        else
                                item->_max = std::min(minGuid + (i+1)*step, maxGuid+1) - 1;
                        if (item->_cur < maxGuid)
                        {
                                _itemList.emplace_back(item);
                                /*
                                LOG_INFO("1111111111 i[{}] idx[{}] _cur[{}] _max[{}] next[{}]"
                                         , i, item->_idx, item->_cur, item->_max, minGuid+(i+1)*step);
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

        void Terminate() override;

        bool _inTimer = false;
        int64_t _idx = 0;
        ::nl::util::SteadyTimer _timer;
        constexpr static int64_t scInitArrSize = 127;
        std::vector<stGenGuidItemPtr> _itemList;

#endif

SPECIAL_ACTOR_DEFINE_END(GenGuidActor);

DECLARE_SERVICE_BASE_BEGIN(GenGuid, SessionDistributeMod, ServiceSession);

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
                for (auto& val : dbCfg->_genGuidItemList)
                {
                        auto item = val.second;
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

        template <typename _Ay, EGenGuidType _Type>
        FORCE_INLINE uint64_t GenGuid(const std::shared_ptr<_Ay>& act)
        {
                auto ses = SuperType::DistSession(0);
                if (!ses)
                        return GEN_GUID_SERVICE_INVALID_GUID;

                auto agent = std::make_shared<typename SessionType::ActorAgentType>(0, ses);
                agent->BindActor(act);
                ses->AddAgent(agent);

                auto msg = std::make_shared<MsgGenGuid>();
                msg->set_type(_Type);
                auto callRet = Call(MsgGenGuid, act, agent, E_MIMT_GenGuid, E_MIGGST_Req, msg);
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
