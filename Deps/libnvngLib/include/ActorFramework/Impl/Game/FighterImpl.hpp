#pragma once

#include "GameLobbySession.h"
#include "GameGateSession.h"

template <typename _Py, typename _SPy>
class FighterImpl : public std::enable_shared_from_this<FighterImpl<_Py, _SPy>>
{
public :
        typedef IActor RegionType;
        typedef _Py MessageType;
        typedef _SPy SyncMessageType;
        typedef FighterImpl<MessageType, SyncMessageType> ThisType;
public :
        FighterImpl(uint64_t id,
                    const std::shared_ptr<RegionType>& region,
                    const MailReqEnterRegion::MsgReqFighterInfo& info)
                : _region(region)
                  , _id(id)
        {
                _camp = info.camp();
                if (region)
                        _regionGuid = region->GetID();
        }

        virtual ~FighterImpl()
        {
        }

        virtual bool IsRobot() const { return false; }
        FORCE_INLINE uint64_t GetID() const { return _id; }
        FORCE_INLINE const std::string& GetNickName() const { return _nickName; }
        FORCE_INLINE const std::string& GetIcon() const { return _icon; }
        FORCE_INLINE int64_t GetCamp() const { return _camp; }

        virtual bool InitFromLobby(const SyncMessageType& playerInfo)
        {
                _lv = playerInfo.lv();
                _nickName = playerInfo.nick_name();
                _icon = playerInfo.icon();
                return true;
        }

        virtual void PackFighterInfoBrief(MessageType& msg)
        {
                msg.set_guid(GetID());
                msg.set_lv(_lv);
                msg.set_nick_name(GetNickName());
                msg.set_icon(GetIcon());
                msg.set_flag(_flag);
        }

        virtual void PackFighterInfo(MessageType& msg) { }

        FORCE_INLINE std::shared_ptr<RegionType> GetRegion() { return _region.lock(); }
        FORCE_INLINE int64_t GetRegionGuid() { return _regionGuid; }

        virtual void MarkClientDisconnect() { }
        virtual void MarkClientConnect() { }
        virtual bool IsClientConnect() const { return true; }

        virtual GameLobbySession::ActorAgentTypePtr GetPlayer() { return nullptr; }
        virtual void SetClient(const GameGateSession::ActorAgentTypePtr& c) { }
        virtual GameGateSession::ActorAgentTypePtr GetClient() { return nullptr; }

	virtual void Send2Player(uint64_t mainType, uint64_t subType, const ActorMailDataPtr& msg) { }
	virtual void Send2Client(uint64_t mainType, uint64_t subType, const ActorMailDataPtr& msg) { }

public :
        int64_t _flag = 0;
private :
        std::weak_ptr<RegionType> _region;
	uint32_t _id = 0;
        uint32_t _lv = 0;
        uint32_t _camp = 0;
        uint32_t _regionGuid = 0;
        std::string _nickName;
        std::string _icon;
};

template <typename _Ty>
class PlayerFighter : public _Ty
{
        typedef _Ty SuperType;
public :
        PlayerFighter(const std::shared_ptr<typename _Ty::RegionType>& region,
                      const GameLobbySession::ActorAgentTypePtr& p,
                      const GameGateSession::ActorAgentTypePtr& cli,
                      const MailReqEnterRegion::MsgReqFighterInfo& info)
                : SuperType(p->GetID(), region, info)
                  , _player(p)
                  , _client(cli)
        {
        }

        ~PlayerFighter() override
        {
        }

        GameLobbySession::ActorAgentTypePtr GetPlayer() override { return _player; }
        void SetClient(const GameGateSession::ActorAgentTypePtr& c) override { _client = c; }
        GameGateSession::ActorAgentTypePtr GetClient() override { return _client; }

        void Send2Player(uint64_t mainType, uint64_t subType, const ActorMailDataPtr& msg) override
        {
                auto region = _Ty::GetRegion();
                if (_player && region)
                        region->Send(_player, mainType, subType, msg);
        }

        void Send2Client(uint64_t mainType, uint64_t subType, const ActorMailDataPtr& msg) override
        {
                auto region = _Ty::GetRegion();
                if (_client && region && IsClientConnect())
                        region->Send(_client, mainType, subType, msg);
#if 0
                else
                        LOG_WARN("玩家[{}] 向 Client 发送消息 mt[{:#x}] st[{:#x}] 时，_client[{}] region[{}] IsClientConnect[{}]",
                                 SuperType::GetID(), mainType, subType, fmt::ptr(_client.get()), fmt::ptr(region.get()), IsClientConnect());
#endif
        }

        void MarkClientDisconnect() override { _isClientConnect = false; }
        void MarkClientConnect() override { _isClientConnect = true; }
        bool IsClientConnect() const override { return _isClientConnect; }

private :
        GameLobbySession::ActorAgentTypePtr _player;
        GameGateSession::ActorAgentTypePtr _client;
        bool _isClientConnect = true;
};

// vim: fenc=utf8:expandtab:ts=8
