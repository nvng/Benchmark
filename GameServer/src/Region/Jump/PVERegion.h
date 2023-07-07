#pragma once

#include "Region.h"

namespace Jump {

class PVERegion : public Region
{
        typedef Region SuperType;
public :
        PVERegion(const std::shared_ptr<MailRegionCreateInfo>& cInfo,
                  const std::shared_ptr<RegionCfg>& cfg,
                  const GameMgrSession::ActorAgentTypePtr& agent)
                : SuperType(cInfo, cfg, agent)
        {
        }
};

}; // end namespace Jump
