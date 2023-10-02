#pragma once

template <typename _Sy>
class MultiCastWapper
{
public :
        bool Add(uint64_t id, const std::shared_ptr<_Sy>& ses)
        {
                if (!ses)
                        return false;

                auto it = _itemList.find(ses->GetID());
                if (_itemList.end() != it)
                {
                        auto item = it->second;
                        item->_idList.emplace(id);
                        item->_idsMsg.reset();
                }
                else
                {
                        auto item = std::make_shared<stMultiCastWapperItem>();
                        item->_ses = ses;
                        item->_idList.emplace(id);
                        _itemList.emplace(ses->GetID(), item);
                }
                return true;
        }

        void Remove(uint64_t id)
        {
                // TODO:
                for (auto& val : _itemList)
                {
                        auto info = val.second;
                        auto it = info->_idList.find(id);
                        if (info->_idList.end() != it)
                        {
                                info->_idList.erase(it);
                                if (info->_idList.empty())
                                        _itemList.erase(val.first);
                                else
                                        info->_idsMsg.reset();
                                break;
                        }
                }
                return;
        }

        void RemoveSes(uint64_t id)
        {
                _itemList.erase(id);
        }

        void MultiCast(uint64_t from, uint64_t mt, uint64_t st, const std::shared_ptr<google::protobuf::MessageLite>& msg, uint64_t except=0)
        {
                for (auto& val : _itemList)
                {
                        auto item = val.second;
                        auto ses = item->_ses.lock();
                        if (!ses)
                                continue;

                        if (1 == item->_idList.size())
                        {
                                auto id = *item->_idList.begin();
                                if (id != except)
                                        ses->SendPB(msg, mt, st, _Sy::MsgHeaderType::E_RMT_Send, 0, from, id);
                        }
                        else
                        {
                                if (!item->_idsMsg)
                                {
                                        item->_idsMsg = std::make_shared<MsgMultiCastInfo>();
                                        for (auto id : item->_idList)
                                                item->_idsMsg->add_id_list(id);
                                }
                                ses->MultiCast(msg, item->_idsMsg, mt, st, from, except);
                        }
                }
        }

        FORCE_INLINE bool IsEmpty() const { return _itemList.empty(); }

private :
        struct stMultiCastWapperItem
        {
                std::weak_ptr<_Sy> _ses;
                std::shared_ptr<MsgMultiCastInfo> _idsMsg;
                std::unordered_set<uint64_t> _idList;
        };
        std::unordered_map<uint64_t, std::shared_ptr<stMultiCastWapperItem>> _itemList;
};

template <typename _Sy, typename _Ry>
class MultiCastWapperImpl : public MultiCastWapper<_Sy>
{
        typedef MultiCastWapper<_Sy> SuperType;
public :
        FORCE_INLINE bool AddRobot(const std::shared_ptr<_Ry>& robot)
        { return robot ? _robotList.emplace(robot->GetID(), robot).second : false; }

        FORCE_INLINE void RemoveRobot(const std::shared_ptr<_Ry>& robot)
        { if (robot) _robotList.erase(robot->GetID()); }

        void MultiCast(uint64_t from, uint64_t mt, uint64_t st, const std::shared_ptr<google::protobuf::MessageLite>& msg, uint64_t except=0)
        {
                SuperType::MultiCast(from, mt, st, msg, except);
                for (auto& val : _robotList)
                {
                        auto r = val.second.lock();
                        if (r && r->GetID() != except)
                                r->Send2Client(mt, st, msg); // robot 只模拟客户端。
                }
        }

private :
        std::unordered_map<uint64_t, std::weak_ptr<_Ry>> _robotList;
};

// vim: fenc=utf8:expandtab:ts=8:noma
