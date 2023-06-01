#pragma once

#include "RegionMgrBase.h"

class RegionMgr : public RegionMgrBase, public Singleton<RegionMgr>
{
	RegionMgr();
	~RegionMgr();
	friend class Singleton<RegionMgr>;

        typedef RegionMgrBase SuperType;
};

// vim: fenc=utf8:expandtab:ts=8
