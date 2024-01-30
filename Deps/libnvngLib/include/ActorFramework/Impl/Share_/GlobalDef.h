#pragma once

namespace nl::af::impl {

#define INIT_OPT() \
boost::program_options::options_description desc("Usage"); \
desc.add_options() \
("help,h", "produce help message") \
("daemon,d", "run as daemon") \
("silent,s", "start silent") \
("configdir,c", boost::program_options::value<std::string>(&(ServerCfgMgr::GetInstance()->_baseCfgDir))->default_value(""), "config dir") \
("logdir", boost::program_options::value<std::string>(&LogHelper::sLogDir)->default_value("./log"), "log dir") \
("loglevel", boost::program_options::value<int64_t>(&LogHelper::sLogLevel)->default_value(0), "log level") \
("logv", boost::program_options::value<int64_t>(&LogHelper::sLogV)->default_value(0), "log v") \
("logfv", boost::program_options::value<uint64_t>(&LogHelper::sLogFilterFlag)->default_value(0), "log filter level") \
("logmf", boost::program_options::value<uint64_t>(&LogHelper::sLogMaxFiles)->default_value(128), "log max files") \
("logms", boost::program_options::value<uint64_t>(&LogHelper::sLogMaxSize)->default_value(1024), "log max size per file"); \
\
boost::program_options::variables_map vm; \
boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm); \
boost::program_options::notify(vm); \
\
if (vm.count("help")) \
{ \
        std::cout << std::endl << desc << std::endl; \
        return 1; \
} \
\
if (vm.count("daemon")) GetAppBase()->SetDaemon(); \
if (vm.count("silent")) GetAppBase()->SetStartSilent()

template <typename _Ty>
bool ERegionType_GameValid(_Ty t)
{
  return ERegionType_IsValid(t)
    && E_RT_None != t
    && E_RT_MainCity != t;
}

template <typename _Ty>
bool EQueueType_GameValid(_Ty t)
{
        return EQueueType_IsValid(t)
                && E_QT_None != t;
}

#pragma pack(push,1)
struct MsgHeaderClient
{
        uint16_t _type = 0;
        uint32_t _size = 0;
        uint32_t _ct : 2 = 0;
        uint32_t _param : 30 = 0;

        MsgHeaderClient() = default;

        MsgHeaderClient(decltype(_size) size, Compress::ECompressType ct, uint64_t mainType, uint64_t subType)
                : _type(MsgTypeMerge(mainType, subType))
                  , _size(size)
                  , _ct(ct)
                  , _param(0)
        {
        }

        MsgHeaderClient(decltype(_size) size, Compress::ECompressType ct, uint64_t mainType, uint64_t subType, uint64_t param)
                : _type(MsgTypeMerge(mainType, subType))
                  , _size(size)
                  , _ct(ct)
                  , _param(param)
        {
        }

        FORCE_INLINE Compress::ECompressType CompressType() const { return static_cast<Compress::ECompressType>(_ct); }
        FORCE_INLINE uint64_t MainType() const { return MsgMainType(_type); }
        FORCE_INLINE uint64_t SubType() const { return MsgSubType(_type); }

        DEFINE_MSG_TYPE_MERGE(8);
};
#pragma pack(pop)

struct stGoodsItem
{
  uint64_t _cnt = 0;
  uint32_t _id = 0;
  uint32_t _type = 0;

  void Pack(MsgGoodsItem& item)
  {
    item.set_id(_id);
    item.set_num(_cnt);
    item.set_type(_type);
  }

  void UnPack(const MsgGoodsItem& item)
  {
    _id = item.id();
    _cnt = item.num();
    _type = item.type();
  }

  bool UnPack(const rapidjson::GenericValue<rapidjson::UTF8<>>& item)
  {
    if (!item.HasMember("id")
        || !item.HasMember("cnt")
        || !item.HasMember("type"))
      return false;

    _id = item["id"].GetUint64();
    _cnt = item["cnt"].GetUint64();
    _type = item["type"].GetUint64();
    return true;
  }
};
typedef std::shared_ptr<stGoodsItem> stGoodsItemPtr;

template <typename _Ty>
struct stRandomInfo
{
        stRandomInfo() = default;
        explicit stRandomInfo(int64_t w, _Ty& v)
                : _w(w), _v(v)
        {
        }

        int64_t _w = 0;
        _Ty _v = _Ty();

	DISABLE_COPY_AND_ASSIGN(stRandomInfo);
};

template <typename _Ty>
using stRandomSelectType = RandomSelect2<stRandomInfo<_Ty>, int64_t, NullMutex>;

bool ReadRegionCfg(UnorderedMap<ERegionType, std::shared_ptr<RegionCfg>>& _list);

struct stCompetitionSignUpCostInfo
{
        int64_t _type = 0;
        int64_t _param = 0;
        int64_t _cnt = 0;
};
typedef std::shared_ptr<stCompetitionSignUpCostInfo> stCompetitionSignUpCostInfoPtr;

struct stCompetitionTimeInfo
{
        int64_t _type = 0;
        int64_t _timeZoneHour = 0;
        int64_t _timeZoneMin = 0;
        int64_t _openTime = 0;
        int64_t _endTime = 0;
};
typedef std::shared_ptr<stCompetitionTimeInfo> stCompetitionTimeInfoPtr;

struct stCompetitionCfgInfo
{
        int64_t _id = 0;
        int64_t _type = 0;
        Map<int64_t, stCompetitionTimeInfoPtr> _timeInfoList;
        ERegionType _regionType = E_RT_None;
        int64_t _openTimeType = 0;
        int64_t _openTimeTypeParam = 0;
        int64_t _interval = 0;

        int64_t _maxFighterCnt = 0;
        int64_t _promotionParam = 0;

        std::vector<stCompetitionSignUpCostInfoPtr> _signUpCostInfoList;
        std::map<int64_t, int64_t> _rewardByRank; // 名次奖励。
        std::vector<std::shared_ptr<stRandomSelectType<int64_t>>> _mapIDSelectorList;
        std::vector<std::pair<int64_t, int64_t>> _robotAILevelList;
};
typedef std::shared_ptr<stCompetitionCfgInfo> stCompetitionCfgInfoPtr;

bool ReadCompetitionCfg(UnorderedMap<int64_t, stCompetitionCfgInfoPtr>& _list);

}; // end of namespace nl::af::impl

// vim: fenc=utf8:expandtab:ts=8:
