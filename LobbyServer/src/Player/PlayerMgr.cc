#include "PlayerMgr.h"

#include "Player.h"

nl::af::impl::PlayerMgrBase* nl::af::impl::GetPlayerMgrBase()
{
        return PlayerMgr::GetInstance();
}

PlayerMgr::PlayerMgr()
{
}

PlayerMgr::~PlayerMgr()
{
        delete[] _lvExpArr;
        _lvExpArr = nullptr;
}

bool PlayerMgr::Init()
{
	if (!SuperType::Init())
		return false;
	return ReadLevelUpCfg();
}

nl::af::impl::PlayerBasePtr PlayerMgr::CreatePlayer(uint64_t id, const std::string& nickName)
{
        return std::make_shared<Player>(id, nickName);
}

bool PlayerMgr::ReadLevelUpCfg()
{
        auto fileName = nl::af::impl::ServerCfgMgr::GetInstance()->GetConfigAbsolute("PlayerLevel.txt");
        std::stringstream ss;
        if (!ReadFileToSS(ss, fileName))
                return false;

        std::vector<std::pair<int32_t, nl::af::impl::stPlayerLevelInfoPtr>> tmpList;
        int64_t idx = 0;
        std::string tmpStr;
        while (ReadTo(ss, "#"))
        {
                if (++idx <= 3)
                        continue;

                auto cfg = std::make_shared<nl::af::impl::stPlayerLevelInfo>();
                ss >> tmpStr
                        >> tmpStr
                        >> cfg->_lv
                        >> cfg->_exp;
                if (0 == cfg->_exp)
                        cfg->_exp = INT64_MAX;
                if (tmpList.empty())
                {
                        LOG_FATAL_IF(1 != cfg->_lv, "玩家等级表每一行必须是1!!! lv:{}", cfg->_lv);
                }
                else
                {
                        LOG_FATAL_IF(tmpList[tmpList.size()-1].first+1 != cfg->_lv, "玩家等级表必须连续!!! lv:{}", cfg->_lv);
                }
                tmpList.emplace_back(std::make_pair(cfg->_lv, cfg));

                LOG_FATAL_IF(!CheckConfigEnd(ss, fileName),
                             "文件[{}] 读取错位，idx[{}] 没检测到结束符 <end> !!!",
                             fileName, idx);
        }

        LOG_FATAL_IF(tmpList.empty(), "玩家等级表没有可用数据!!!");

        delete[] _lvExpArr;

        _lvExpArrMax = tmpList.size();
        _lvExpArr = new nl::af::impl::stPlayerLevelInfoPtr[tmpList.size()];
        for (auto& val : tmpList)
                _lvExpArr[val.first-1] = val.second;

        return true;
}

// vim: fenc=utf8:expandtab:ts=8
