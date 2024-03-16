#include "GlobalDef.h"

void DropCfgCheckItem(int64_t& idx,
                      std::map<int64_t, std::set<int64_t>>& tList,
                      int64_t id,
                      std::set<int64_t>& tmpList)
{
        if (0 == id)
                return;

        ++idx;
        LOG_FATAL_IF(idx >= 4, "掉落表引用层数大于4!!!");

        auto it = tList.find(id);
        LOG_FATAL_IF(tList.end()==it, "id:{} 在掉落表中未找到!!!", id);

        for (int64_t i : it->second)
        {
                auto idList = tmpList;
                if (!idList.emplace(i).second)
                {
                        std::string str;
                        for (int64_t t : tmpList)
                                str += fmt::format("{}_", t);
                        if (!str.empty())
                                str.pop_back();
                        LOG_FATAL("出现循环引用!!! data:{}", str);
                }

                DropCfgCheckItem(idx, tList, i, idList);
        }
}

// vim: fenc=utf8:expandtab:ts=8
