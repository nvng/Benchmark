#pragma once

#include "FighterImpl.hpp"
#include "msg_jump.pb.h"

namespace Jump {

class Fighter;
typedef std::shared_ptr<Fighter> FighterPtr;

class Region;
typedef std::shared_ptr<Region> RegionPtr;

class Fighter : public FighterImpl<MsgFighterInfo, MailSyncPlayerInfo2Region>
{
public :
        typedef FighterImpl<MsgFighterInfo, MailSyncPlayerInfo2Region> SuperType;
        typedef Region RegionType;
public :
        Fighter(uint64_t id,
                const RegionPtr& region,
                const MailReqEnterRegion::MsgReqFighterInfo& info);
        ~Fighter() override;

        static void InitOnce();
        bool InitFromLobby(const SyncMessageType& playerInfo) override;
        virtual void Rebind(const std::shared_ptr<Fighter>& obj) { }

public :
        FORCE_INLINE RegionPtr GetRegion() { return std::reinterpret_pointer_cast<Region>(SuperType::GetRegion()); }
        void PackFighterInfoBrief(MsgFighterInfo& msg) override;
        void PackFighterInfo(MsgFighterInfo& msg) override;
        void PackConclude(const std::shared_ptr<Region>& region);

public :
        FORCE_INLINE int64_t GetPosIdx() const { return _posInfo.dst_idx(); }
        FORCE_INLINE int64_t GetScore() const { return _posInfo.score(); }
        FORCE_INLINE time_t GetScoreTime() const { return _posInfo.dst_time(); }

public :
        FORCE_INLINE bool HasBattleItem(uint64_t id) { return !_battleItemList.empty(); }
        void RemoveBattleItem(uint64_t id)
        {
                auto it = _battleItemList.find(id);
                if (_battleItemList.end() != it)
                {
                        _battleItemUseList[((id & 0xFF000000) >> 24)] += 1;
                        _battleItemList.erase(it);
                        // auto subID = (id&0xFF000000) >> 24;
                        // REGION_LOG_INFO(GetRegionGuid(), "111111111111111111111111 f:{} id:{} cnt:{}", GetID(), subID, _battleItemUseList[subID]);
                }
                // REGION_LOG_INFO(GetRegionGuid(), "玩家[{}] 使用道具 id:{} leftcnt:{}", GetID(), id, _battleItemList.size());
        }

public :
        MsgFighterPosInfo _posInfo;
        MsgConcludeFighterInfo _concludeInfo;
        std::unordered_set<int64_t> _battleItemList;
        std::unordered_map<int64_t, int64_t> _battleItemUseList;
        std::unordered_map<int64_t, int64_t> _battleItemHitList;
        TimerGuidType _landTimerGuid = INVALID_TIMER_GUID;
        time_t _endTime = 0;
        time_t _shieldEndTime = 0;
        uint32_t _heroID = 0;
        uint32_t _rankLV = 0;
        uint32_t _rankStar = 0;
        int16_t _speedPosCnt = 0;
        int16_t _objGuid = 0;
        int16_t _diKangGanRaoCnt = 0;
        int16_t _coreCnt = 0;
        int16_t _continueCenterCnt = 0;
        int64_t _addCoins = 0;

        DECLARE_SHARED_FROM_THIS(Fighter);
};

template <typename _By>
class Robot : public _By
{
        typedef _By SuperType;
public :
        Robot(int64_t id, const RegionPtr& region, const MailReqEnterRegion::MsgReqFighterInfo& info)
                : SuperType(id, region, info)
        {
        }

        ~Robot() override
        {
        }

	bool IsRobot() const override { return true; }
        void Rebind(const std::shared_ptr<_By>& obj) override
        {
                _bindActor = obj;
        }
        void PackFighterInfo(MsgFighterInfo& msg) override
        {
                SuperType::PackFighterInfo(msg);
                msg.set_ai_action(_aiAction);
                auto obj = _bindActor.lock();
                if (obj)
                        msg.set_ai_bind_guid(obj->GetID());
                else
                        msg.set_ai_bind_guid(1);
        }

        bool InitFromLobby(const typename SuperType::SyncMessageType& playerInfo) override
        {
                if (!SuperType::InitFromLobby(playerInfo))
                        return false;

                MailSyncPlayerInfo2RegionExtra data;
                playerInfo.extra().UnpackTo(&data);
                _aiAction = data.ai_id();

                return true;
        }

        void Send2Client(uint64_t mainType, uint64_t subType, const ActorMailDataPtr& msg) override
        {
                auto obj = _bindActor.lock();
                if (obj)
                        obj->Send2Client(mainType, subType, msg);
        }

private :
        std::weak_ptr<_By> _bindActor;
        int64_t _aiAction = 0;
};

}; // end namespace Jump

// vim: fenc=utf8:expandtab:ts=8
