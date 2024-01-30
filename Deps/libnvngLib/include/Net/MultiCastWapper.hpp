#pragma once

template <typename _Sy>
class MultiCastWapper
{
public :
        bool Add(int64_t id, const std::shared_ptr<_Sy>& ses)
        {
                if (!ses)
                        return false;

                auto it = _itemList.find(ses->GetID());
                if (_itemList.end() != it)
                {
                        auto item = it->second;
                        item->_msgIdsList->mutable_id_list()->insert({id, false});
                }
                else
                {
                        auto item = std::make_shared<stMultiCastWapperItem>();
                        item->_ses = ses;
                        item->_msgIdsList->mutable_id_list()->insert({id, false});
                        _itemList.emplace(ses->GetID(), item);
                }
                return true;
        }

        void Remove(int64_t id)
        {
                for (auto& val : _itemList)
                {
                        auto info = val.second;
                        auto idList = info->_msgIdsList->mutable_id_list();
                        auto it = idList->find(id);
                        if (idList->end() != it)
                        {
                                idList->erase(it);
                                if (info->_msgIdsList->id_list_size() <= 0)
                                        _itemList.erase(val.first);
                                break;
                        }
                }
                return;
        }

        void RemoveSes(int64_t id)
        {
                _itemList.erase(id);
        }

        void MultiCast(int64_t from, uint64_t mt, uint64_t st, const nl::af::ActorMailDataPtr& msg, int64_t except=0)
        {
                if (_itemList.empty())
                        return;

                for (auto& val : _itemList)
                {
                        auto item = val.second;
                        auto ses = item->_ses.lock();
                        if (!ses)
                                continue;

                        if (1 == item->_msgIdsList->id_list_size())
                        {
                                auto id = item->_msgIdsList->id_list().begin()->first;
                                if (id != except)
                                        ses->SendPB(msg, mt, st, _Sy::MsgHeaderType::E_RMT_Send, 0, from, id);
                        }
                        else
                        {
                                item->_msgIdsList->set_except(except);
                                ses->MultiCast(msg, item->_msgIdsList, mt, st, from);
                        }
                }
        }

        FORCE_INLINE bool IsEmpty() const { return _itemList.empty(); }

private :
        struct stMultiCastWapperItem
        {
                std::weak_ptr<_Sy> _ses;
                std::shared_ptr<MsgMultiCastInfo> _msgIdsList = std::make_shared<MsgMultiCastInfo>();
        };
        std::unordered_map<int64_t, std::shared_ptr<stMultiCastWapperItem>> _itemList;
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

        void MultiCast(int64_t from, uint64_t mt, uint64_t st, const nl::af::ActorMailDataPtr& msg, int64_t except=0)
        {
                SuperType::MultiCast(from, mt, st, msg, except);
                for (auto& val : _robotList)
                {
                        auto r = val.second.lock();
                        if (r)
                                r->Send2Client(mt, st, msg); // robot 只模拟客户端。
                }
        }

private :
        std::unordered_map<uint64_t, std::weak_ptr<_Ry>> _robotList;
};

// vim: fenc=utf8:expandtab:ts=8:noma
