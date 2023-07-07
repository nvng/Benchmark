#pragma once

class Player;
typedef std::shared_ptr<Player> PlayerPtr;

class Bag
{
public :
        void InitFromDB(const google::protobuf::RepeatedPtrField<MsgGoodsItem>& msgGoodsList);
        void Pack2DB(google::protobuf::RepeatedPtrField<MsgGoodsItem>& msgGoodsList) const;
        void Pack2Client(google::protobuf::RepeatedPtrField<MsgGoodsItem>& msgGoodsList) const;

        int64_t GetCnt(int64_t id);
        std::tuple<int64_t, int64_t, int64_t> Add(int64_t id, int64_t cnt, int64_t type=0);
        std::tuple<int64_t, int64_t, int64_t> Del(int64_t id, int64_t cnt);

        bool Check(const std::vector<std::pair<int64_t, int64_t>>& costList);
        std::vector<std::tuple<int64_t, int64_t, int64_t>> Del(const std::vector<std::pair<int64_t, int64_t>>& costList);
        std::vector<std::tuple<int64_t, int64_t, int64_t>> CheckAndDel(const std::vector<std::pair<int64_t, int64_t>>& costList);

private :
        std::unordered_map<int64_t, std::pair<int64_t, int64_t>> _goodsList;
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
};
typedef std::shared_ptr<stBagItemCfg> stBagItemCfgPtr;

class Player;
class BagMgr : public Singleton<BagMgr>
{
        BagMgr() : _itemLinZhuRandomSelect(RandInRange<int64_t>) {}
        ~BagMgr() {}
        friend class Singleton<BagMgr>;

public :
        bool Init();

        bool ReadBagCfg();
        stBagItemCfgPtr GetBagItemCfg(int64_t id);

        bool ReadDropCfg();
        EClientErrorType DoDropInternal(const std::vector<std::pair<int64_t, int64_t>>& idList,
                                        std::map<int64_t, std::pair<int64_t, int64_t>>& rewardList,
                                        int64_t deep);
        EClientErrorType DoDrop(const PlayerPtr& p,
                                MsgPlayerChange& msg,
                                const std::vector<std::pair<int64_t, int64_t>>& idList,
                                EPlayerLogModuleType from = static_cast<EPlayerLogModuleType>(E_LOG_MT_None),
                                int64_t r = 0);

public :
        UnorderedMap<int64_t, int64_t> _itemIDByPos; // 灵珠。
        stRandomSelectType<int64_t> _itemLinZhuRandomSelect;

        std::unordered_map<int64_t, stBagItemCfgPtr> _bagItemCfgList;
        UnorderedMap<int64_t, stDropCfgItemPtr> _dropList;
};

// vim: fenc=utf8:expandtab:ts=8
