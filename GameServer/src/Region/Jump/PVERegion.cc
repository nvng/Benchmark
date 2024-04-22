#include "PVERegion.h"

namespace Jump {

#define REGISTER_REGION(rt) \
AutoRelease ar_##rt([]() { \
        RegionMgr::GetInstance()->RegisterOpt(E_RT_##rt, []() { \
                return PVERegion::InitOnce(); \
        }, \
        [](const std::shared_ptr<MailRegionCreateInfo>& cInfo, const std::shared_ptr<RegionCfg>& cfg, const GameMgrSession::ActorNetAgentTypePtr& agent) { \
                auto ret = std::make_shared<PVERegion>(cInfo, cfg, agent); \
                ret->Start(); \
                return ret; \
        }); \
}, \
[]() { })

REGISTER_REGION(PVE);

#undef REGISTER_REGION

}; // end namespace Jump
