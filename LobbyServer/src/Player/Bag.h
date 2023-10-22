#pragma once

#include "msg_client.pb.h"
#include "msg_common.pb.h"
class Player;
typedef std::shared_ptr<Player> PlayerPtr;

struct stBagBufInfo
{
        int64_t _id = 0;
        time_t _endTime = 0;
        bool _active = false;

        void Clear()
        {
                _id = 0;
                _endTime = 0;
                _active = false;
        }

        void Pack(auto& msg) const
        {
                msg.set_id(_id);
                msg.set_end_time(_endTime);
                msg.set_active(_active);
        }

        void UnPack(const auto& msg)
        {
                _id = msg.id();
                _endTime = msg.end_time();
                _active = msg.active();
        }
};
typedef std::shared_ptr<stBagBufInfo> stBagBufInfoPtr;

class Player;
class Bag
{
public :
        void InitFromDB(const DBPlayerInfo& dbInfo);
        void Pack2DB(auto& dbInfo)
        {
                auto now = GetClock().GetTimeStamp();
                for (auto& val : _goodsList)
                {
                        auto endTime = std::get<2>(val.second);
                        if (0 == endTime || endTime > now)
                        {
                                auto item = dbInfo.add_goods_list();
                                item->set_id(val.first);
                                item->set_num(std::get<0>(val.second));
                                item->set_type(std::get<1>(val.second));
                                item->set_end_time(std::get<2>(val.second));
                        }
                }

                std::vector<EBagBuffType> removeList;
                removeList.reserve(2);
                for (auto& val : _bufList)
                {
                        std::vector<int64_t> removeInnerList;
                        removeInnerList.reserve(2);
                        removeList.clear();
                        auto msgList = dbInfo.add_bag_buf_list();
                        msgList->set_type(val.first);
                        for (auto& innVal : val.second)
                        {
                                auto& info = innVal.second;
                                if (info->_endTime > now)
                                {
                                        auto msgInfo = msgList->add_buf_list();
                                        info->Pack(*msgInfo);
                                }
                                else
                                {
                                        removeInnerList.emplace_back(info->_id);
                                }
                        }

                        for (auto id : removeInnerList)
                                val.second.erase(id);
                        if (val.second.empty())
                                removeList.emplace_back(val.first);
                }

                for (auto t : removeList)
                        _bufList.erase(t);

                for (auto& val : _bufList[E_BBT_Goods])
                {
                        auto info = val.second;
                        LOG_INFO("buffID[{}] active[{}] endTime[{}]",
                                 info->_id, info->_active, info->_endTime);
                }
        }

        void Pack2Client(MsgPlayerInfo& msg);
        int64_t GetAddRatio(EBagBuffType t);

        int64_t GetCnt(int64_t id);
        std::tuple<int64_t, int64_t, int64_t> Add(const std::shared_ptr<Player>& p, MsgPlayerChange& msg, int64_t id, int64_t cnt, int64_t type, ELogServiceOrigType logType, uint64_t logParam);
        std::tuple<int64_t, int64_t, int64_t> Del(const std::shared_ptr<Player>& p, int64_t id, int64_t cnt, ELogServiceOrigType logType, uint64_t logParam);

        bool Check(const std::vector<std::pair<int64_t, int64_t>>& costList);
        std::vector<std::tuple<int64_t, int64_t, int64_t>> Del(const std::shared_ptr<Player>& p, const std::vector<std::pair<int64_t, int64_t>>& costList, ELogServiceOrigType logType, uint64_t logParam);
        std::vector<std::tuple<int64_t, int64_t, int64_t>> CheckAndDel(const std::shared_ptr<Player>& p, const std::vector<std::pair<int64_t, int64_t>>& costList, ELogServiceOrigType logType, uint64_t logParam);

        std::vector<int64_t> GetActiveBufList();

private :
        friend class Player;
        std::unordered_map<int64_t, std::tuple<int64_t, int64_t, time_t>> _goodsList;
        std::unordered_map<EBagBuffType, std::unordered_map<int64_t, stBagBufInfoPtr>> _bufList;
};

struct stDropInfo
{
        int64_t _type = 0;
        int64_t _tParam = 0;
        int64_t _num = 0;
        int64_t _ratio = 0;
};
typedef std::shared_ptr<stDropInfo> stDropInfoPtr;

struct stDropCfgItem
{
        stDropCfgItem() : _randomInfo(RandInRange) {}

        int64_t _id = 0;
        std::vector<stDropInfoPtr> _dropList;
        bool _hasRandomDrop = false;
        stRandomSelectType<int64_t> _randomInfo;
};
typedef std::shared_ptr<stDropCfgItem> stDropCfgItemPtr;

struct stBagItemCfg
{
        int64_t _id = 0;
        int64_t _pinZhi = 0;
        int64_t _type = 0;
        int64_t _param = 0;
        int64_t _effectiveType = 0;
        int64_t _param_1 = 0;
        int64_t _price = 0;
};
typedef std::shared_ptr<stBagItemCfg> stBagItemCfgPtr;

class BagMgr : public Singleton<BagMgr>
{
        BagMgr() : _itemLinZhuRandomSelect(RandInRange<int64_t>) {}
        ~BagMgr() {}
        friend class Singleton<BagMgr>;

public :
        bool Init();

        bool ReadBagCfg();

        bool ReadDropCfg();
        EClientErrorType DoDropInternal(const std::vector<std::pair<int64_t, int64_t>>& idList,
                                        std::map<int64_t, std::pair<int64_t, int64_t>>& rewardList,
                                        int64_t deep);
        EClientErrorType DoDrop(const PlayerPtr& p,
                                MsgPlayerChange& msg,
                                const std::vector<std::pair<int64_t, int64_t>>& idList,
                                ELogServiceOrigType logType,
                                uint64_t logParam,
                                int64_t r = 0);

public :
        UnorderedMap<int64_t, int64_t> _itemIDByPos; // 灵珠。
        stRandomSelectType<int64_t> _itemLinZhuRandomSelect;

        UnorderedMap<int64_t, stBagItemCfgPtr> _bagItemCfgList;
        UnorderedMap<int64_t, stDropCfgItemPtr> _dropList;
};

// vim: fenc=utf8:expandtab:ts=8
