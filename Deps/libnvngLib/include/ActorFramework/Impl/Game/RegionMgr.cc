#include "RegionMgr.h"

struct stRegionCreateInfo : public stActorMailBase
{
        uint64_t _guid = 0;
        GameMgrSession::ActorAgentTypePtr _agent;
        std::shared_ptr<MailRegionCreateInfo> _cInfo;
};

SPECIAL_ACTOR_MAIL_HANDLE(RegionMgrActor, E_MIGCST_RegionCreate, stRegionCreateInfo)
{
        auto pb = msg->_cInfo;
        auto agent = msg->_agent;
        REGION_DLOG_INFO(pb->region_guid(),
                         "RegionMgrActor 创建region[{}] id[{}] from[{}]",
                         pb->region_type(), pb->region_guid(), agent->GetID());

        auto retMsg = std::make_shared<MailResult>();
        retMsg->set_error_type(E_IET_Fail);

        do
        {
                auto ses = agent->GetSession();
                if (!ses)
                {
                        REGION_LOG_ERROR(pb->region_guid(),
                                         "req[{}] 请求创建region[{}] 时，ses already release!!!",
                                         agent->GetID(), pb->region_guid());
                        break;
                }

                auto region = RegionMgr::GetInstance()->GetActor(pb->region_guid());
                if (region)
                {
                        REGION_LOG_ERROR(pb->region_guid(),
                                         "req[{}] 请求创建region[{}] 时，region already exist!!!",
                                         agent->GetID(), pb->region_guid());
                        retMsg->set_error_type(E_IET_RegionCreateAlreadyExist);
                        break;
                }

                region = RegionMgr::GetInstance()->CreateRegion(pb, agent);
                if (!region)
                {
                        REGION_LOG_ERROR(pb->region_guid(),
                                         "req[{}] 请求创建region[{}] 时，region create nullptr!!!",
                                         agent->GetID(), pb->region_guid());
                        retMsg->set_error_type(E_IET_RegionCreateError);
                        break;
                }

                ++GetApp()->_regionCreateCnt;
                agent->BindActor(region);
                // region->Start(); // 已经在 CreateFunc 中 Start。
                ses->AddAgent(agent);

                retMsg->set_error_type(E_IET_Success);
        } while (0);

        RegionMgr::GetInstance()->_reqList.Remove(agent->GetID());
        return retMsg;
}

RegionMgr::RegionMgr()
	: SuperType("RegionMgr")
	  , _reqList("RegionMgr_reqList")
{
}

RegionMgr::~RegionMgr()
{
}

bool RegionMgr::Init()
{
	if (!SuperType::Init())
		return false;

        LOG_FATAL_IF(!nl::af::impl::ReadRegionCfg(_regionCfgList), "读取 RegionCfg.txt 配置错误!!!");
        LOG_FATAL_IF(!nl::af::impl::ReadCompetitionCfg(_competitionCfgList), "读取 Match.txt 配置错误!!!");
        GetRegionOptList().Foreach([](const auto& optInfo) {
                if (optInfo && optInfo->_initCfgFunc)
                        LOG_FATAL_IF(!optInfo->_initCfgFunc(), "配置读取错误!!!");
        });

        for (int64_t i=0; i<ERegionType_ARRAYSIZE; ++i)
        {
                if (!ERegionType_GameValid(i))
                        continue;

                auto cfg = _regionCfgList.Get(static_cast<ERegionType>(i));
                if (!cfg)
                        continue;

                auto mgr = std::make_shared<RegionMgrActor>(cfg);
                mgr->Start();
                _regionMgrActorArr[i] = mgr;
        }

        return true;
}

IActorPtr RegionMgr::CreateRegion(const std::shared_ptr<MailRegionCreateInfo>& cInfo,
                                  const GameMgrSession::ActorAgentTypePtr& agent)
{
	IActorPtr reg;
        auto cfg = _regionCfgList.Get(cInfo->region_type());
        if (!cfg)
        {
                REGION_LOG_ERROR(cInfo->region_guid(),
				 "创建 region 时，配置没找到!!! regionType[{}]",
				 cInfo->region_type());
                return reg;
        }

        auto createInfo = GetRegionOptList().Get(cInfo->region_type());
        if (!createInfo || !createInfo->_createRegionFunc)
        {
                REGION_LOG_ERROR(cInfo->region_guid(),
				 "创建 region 时，创建操作没找到!!! regionType[{}]",
				 cInfo->region_type());
                return reg;
        }
        return createInfo->_createRegionFunc(cInfo, cfg, agent);
}

bool RegionMgr::RegisterOpt(ERegionType regionType,
                         decltype(stRegionOptInfo::_initCfgFunc) initCfg,
                         decltype(stRegionOptInfo::_createRegionFunc) createRegion)
{
        if (ERegionType_GameValid(regionType))
        {
                auto info = std::make_shared<stRegionOptInfo>();
                info->_initCfgFunc = initCfg;
                info->_createRegionFunc = createRegion;
                return GetRegionOptList().Add(regionType, info);
        }
        return false;
}

NET_MSG_HANDLE(GameMgrSession, E_MIMT_GameCommon, E_MIGCST_RegionCreate, MailRegionCreateInfo)
{
	REGION_DLOG_INFO(msg->region_guid(),
			 "game mgr 请求创建region[{}] id[{}]",
			 msg->region_type(), msg->region_guid());
	if (RegionMgr::GetInstance()->_reqList.Add(msgHead._from))
	{
		auto agent = std::make_shared<GameMgrSession::ActorAgentType>(static_cast<int64_t>(msgHead._from), shared_from_this());
                auto mail = std::make_shared<stRegionCreateInfo>();
                mail->_cInfo = msg;
                mail->_agent = agent;

                RegionMgr::GetInstance()->DistRegionMgrActor(msg->region_type())->
                        CallPushInternal(agent, E_MIMT_GameCommon, E_MIGCST_RegionCreate, mail, msgHead._guid);
	}
	else
        {
                LOG_ERROR("GameMgr 请求创建 region[{}] 时已经有相同 from[{}] 在请求了!!!", msgHead._to, msgHead._from);
                auto ret = std::make_shared<MailResult>();
                ret->set_error_type(E_IET_AlreadyRequest);
                SendPB(ret, E_MIMT_GameCommon, E_MIGCST_RegionCreate,
                       MsgHeaderType::E_RMT_CallRet, msgHead._guid, msgHead._to, msgHead._from);
        }
}

NET_MSG_HANDLE(GameMgrSession, E_MIMT_GameCommon, E_MIGCST_RegionDestroy)
{
        auto agent = GetAgent(msgHead._from, msgHead._to);
        if (agent)
        {
                GameMgrSession::ActorAgentTypeWeakPtr weakAgent = agent;
                RegionMgr::GetInstance()->DistRegionMgrActor(static_cast<ERegionType>(agent->GetType()))
                        ->SendPush([weakAgent, msgHead]() {
                                auto agent = weakAgent.lock();
                                if (agent)
                                {
                                        auto act = agent->GetBindActor();
                                        if (act)
                                        {
                                                RegionMgr::GetInstance()->DistRegionMgrActor(static_cast<ERegionType>(act->GetType()))
                                                        ->Send(act, E_MIMT_GameCommon, E_MIGCST_RegionDestroy, nullptr);

                                                // 相同 region 的添加和删除操作，必须有时序性，
                                                // 因此必须由 ses 到 RegionMgrActor 中操作。
                                                RegionMgr::GetInstance()->RemoveActor(act);
                                                act->Terminate();
                                                // act->WaitForTerminate();

                                                ++GetApp()->_regionDestroyCnt;
                                        }
                                        else
                                        {
                                                REGION_LOG_INFO(msgHead._to, "region 已经被释放!!! from[{}] to[{}]", msgHead._from, msgHead._to);
                                        }

                                        auto retMsg = std::make_shared<MailResult>();
                                        retMsg->set_error_type(E_IET_Success);
                                        agent->CallRet(retMsg, msgHead._guid, E_MIMT_GameCommon, E_MIGCST_RegionDestroy);
                                }
                                else
                                {
                                        REGION_LOG_WARN(msgHead._to, "agent已经被释放!!! from[{}] to[{}]", msgHead._from, msgHead._to);
                                }
                        });
        }
        else
        {
                // 已经被删除。
                REGION_LOG_WARN(msgHead._to, "GameMgr 请求删除 region[{}] 时，已经被删除!!!", msgHead._to);
                auto retMsg = std::make_shared<MailResult>();
                retMsg->set_error_type(E_IET_Success);
                SendPB(retMsg, E_MIMT_GameCommon, E_MIGCST_RegionDestroy, MsgHeaderType::E_RMT_CallRet,
                       msgHead._guid, msgHead._to, msgHead._from);
        }
}

// vim: fenc=utf8:expandtab:ts=8
