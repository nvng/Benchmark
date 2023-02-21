#include "RegionMgr.h"

nl::af::impl::RegionMgrBase* nl::af::impl::GetRegionMgrBase()
{
        return RegionMgr::GetInstance();
}

RegionMgr::RegionMgr()
{
}

RegionMgr::~RegionMgr()
{
}

// vim: fenc=utf8:expandtab:ts=8
