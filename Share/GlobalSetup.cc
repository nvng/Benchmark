#include "GlobalSetup.h"
#include <cassert>

bool GlobalSetup_CH::Init()
{
        const std::string fileName = ServerCfgMgr::GetInstance()->GetConfigAbsolute("Global.txt");
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

        // 玩家修改昵称花费。
        ss >> tmpStr >> tmpStr >> tmpStr >> _changeNickNameCostList >> tmpStr;

        // 排行榜最大容量和最大显示。
        ss >> tmpStr >> tmpStr >> tmpStr >> valStr >> tmpStr;
        for (auto& str : Tokenizer(valStr, "_"))
        {
                auto tmpList = Tokenizer<int64_t>(str, "|");
                LOG_FATAL_IF(4 != tmpList.size() || !ERankType_IsValid(tmpList[0]), "排行榜最大容量和最大显示，格式错误!!!");
                _rankDefArr[tmpList[0]] = std::make_tuple(tmpList[1], tmpList[2], tmpList[3]);
        }

        // 排队间隔。
        std::vector<int64_t> tmpList;
        ss >> tmpStr >> tmpStr >> tmpStr >> tmpList >> tmpStr;
        assert(tmpList.size() >= 2);
        _queueIntervalList.resize(tmpList.size());
        for (int64_t i = 0; i<static_cast<int64_t>(tmpList.size()); ++i)
        {
                assert(tmpList[i] > 0);
                for (int64_t j=0; j<=i; ++j)
                        _queueIntervalList[i] += tmpList[j];
        }

        // 排队段位只匹配机器人。
        ss >> tmpStr >> tmpStr >> tmpStr >> _queueOnlyRobotRankList >> tmpStr;
        assert(_queueOnlyRobotRankList.size() >= 2);

        // 随机地图信息。
        ss >> tmpStr >> tmpStr >> tmpStr >> tmpStr;
        std::vector<int64_t> mapList;
        for (auto& str : Tokenizer<std::string>(tmpStr, "_"))
        {
                std::vector<int64_t> tmpList = Tokenizer<int64_t>(str, "|");
                assert(tmpList.size() >= 2);
                for (int64_t i=1; i<static_cast<int64_t>(tmpList.size()); ++i)
                        mapList.emplace_back(tmpList[i]);
                _randomMapInfoList[tmpList[0]] = mapList;
        }
        ss >> tmpStr;

        // 随机商店刷新费用列表。
        ss >> tmpStr >> tmpStr >> tmpStr >> tmpStr;
        _randomShopRefreshCostList = Tokenizer<int64_t>(tmpStr, "|");
        ss >> tmpStr;

        // 无尽模式复活消耗钻石。
        ss >> tmpStr >> tmpStr >> tmpStr >> _pveEndlessRebornCost >> tmpStr;

        // 随机商店每日刷新次数。
        ss >> tmpStr >> tmpStr >> tmpStr >> _randomShopRefreshCntDaily >> tmpStr;

        // 支付IP限制列表
        std::vector<std::string> strList;
        ss >> tmpStr >> tmpStr >> tmpStr >> strList >> tmpStr;
        for (auto& ip : strList)
                _payIPLimitList.emplace(ip);

        // 白名单及开关
        tmpList.clear();
        ss >> tmpStr >> tmpStr >> tmpStr >> tmpList >> tmpStr;
        if (!tmpList.empty())
        {
                _whiteListSwitch = tmpList[0];
                for (int64_t i=1; i<static_cast<int64_t>(tmpList.size()); ++i)
                        _whiteList.emplace(tmpList[i]);
        }

        return true;
}

// vim: fenc=utf8:expandtab:ts=8
