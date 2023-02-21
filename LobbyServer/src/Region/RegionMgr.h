#pragma once

#include "RegionMgrBase.h"

class RegionMgr : public nl::af::impl::RegionMgrBase, public Singleton<RegionMgr>
{
	RegionMgr();
	~RegionMgr();
	friend class Singleton<RegionMgr>;

        typedef nl::af::impl::RegionMgrBase SuperType;
};

// vim: fenc=utf8:expandtab:ts=8
