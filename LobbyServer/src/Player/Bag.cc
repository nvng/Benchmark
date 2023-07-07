#include "Bag.h"

#include "Player.h"

void Bag::InitFromDB(const google::protobuf::RepeatedPtrField<MsgGoodsItem>& msgGoodsList)
{
        _goodsList.clear();
        for (auto& item : msgGoodsList)
                _goodsList.emplace(item.id(), std::make_pair(item.num(), item.type()));
}

void Bag::Pack2DB(google::protobuf::RepeatedPtrField<MsgGoodsItem>& msgGoodsList) const
{
        for (auto& val : _goodsList)
        {
                auto item = msgGoodsList.Add();
                item->set_id(val.first);
                item->set_num(val.second.first);
                item->set_type(val.second.second);
        }
}

void Bag::Pack2Client(google::protobuf::RepeatedPtrField<MsgGoodsItem>& msgGoodsList) const
{
        Pack2DB(msgGoodsList);
}

int64_t Bag::GetCnt(int64_t id)
{
        auto it = _goodsList.find(id);
        return _goodsList.end()!=it ? it->second.first : 0;
}

std::tuple<int64_t, int64_t, int64_t> Bag::Add(int64_t id, int64_t cnt, int64_t type/*=0*/)
{
        std::tuple<int64_t, int64_t, int64_t> ret;
        if (cnt <= 0)
                return ret;

        auto it = _goodsList.find(id);
        if (_goodsList.end() != it)
        {
                it->second.first += cnt;
                return std::make_tuple(id, it->second.first, it->second.second);
        }
        else
        {
                _goodsList.emplace(id, std::make_pair(cnt, type));
                return std::make_tuple(id, cnt, type);
        }
}

std::tuple<int64_t, int64_t, int64_t> Bag::Del(int64_t id, int64_t cnt)
{
        std::tuple<int64_t, int64_t, int64_t> ret;
        if (cnt <= 0)
                return ret;

        auto it = _goodsList.find(id);
        if (_goodsList.end() != it)
        {
                if (it->second.first < cnt)
                        return ret;

                it->second.first -= cnt;
                ret = std::make_tuple(id, it->second.first, it->second.second);
                if (it->second.first <= 0)
                        _goodsList.erase(it);
                return ret;
        }
        return ret;
}

bool Bag::Check(const std::vector<std::pair<int64_t, int64_t>>& costList)
{
        for (auto& val : costList)
        {
                if (GetCnt(val.first) < val.second)
                        return false;
        }
        return true;
}

std::vector<std::tuple<int64_t, int64_t, int64_t>>
Bag::Del(const std::vector<std::pair<int64_t, int64_t>>& costList)
{
        std::vector<std::tuple<int64_t, int64_t, int64_t>> retList;
        retList.reserve(costList.size());
        for (auto& val : costList)
                retList.emplace_back(Del(val.first, val.second));
        return retList;
}

std::vector<std::tuple<int64_t, int64_t, int64_t>>
Bag::CheckAndDel(const std::vector<std::pair<int64_t, int64_t>>& costList)
{
        std::vector<std::tuple<int64_t, int64_t, int64_t>> retList;
        if (!Check(costList))
                return std::vector<std::tuple<int64_t, int64_t, int64_t>>();
        return Del(costList);
}

bool BagMgr::Init()
{
        return ReadBagCfg()
                && ReadDropCfg();
}

bool BagMgr::ReadBagCfg()
{
        std::string fileName = ServerCfgMgr::GetInstance()->GetConfigAbsolute("Item.txt");
        std::stringstream ss;
        if (!ReadFileToSS(ss, fileName))
                return false;

        int64_t idx = 0;
        std::string tmpStr;
        _itemIDByPos.Clear();
        _bagItemCfgList.clear();
        while (ReadTo(ss, "#"))
        {
                if (++idx <= 3)
                        continue;

                stBagItemCfgPtr cfg = std::make_shared<stBagItemCfg>();
                ss >> cfg->_id
                        >> tmpStr
                        >> tmpStr
                        >> tmpStr
                        >> cfg->_pinZhi
                        >> cfg->_type
                        >> cfg->_effectiveType
                        >> cfg->_param
                        >> cfg->_param_1
                        ;

                LOG_FATAL_IF(!_bagItemCfgList.emplace(cfg->_id, cfg).second,
                             "文件[{}] 唯一ID重复，id[{}]!!!",
                             fileName, cfg->_id);
                LOG_FATAL_IF(!CheckConfigEnd(ss, fileName),
                             "文件[{}] 读取错位，id[{}] 没检测到结束符 <end> !!!",
                             fileName, cfg->_id);
        }

        return true;
}


stBagItemCfgPtr BagMgr::GetBagItemCfg(int64_t id)
{
        auto it = _bagItemCfgList.find(id);
        return _bagItemCfgList.end()!=it ? it->second : nullptr;
}

bool BagMgr::ReadDropCfg()
{
        std::string fileName = ServerCfgMgr::GetInstance()->GetConfigAbsolute("Loots.txt");
        std::stringstream ss;
        if (!ReadFileToSS(ss, fileName))
                return false;

        std::map<int64_t, std::set<int64_t>> checkList;
        int64_t idx = 0;
        std::string tmpStr;
        std::vector<int64_t> tmpList;
        std::vector<int64_t> tmpList_1;
        _dropList.Clear();
        while (ReadTo(ss, "#"))
        {
                if (++idx <= 3)
                        continue;

                stDropCfgItemPtr cfg = std::make_shared<stDropCfgItem>();
                ss >> cfg->_id
                        >> tmpStr
                        >> tmpStr;
                tmpList = Tokenizer<int64_t>(tmpStr, "|");
                LOG_FATAL_IF(tmpList.empty(), "掉落表数组长度为0!!! id:{}", cfg->_id);
                for (int64_t t : tmpList)
                {
                        auto info = std::make_shared<stDropInfo>();
                        info->_type = t;
                        cfg->_dropList.emplace_back(info);
                }

                ss >> tmpStr;
                tmpList = Tokenizer<int64_t>(tmpStr, "|");
                LOG_FATAL_IF(tmpList.size()!=cfg->_dropList.size(), "掉落表数组长度不一样!!! id:{}", cfg->_id);
                for (int i=0; i<static_cast<int>(tmpList.size()); ++i)
                        cfg->_dropList[i]->_tParam = tmpList[i];

                ss >> tmpStr;
                tmpList = Tokenizer<int64_t>(tmpStr, "|");
                LOG_FATAL_IF(tmpList.size()!=cfg->_dropList.size(), "掉落表数组长度不一样!!! id:{}", cfg->_id);
                for (int i=0; i<static_cast<int>(tmpList.size()); ++i)
                        cfg->_dropList[i]->_num = tmpList[i];

                ss >> tmpStr;
                tmpList = Tokenizer<int64_t>(tmpStr, "|");
                LOG_FATAL_IF(tmpList.size()!=cfg->_dropList.size(), "掉落表数组长度不一样!!! id:{}", cfg->_id);
                for (int i=0; i<static_cast<int>(tmpList.size()); ++i)
                        cfg->_dropList[i]->_ratio = tmpList[i];

                for (auto& info : cfg->_dropList)
                {
                        switch (info->_type)
                        {
                        case 1 : // 1，货币 (1=金币 2=体力 3=元宝 4=日活跃 5=周活跃 6=外部经验)
                                LOG_FATAL_IF(info->_num<=0, "掉落表货币类型，数量不大于0!!! id:{}", cfg->_id);
                                break;
                        default :
                                break;
                        }
                }

                ss >> tmpStr;
                if ("0" != tmpStr)
                {
                        tmpList = Tokenizer<int64_t>(tmpStr, "|");
                        ss >> tmpStr;
                        tmpList_1 = Tokenizer<int64_t>(tmpStr, "|");
                        LOG_FATAL_IF(tmpList.size()!=tmpList_1.size(), "掉落表数组长度不一样!!! id:{}", cfg->_id);
                        cfg->_hasRandomDrop = !tmpList.empty();

                        std::set<int64_t> idList;
                        for (int i=0; i<static_cast<int>(tmpList.size()); ++i)
                        {
                                idList.emplace(tmpList[i]);
                                auto info = std::make_shared<const stRandomInfo<int64_t>>(tmpList_1[i], tmpList[i]);
                                cfg->_randomInfo.Add(tmpList_1[i], info);
                        }
                        checkList.emplace(cfg->_id, idList);
                }
                else
                {
                        ss >> tmpStr;
                }

                LOG_FATAL_IF(!_dropList.Add(cfg->_id, cfg),
                             "文件[{}] 唯一ID重复，id[{}]!!!",
                             fileName, cfg->_id);
                LOG_FATAL_IF(!CheckConfigEnd(ss, fileName),
                             "文件[{}] 读取错位，id[{}] 没检测到结束符 <end> !!!",
                             fileName, cfg->_id);
        }

        /* 
        for (auto& val : checkList)
        {
                std::set<int64_t> tmpList;
                int64_t idx = 0;
                DropCfgCheckItem(idx, checkList, val.first, tmpList);
        }
        */

        return true;
}

EClientErrorType BagMgr::DoDropInternal(const std::vector<std::pair<int64_t, int64_t>>& idList,
                                        std::map<int64_t, std::pair<int64_t, int64_t>>& rewardList,
                                        int64_t deep)
{
        LOG_ERROR_IF(deep>=3, "掉落表深度 deep:{}!!!", deep);
        auto addItem = [&rewardList](int64_t id, int64_t cnt, int64_t flag) {
                auto it = rewardList.find(id);
                if (rewardList.end() != it)
                        it->second.first += cnt;
                else
                        rewardList.emplace(id, std::make_pair(cnt, flag));
        };

        for (auto& val : idList)
        {
                auto cfg = _dropList.Get(val.first);
                if (!cfg)
                {
                        LOG_INFO("ddddddddddddddddddddddd drop:{} not found!!!", val.first);
                        return E_CET_CfgNotFound;
                }

                for (int64_t i=0; i<val.second; ++i)
                {
                        for (auto& info : cfg->_dropList)
                        {
                                if (RandInRange(0, 10000) >= info->_ratio)
                                        continue;

                                // LOG_INFO("bbbbbbbbbbbbbbbbbbbbb cfgID:{} id:{} num:{}", val.first, info->_tParam, info->_num);
                                switch (info->_type)
                                {
                                case 1 : // 1，货币 (1=金币 2=体力 3=元宝 4=日活跃 5=周活跃 6=外部经验)
                                case 2 :
                                        addItem(info->_tParam, info->_num, info->_type);
                                        break;
                                default:
                                        addItem(info->_tParam, info->_num, info->_type);
                                        break;
                                }
                        }

                        const auto& rInfo = cfg->_randomInfo.Get();
                        if (0 != rInfo._v)
                                DoDropInternal({{ rInfo._v, 1 }}, rewardList, deep+1);
                }
        }

        return E_CET_Success;
}

EClientErrorType BagMgr::DoDrop(const PlayerPtr& p,
                                MsgPlayerChange& msg,
                                const std::vector<std::pair<int64_t, int64_t>>& idList,
                                EPlayerLogModuleType from/* = static_cast<EPlayerLogModuleType>(E_LOG_MT_None)*/,
                                int64_t r/* = 0*/)
{
        std::map<int64_t, std::pair<int64_t, int64_t>> tmpList;
        auto errorType = DoDropInternal(idList, tmpList, 0);
        if (E_CET_Success != errorType)
                return errorType;

        for (auto& val : tmpList)
                p->AddDrop(msg, val.second.second, val.first, val.second.first * (1.0 + r / 10000.0), from);

        return E_CET_Success;
}

// vim: fenc=utf8:expandtab:ts=8
