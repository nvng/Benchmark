#pragma once

#include "Player.h"

namespace nl::af::impl {

template <typename _Ty, typename _Mh, Compress::ECompressType _Ct=Compress::ECompressType::E_CT_Zstd>
class TcpSessionGate : public ::nl::net::server::TcpSession<_Ty, false, _Mh, _Ct>
{
        typedef ::nl::net::server::TcpSession<_Ty, false, _Mh, _Ct> SuperType;
        typedef _Ty ImplType;
public :
        typedef typename SuperType::MsgHeaderType MsgHeaderType;

        TcpSessionGate(typename SuperType::SocketType&& s)
                : SuperType(std::move(s))
                  , _playerList("TcpSessionGate_playerList")
        {
        }

        void OnMessageMultiCast(typename SuperType::BuffTypePtr::element_type* buf, const typename SuperType::BuffTypePtr& bufRef) override
        {
                auto msgMultiCastHead = *reinterpret_cast<typename MsgHeaderType::MsgMultiCastHeader*>(buf);
                switch (msgMultiCastHead._type)
                {
                case MsgHeaderType::template MsgTypeMerge<E_MIMT_Internal, E_MIIST_MultiCast>() :
                        {
                                const auto bodySize = msgMultiCastHead._size - sizeof(typename MsgHeaderType::MsgMultiCastHeader) - msgMultiCastHead._ssize;

                                MsgMultiCastInfo idListMsg;
                                Compress::UnCompressAndParseAlloc(static_cast<Compress::ECompressType>(msgMultiCastHead._ct),
                                                                  idListMsg,
                                                                  buf + sizeof(typename MsgHeaderType::MsgMultiCastHeader) + bodySize,
                                                                  msgMultiCastHead._ssize);

                                const uint64_t type = msgMultiCastHead._stype;
                                const Compress::ECompressType ct = static_cast<Compress::ECompressType>(msgMultiCastHead._sct);

                                for (auto& val : idListMsg.id_list())
                                {
                                        if (val.first != idListMsg.except())
                                        {
                                                auto p = GetPlayer(val.first);
                                                if (p)
                                                        p->template Send2Client<MsgHeaderType>(bufRef, buf, bodySize + sizeof(typename MsgHeaderType::MsgMultiCastHeader),
                                                                                      ct, MsgHeaderType::MsgMainType(type), MsgHeaderType::MsgSubType(type));
                                        }
                                }
                        }
                        break;
                case MsgHeaderType::template MsgTypeMerge<E_MIMT_Internal, E_MIIST_BroadCast>() :
                        {
                                const uint64_t type = msgMultiCastHead._stype;
                                const Compress::ECompressType ct = static_cast<Compress::ECompressType>(msgMultiCastHead._ct);

                                PlayerMgr::GetInstance()->Foreach([&bufRef, buf, &msgMultiCastHead, ct, type](const auto& p) {
                                        p->template Send2Client<MsgHeaderType>(bufRef, buf, msgMultiCastHead._size, ct, MsgHeaderType::MsgMainType(type), MsgHeaderType::MsgSubType(type));
                                });
                        }
                        break;
                default: break;
                }
        }

public :
        FORCE_INLINE bool AddPlayer(const PlayerPtr& p)
        { if (!p) return false; return _playerList.Add(p->GetID(), p); }

        FORCE_INLINE PlayerPtr GetLobbyPlayer(int64_t uniqueID, uint64_t id)
        {
                auto ret = _playerList.Get(id).lock();
                LOG_WARN_IF(ret && ret->GetUniqueID() != uniqueID,
                            "获取玩家[{}]时，GetUniqueID[{}] uniqueID[{}]",
                            id, ret->GetUniqueID(), uniqueID);
                return ret && uniqueID == ret->GetUniqueID() ? ret : nullptr;
        }

        FORCE_INLINE PlayerPtr GetPlayer(uint64_t id)
        { return _playerList.Get(id).lock(); }

        FORCE_INLINE PlayerPtr RemovePlayer(uint64_t id, void* ptr)
        { return _playerList.Remove(id, ptr).lock(); }
        FORCE_INLINE std::size_t GetPlayerCnt() { return _playerList.Size(); }

protected :
        ThreadSafeUnorderedMap<uint64_t, PlayerWeakPtr> _playerList;
};

}; // end of namespace nl::af::impl

// vim: fenc=utf8:expandtab:ts=8:noma
