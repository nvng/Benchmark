#pragma once

#include <boost/unordered/concurrent_flat_set.hpp>

template <typename _Vy>
class ThreadSafeUnorderedSet
{
public :
        explicit ThreadSafeUnorderedSet(const std::string& flag)
        {
        }

        explicit ThreadSafeUnorderedSet(ThreadSafeUnorderedSet&& rhs)
        {
                _list = std::move(rhs._list);
        }

        ~ThreadSafeUnorderedSet() = default;

        FORCE_INLINE bool Add(_Vy v)
        { return _list.emplace(v); }

        FORCE_INLINE void Remove(_Vy v)
        { _list.erase(v); }

        FORCE_INLINE bool Get(_Vy v)
        { return _list.cvisit(v); }

        FORCE_INLINE void Clear() { _list.clear(); }
        FORCE_INLINE std::size_t Empty() { return _list.empty(); }
        FORCE_INLINE std::size_t Size() { return _list.size(); }

        FORCE_INLINE void ForeachClear(const auto& cb)
        {
                decltype(_list) tmpList = std::move(_list);
                tmpList.cvisit_all([&cb](auto& val) {
                        cb(val.second);
                });
        }

        FORCE_INLINE void Foreach(const auto& cb)
        {
                decltype(_list) tmpList = _list;
                tmpList.cvisit_all([&cb](auto& val) {
                        cb(val.second);
                });
        }

private :
        boost::concurrent_flat_set<_Vy> _list;
};

#define SetCommon(_Fy, _Ly) \
template <typename _Vy> \
class SetInternal##_Fy \
{ \
public : \
        FORCE_INLINE bool Add(const _Vy& v) \
        { \
                return _list.emplace(v).second; \
        } \
 \
        FORCE_INLINE _Vy Remove(const _Vy& v) \
        { \
                _Vy ret; \
                auto it = _list.find(v); \
                if (_list.end() != it) \
                { \
                        ret = *it; \
                        _list.erase(it); \
                } \
                return ret; \
        } \
 \
        FORCE_INLINE _Vy Get(const _Vy& v) \
        { \
                auto it = _list.find(v); \
                return _list.end() != it ? *it : _Vy(); \
        } \
 \
        FORCE_INLINE void Clear() \
        { \
                _list.clear(); \
        } \
 \
        FORCE_INLINE std::size_t Size() \
        { \
                return _list.size(); \
        } \
 \
        template <typename _Fy> \
        FORCE_INLINE void Foreach(_Fy cb) \
        { \
                for (auto& val : _list) \
                        cb(val); \
        } \
 \
private : \
        _Ly<_Vy> _list; \
}

SetCommon(Set, std::set);
SetCommon(UnorderedSet, std::unordered_set);

template <typename _Vy>
using Set = SetInternalSet<_Vy>;

template <typename _Vy>
using UnorderedSet = SetInternalUnorderedSet<_Vy>;

// vim: fenc=utf8:expandtab:ts=8
