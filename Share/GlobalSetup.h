#pragma once

#include "Share/GlobalDef.h"
#include "msg_rank.pb.h"

class GlobalSetup_CH : public Singleton<GlobalSetup_CH>
{
        GlobalSetup_CH() {}
        ~GlobalSetup_CH() {}
        friend class Singleton<GlobalSetup_CH>;

public :
        bool Init();

public :
        time_t _openServerTime = 0;
        time_t _openServerHour = 0;
        time_t _openServerDay = 0;

        int64_t _dataResetNonZero = 0;
        std::vector<int64_t> _changeNickNameCostList;

        int64_t _gameSeasonMinBattleCntForReward = 0;
        int64_t _randomShopFreeRefreshInterval = 0;
        std::vector<int64_t> _randomShopRefreshCostList;

        std::tuple<int64_t, int64_t, int64_t> _rankDefArr[ERankType_ARRAYSIZE];

        std::vector<int64_t> _queueIntervalList;
        int64_t CalQueueIntervalCnt(int64_t diff)
        {
                if (diff < _queueIntervalList[0] || diff >= _queueIntervalList[_queueIntervalList.size()-2])
                        return 0;

                int64_t i = 1;
                for (; i<static_cast<int64_t>(_queueIntervalList.size()-1); ++i)
                {
                        if (diff < _queueIntervalList[i])
                                return i;
                }
                return i;
        }
        std::vector<int64_t> _queueOnlyRobotRankList;
        std::map<int64_t, std::vector<int64_t>> _randomMapInfoList;
        int64_t _pveEndlessRebornCost = 0;
        int64_t _randomShopRefreshCntDaily = 0;
        std::unordered_set<std::string> _payIPLimitList;

        bool _whiteListSwitch = false;
        std::unordered_set<uint64_t> _whiteList;
};

// vim: fenc=utf8:expandtab:ts=8
