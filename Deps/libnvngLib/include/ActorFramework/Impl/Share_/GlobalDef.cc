#include "GlobalDef.h"
#include "Tools/LogHelper.h"

namespace nl::af::impl {

bool ReadRegionCfg(UnorderedMap<ERegionType, std::shared_ptr<RegionCfg>>& _list)
{
        std::string fileName = ServerCfgMgr::GetInstance()->GetConfigAbsolute("Region.txt");
        std::stringstream ss;
        if (!ReadFileToSS(ss, fileName))
                return false;

        int64_t idx = 0;
        int64_t tmpInt = 0;
        std::string tmpStr;
        _list.Clear();
        while (ReadTo(ss, "#"))
        {
                if (++idx <= 3)
                        continue;

                std::shared_ptr<RegionCfg> cfg = std::make_shared<RegionCfg>();
                ss >> tmpStr
                        >> tmpStr
                        >> tmpStr
                        >> tmpInt;
                cfg->set_region_type(static_cast<ERegionType>(tmpInt));

                // 人数限制。
                ss >> tmpStr;
                if ("-1" != tmpStr)
                {
                        auto tmpList = Tokenizer<int64_t>(tmpStr, "|");
                        LOG_FATAL_IF(2!=tmpList.size(), "文件[{}] 人数限制 配置错误!!!", fileName);
                        cfg->set_player_limit_min(tmpList[0]);
                        cfg->set_player_limit_max(tmpList[1]);
                }
                else
                {
                        cfg->set_player_limit_min(0);
                        cfg->set_player_limit_max(INT64_MAX);
                }

                // 机器人限制。
                ss >> tmpStr;
                if ("-1" != tmpStr)
                {
                        auto tmpList = Tokenizer<int64_t>(tmpStr, "|");
                        LOG_FATAL_IF(2!=tmpList.size(), "文件[{}] 机器人限制 配置错误!!!", fileName);
                        cfg->set_robot_limit_min(tmpList[0]);
                        cfg->set_robot_limit_max(tmpList[1]);
                }
                else
                {
                        cfg->set_robot_limit_min(0);
                        cfg->set_robot_limit_max(INT64_MAX);
                }

                // 机器人请求刷频率(多少秒请求一个)
                ss >> tmpStr;
                if ("-1" != tmpStr)
                {
                        auto tmpList = Tokenizer<int64_t>(tmpStr, "|");
                        LOG_FATAL_IF(2!=tmpList.size(), "文件[{}] 机器人请求刷频率(多少秒请求一个) 配置错误!!!", fileName);
                        cfg->set_robot_req_rate_min(tmpList[0]);
                        cfg->set_robot_req_rate_min(tmpList[1]);
                }
                else
                {
                        cfg->set_robot_req_rate_min(INT64_MAX);
                        cfg->set_robot_req_rate_min(INT64_MAX);
                }

                // 旁观人数限制
                ss >> tmpStr;
                if ("-1" != tmpStr)
                {
                        auto tmpList = Tokenizer<int64_t>(tmpStr, "|");
                        LOG_FATAL_IF(2!=tmpList.size(), "文件[{}] 旁观人数限制 配置错误!!!", fileName);
                        cfg->set_watcher_limit_min(tmpList[0]);
                        cfg->set_watcher_limit_max(tmpList[0]);
                }
                else
                {
                        cfg->set_watcher_limit_min(0);
                        cfg->set_watcher_limit_max(0);
                }

                // 场景状态时限
                ss >> tmpStr;
                if ("-1" != tmpStr)
                {
                        auto tmpList = Tokenizer<int64_t>(tmpStr, "|");
                        for (int64_t i : tmpList)
                                cfg->add_region_state_time_limit_list(i);
                }

                // 玩家状态时限
                ss >> tmpStr;
                if ("-1" != tmpStr)
                {
                        auto tmpList = Tokenizer<int64_t>(tmpStr, "|");
                        for (int64_t i : tmpList)
                                cfg->add_fighter_state_time_limit_list(i);
                }

                // 自定义参数1
                ss >> tmpStr;
                for (int64_t id : Tokenizer<int64_t>(tmpStr, "|"))
                        cfg->add_param_1_list(id);

                ss >> tmpStr;

                LOG_FATAL_IF(!CheckConfigEnd(ss, fileName),
                             "文件[{}] 读取错位，id[{}] 没检测到结束符 <end> !!!",
                             fileName, cfg->region_type());

                LOG_FATAL_IF(!_list.Add(cfg->region_type(), cfg),
                             "文件[{}] 唯一标识 type[{}] 重复!!!",
                             fileName, cfg->region_type());
        }

        return true;
}

bool ReadCompetitionCfg(UnorderedMap<int64_t, stCompetitionCfgInfoPtr>& _list)
{
        std::string fileName = ServerCfgMgr::GetInstance()->GetConfigAbsolute("Match.txt");
        std::stringstream ss;
        if (!ReadFileToSS(ss, fileName))
                return false;

        // auto now = GetClock().GetTimeStamp();
        auto parseTime = [](const auto& cfg, auto& timeInfo, const std::string& tmpStr, int64_t flag) {
                auto tmpList = Tokenizer<int64_t>(tmpStr, "|");
                LOG_FATAL_IF(tmpList.empty(), "");
                time_t cfgTime = 0;
                switch (tmpList.size())
                {
                case 1 : cfgTime = tmpList[0] * 3600; break;
                case 2 : cfgTime = tmpList[0] * 3600 + tmpList[1] * 60; break;
                case 3 : cfgTime = tmpList[0] * 3600 + tmpList[1] * 60 + tmpList[2]; break;
                default : cfgTime = -1; break;
                }

                auto zoneHour = GetClock().GetDay_Slow(0);
                auto zoneMin  = GetClock().GetMin_Slow(0);
                // auto timeDay = GetClock().TimeClear_Slow(now, Clock::E_CTT_DAY);

                auto ret = cfgTime
                        - (HOUR_TO_SEC(zoneHour) + MIN_TO_SEC(zoneMin))
                        + HOUR_TO_SEC(timeInfo->_timeZoneHour) + MIN_TO_SEC(timeInfo->_timeZoneMin);
                if (0 == flag)
                {
                        timeInfo->_openTime = ret;
                        if (3 == cfg->_openTimeType)
                                timeInfo->_openTime = 0;
                        LOG_INFO("id[{}] 赛区[{}] 时区[{}:{}] OpenTime[{}]",
                                 cfg->_id, timeInfo->_type, timeInfo->_timeZoneHour, timeInfo->_timeZoneMin,
                                 GetClock().GetTimeString_Slow(timeInfo->_openTime));
                }
                else
                {
                        timeInfo->_endTime = ret;
                        if (3 == cfg->_openTimeType)
                                timeInfo->_endTime = 24 * 3600;
                        LOG_INFO("id[{}] 赛区[{}] 时区[{}:{}] EndTime[{}]",
                                 cfg->_id, timeInfo->_type, timeInfo->_timeZoneHour, timeInfo->_timeZoneMin,
                                 GetClock().GetTimeString_Slow(timeInfo->_endTime));
                }
        };

        int64_t idx = 0;
        std::vector<int64_t> tmpArr[3];
        std::string tmpStr;
        _list.Clear();
        while (ReadTo(ss, "#"))
        {
                if (++idx <= 3)
                        continue;

                auto cfg = std::make_shared<stCompetitionCfgInfo>();
                ss >> cfg->_id
                        >> tmpStr
                        >> tmpStr
                        >> cfg->_type;

                for (int64_t i=0; i<2; ++i)
                {
                        ss >> tmpStr;
                        tmpArr[i] = Tokenizer<int64_t>(tmpStr, "|");
                }

                for (int64_t i=0; i<tmpArr[0].size(); ++i)
                {
                        auto timeInfo = std::make_shared<stCompetitionTimeInfo>();
                        timeInfo->_type = tmpArr[0][i];
                        timeInfo->_timeZoneHour = tmpArr[1][i] / 10000;
                        timeInfo->_timeZoneMin  = ((tmpArr[1][i] % 10000) / 10000.0) * 60;
                        cfg->_timeInfoList.Add(timeInfo->_type, timeInfo);
                }

                ss >> cfg->_regionType
                        >> cfg->_openTimeType
                        >> cfg->_openTimeTypeParam
                        ;

                ss >> tmpStr;
                cfg->_timeInfoList.Foreach([cfg, &parseTime, &tmpStr](auto& timeInfo) {
                        parseTime(cfg, timeInfo, tmpStr, 0);
                });

                ss >> tmpStr;
                cfg->_timeInfoList.Foreach([&cfg, &parseTime, &tmpStr](auto& timeInfo) {
                        parseTime(cfg, timeInfo, tmpStr, 1);
                });

                ss >> cfg->_interval
                        >> cfg->_maxFighterCnt
                        >> cfg->_promotionParam
                        ;

                for (int64_t i=0; i<3; ++i)
                {
                        ss >> tmpStr;
                        tmpArr[i] = Tokenizer<int64_t>(tmpStr, "|");
                }
                LOG_FATAL_IF(tmpArr[0].size() != tmpArr[1].size() || tmpArr[0].size() != tmpArr[2].size(), "");
                for (int64_t i=0; i<tmpArr[0].size(); ++i)
                {
                        auto costInfo = std::make_shared<stCompetitionSignUpCostInfo>();
                        costInfo->_type = tmpArr[0][i];
                        costInfo->_param = tmpArr[1][i];
                        costInfo->_cnt = tmpArr[2][i];
                        cfg->_signUpCostInfoList.emplace_back(costInfo);
                }

                for (int64_t i=0; i<2; ++i)
                {
                        ss >> tmpStr;
                        tmpArr[i] = Tokenizer<int64_t>(tmpStr, "|");
                }
                LOG_FATAL_IF(tmpArr[0].size() != tmpArr[1].size(), "");
                for (int64_t i=0; i<tmpArr[0].size(); ++i)
                        cfg->_rewardByRank.emplace(tmpArr[0][i], tmpArr[1][i]);

                cfg->_timeInfoList.Foreach([&cfg](auto& timeInfo) {
                        const auto oldEndTime = timeInfo->_endTime;
                        timeInfo->_endTime = timeInfo->_openTime + cfg->_interval * ((timeInfo->_endTime - timeInfo->_openTime) / cfg->_interval);
                        LOG_FATAL_IF(oldEndTime != timeInfo->_endTime, "");
                });

                cfg->_mapIDSelectorList.resize(3);
                for (int64_t i=0; i<cfg->_mapIDSelectorList.size(); ++i)
                {
                        tmpArr[0].clear();
                        tmpArr[1].clear();
                        ss >> tmpArr[0]
                                >> tmpArr[1];
                        LOG_FATAL_IF(tmpArr[0].empty() || tmpArr[0].size() != tmpArr[1].size(), "");

                        for (int64_t j=0; j<tmpArr[0].size(); ++j)
                        {
                                auto& rs = cfg->_mapIDSelectorList[i];
                                if (!rs)
                                        rs = std::make_shared<stRandomSelectType<int64_t>>(RandInRange<int64_t>);

                                const auto weight = tmpArr[1][j];
                                auto info = std::make_shared<const stRandomInfo<int64_t>>(weight, tmpArr[0][j]);
                                rs->Add(weight, info);
                        }
                }
                
                tmpArr[0].clear();
                tmpArr[1].clear();
                ss >> tmpArr[0]
                        >> tmpArr[1];
                LOG_FATAL_IF(tmpArr[0].empty() || tmpArr[0].size() != tmpArr[1].size(), "0[{}] 1[{}]", tmpArr[0].size(), tmpArr[1].size());
                int64_t sum = 0;
                for (int64_t i=0; i<tmpArr[0].size(); ++i)
                {
                        sum += tmpArr[1][i];
                        cfg->_robotAILevelList.emplace_back(std::make_pair(tmpArr[1][i], tmpArr[0][i]));
                }

                LOG_FATAL_IF(10000 != sum, "");

                LOG_FATAL_IF(!CheckConfigEnd(ss, fileName),
                             "文件[{}] 读取错位，id[{}] 没检测到结束符 <end> !!!",
                             fileName, cfg->_id);

                LOG_FATAL_IF(!_list.Add(cfg->_id, cfg),
                             "文件[{}] 唯一标识 type[{}] 重复!!!",
                             fileName, cfg->_id);
        }

        return true;
}

}; // end of namespace nl::af::impl

// vim: fenc=utf8:expandtab:ts=8
