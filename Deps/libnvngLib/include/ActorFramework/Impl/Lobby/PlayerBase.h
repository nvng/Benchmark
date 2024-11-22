#pragma once

#include "PlayerMgrBase.h"
#include "LobbyGateSession.h"
#include "LobbyGameMgrSession.h"

namespace nl::af::impl {
      
#define INVALID_MONEY_VAL INT64_MIN

enum EPlayerBaseInternalFlagType
{
        E_PIF_Online = 1 << 0,
        E_PIF_InDeleteTimer = 1 << 1,
        E_PIF_InSaveTimer = 1 << 2,
        E_PIF_InRank = 1 << 3,
};

class PlayerBase : public ActorImpl<PlayerBase, PlayerMgrBase>
{
        typedef ActorImpl<PlayerBase, PlayerMgrBase> SuperType;
public :
#ifndef ____BENCHMARK____
        PlayerBase(uint64_t guid, const std::string& nickName, const std::string& icon)
                : SuperType(guid), _nickName(nickName), _iconStr(icon)
        {
        }
#else
        PlayerBase(uint64_t guid, const std::string& nickName, const std::string& icon)
                : SuperType(guid, 1 << 13), _nickName(nickName), _iconStr(icon)
        {
                ++_playerFlag;
        }
#endif

#ifndef ____BENCHMARK____
        ~PlayerBase() override
        {
        }
#else
        ~PlayerBase() override
        {
                --_playerFlag;
        }
#endif

        bool Start(std::size_t stackSize = 32 * 1024) override;

        virtual void Online();
        virtual void Offline();
        void KickOut(int64_t errorType);
        void OnClientReconnect(ERegionType oldRegionType);
        void OnDisconnect(const IActorPtr& agent);

        virtual void Pack2Client(MsgPlayerInfo& msg);
        virtual void OnCreateAccount(const std::shared_ptr<MsgClientLogin>& loginMsg);
        virtual bool LoadFromDB(const std::shared_ptr<MsgClientLogin>& loginMsg);
        virtual void InitFromDB(const DBPlayerInfo& dbInfo);
        virtual void AfterInitFromDB();
        FORCE_INLINE void Save2DB() { InitSavePlayer2DBTimer(); }
        virtual void Pack2DB(DBPlayerInfo& dbInfo);
        virtual bool Flush2DB(bool isDelete);

        bool CheckDayChange();
        virtual void OnDayChange(bool sync);
        bool CheckDataReset(int64_t h);
        virtual void OnDataReset(MsgDataResetNoneZero& msg, int64_t h);

public :
        void InitSavePlayer2DBTimer();
        void InitDeletePlayerTimer(uint64_t deleteUniqueID, double internal = 30.0);

public :
        FORCE_INLINE const std::string& GetNickName() const { return _nickName; }
        FORCE_INLINE const std::string& GetIcon() const { return _iconStr; }
protected :
        std::string _nickName;
        std::string _iconStr;
        int64_t _version = 0;
        std::atomic_uint64_t _uniqueID = 0; // 用于区分不同网关，不同网络链接过来的消息，只在 PlayerMgrActor 中赋值。

public :
        std::atomic_uint64_t _internalFlag = 0;

        // {{{ player attr
public :
        virtual void PackUpdatePlayerAttrExtra(EPlayerAttrType t, MsgUpdatePlayerAttr& msg);

        template <EPlayerAttrType _Ty>
        FORCE_INLINE void PackUpdatePlayerAttr(MsgPlayerChange& msg)
        {
                auto info = msg.add_change_attr_list();
                info->set_type(_Ty);
                info->set_val(GetAttr<_Ty>());
                PackUpdatePlayerAttrExtra(_Ty, *info);
        }

        template <EPlayerAttrType _Ty>
        FORCE_INLINE int64_t GetAttr() { return _attrArr[_Ty]; }
        template <EPlayerAttrType _Ty>
        FORCE_INLINE int64_t AddAttr(int64_t cnt)
        {
                if (cnt <= 0)
                        return _attrArr[_Ty];
                _attrArr[_Ty] += cnt;
                Save2DB();
                return _attrArr[_Ty];
        }
        template <EPlayerAttrType _Ty>
        FORCE_INLINE int64_t AddAttr(MsgPlayerChange& msg, int64_t cnt)
        {
                auto ret = AddAttr<_Ty>(cnt);
                PackUpdatePlayerAttr<_Ty>(msg);
                return ret;
        }
        template <EPlayerAttrType _Ty>
        FORCE_INLINE bool SetAttr(int64_t val)
        {
                _attrArr[_Ty] = val;
                Save2DB();
                return true;
        }

        template <EPlayerAttrType _Ty>
        FORCE_INLINE bool DelAttr(int64_t cnt)
        {
                if (cnt <= 0 || _attrArr[_Ty] < cnt)
                        return false;
                _attrArr[_Ty] -= cnt;
                Save2DB();
                return true;
        }
        template <EPlayerAttrType _Ty>
        FORCE_INLINE bool DelAttr(MsgPlayerChange& msg, int64_t cnt)
        {
                auto ret = DelAttr<_Ty>(cnt);
                PackUpdatePlayerAttr<_Ty>(msg);
                return ret;
        }

        virtual int64_t GetMoney(int64_t t);
        virtual int64_t AddMoney(MsgPlayerChange& msg, int64_t t, int64_t cnt, ELogServiceOrigType logType, uint64_t logParam);
        virtual bool CheckMoney(int64_t t, int64_t cnt);
        virtual int64_t DelMoney(MsgPlayerChange& msg, int64_t t, int64_t cnt, ELogServiceOrigType logType, uint64_t logParam);

        virtual bool AddExp(int64_t exp, ELogServiceOrigType logType, uint64_t logParam);
protected :
        void CheckTiLiRecovery();
        int64_t _attrArr[EPlayerAttrType_ARRAYSIZE] = { 0 };
        // }}}

        // {{{ location
public :
        FORCE_INLINE static uint64_t GenLocationIDFromHuman(uint64_t n, uint64_t f)
        { return GenLocationID(n, f / 10000, (f%10000)/100, (f%10000)%100); }
        FORCE_INLINE static uint64_t GenLocationID(int64_t n, int64_t p, int64_t c, int64_t d)
        { return (n&0xFFF) << 36 | (p&0xFFF) << 24 | (c & 0xFFF) << 12 | (d & 0xFFF); }
        FORCE_INLINE static std::array<uint64_t, 4> ParseLocationID(uint64_t id)
        {
                return std::array<uint64_t, 4> {
                        id & 0xFFF000000000,
                           id & 0xFFFFFF000000,
                           id & 0xFFFFFFFFF000,
                           id & 0xFFFFFFFFFFFF,
                };
        }
        FORCE_INLINE static std::pair<uint64_t, uint64_t> ParseLocationIDHuman(uint64_t id)
        {
                return std::pair<uint64_t, uint64_t> {
                        id >> 36,
                           ((id>>24)&0xFFF) * 10000 + ((id>>12) & 0xFFF) * 100 + (id & 0xFFF),
                };
        }
        // }}}

public :
        static void PackUpdateGoods(MsgPlayerChange& msg, int64_t t, int64_t id, int64_t num);
        FORCE_INLINE static void PackUpdateGoods(MsgPlayerChange& msg, const std::tuple<int64_t, int64_t, int64_t>& val)
        { PackUpdateGoods(msg, std::get<2>(val), std::get<0>(val), std::get<1>(val)); }
        FORCE_INLINE static void PackUpdateGoods(MsgPlayerChange& msg, const auto& tList)
        { for (auto& val : tList) PackUpdateGoods(msg, val); }

        virtual void AddDrop(MsgPlayerChange& msg,
                             int64_t type,
                             int64_t param,
                             int64_t cnt,
                             ELogServiceOrigType logType,
                             uint64_t logParam)
        {
        }

public :
        void Send2Client(uint64_t mainType, uint64_t subType, const std::shared_ptr<google::protobuf::MessageLite>& msg);
        void SetClient(const LobbyGateSession::ActorAgentTypePtr& client);
        int64_t GetClientSID() const;
        LobbyGateSession::ActorAgentTypePtr _clientActor;

public :
        virtual std::shared_ptr<MsgReqQueue> ReqQueue(const std::shared_ptr<MsgReqQueue>& msg);
        void ExitQueue();
        virtual std::shared_ptr<MsgExitQueue> ExitQueueInternal();
        FORCE_INLINE LobbyGameMgrSession::ActorAgentTypePtr GetQueue() const { return _queue; }
        EInternalErrorType QueueOpt(EQueueOptType opt, int64_t param);

        void ReqEnterRegion(const std::shared_ptr<MsgReqEnterRegion>& msg);
        FORCE_INLINE void SetRegion(const IActorPtr& region) { _region = region; }
        FORCE_INLINE IActorPtr GetRegion() const { return _region; }

        void Back2MainCity(ERegionType oldRegionType);

private :
        void Terminate() override;
        std::shared_ptr<MsgMatchInfo> _matchInfo;

private :
        // {{{ 缓存。
        IActorPtr _region; // TODO: 房间管理服断开连接，房间服断开连接。
        LobbyGameMgrSession::ActorAgentTypePtr _queue; // TODO: 排队服断开连接。
        // }}}

        nl::util::SteadyTimer _saveTimer;
        nl::util::SteadyTimer _deleteTimer;

        friend class PlayerMgrActor;
        friend class ActorImpl<PlayerBase, PlayerMgrBase>;
        EXTERN_ACTOR_MAIL_HANDLE();
        DECLARE_SHARED_FROM_THIS(PlayerBase);

#ifdef ____BENCHMARK____
public :
        uint64_t _actorMgrID = 0;
        static std::atomic_int64_t _playerFlag;
#endif
};
typedef std::shared_ptr<PlayerBase> PlayerBasePtr;
typedef std::weak_ptr<PlayerBase> PlayerBaseWeakPtr;

}; // end of namespace nl::af::impl

// vim: fenc=utf8:expandtab:ts=8
