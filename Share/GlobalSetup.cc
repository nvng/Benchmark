#include "GlobalSetup.h"

bool GlobalSetup_CH::Init()
{
        const std::string fileName = nl::af::impl::ServerCfgMgr::GetInstance()->GetConfigAbsolute("Global.txt");
        std::stringstream ss;
        if (!ReadFileToSS(ss, fileName))
                return false;

        std::string valStr;
        std::string tmpStr;
        for (int i=0; i<15; ++i)
                ss >> tmpStr;

        // 开服时间。
        ss >> tmpStr >> tmpStr >> tmpStr >> _openServerTime >> tmpStr;
        LOG_FATAL_IF(0 == _openServerTime, "开服时间配置错误!!!");
        _openServerHour = Clock::TimeClear_Slow(_openServerTime, Clock::E_CTT_HOUR);
        _openServerDay = Clock::TimeClear_Slow(_openServerTime, Clock::E_CTT_DAY);

        // 非零点数据刷新。
        ss >> tmpStr >> tmpStr >> tmpStr >> _dataResetNonZero >> tmpStr;
        assert(_dataResetNonZero > 0);

        return true;
}

// vim: fenc=utf8:expandtab:ts=8
