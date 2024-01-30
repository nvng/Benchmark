#pragma once

#include "Util.h"

#define ThreadSafeSetCommon(_Fy, _Ly) \
template <typename _Vy, std::size_t _Offset> \
class ThreadSafeSetInternal##_Fy \
{ \
public : \
        ThreadSafeSetInternal##_Fy(const std::string& flag) \
                : _cnt(Next2N(std::thread::hardware_concurrency()) - 1) \
                , _flag(flag) \
        { \
                _listMutex = new SpinLock[_cnt+1]; \
                _list = new _Ly<_Vy>[_cnt+1]; \
        } \
\
        ~ThreadSafeSetInternal##_Fy() \
        { \
                delete[] _listMutex; \
                _listMutex = nullptr; \
                delete[] _list; \
                _list = nullptr; \
        } \
\
        FORCE_INLINE bool Add(const _Vy& v) \
        { \
                auto idx = Idx(v); \
                LOCK_GUARD(_listMutex[idx], idx, _flag); \
                return _list[idx].emplace(v).second; \
        } \
 \
        FORCE_INLINE _Vy Remove(const _Vy& v) \
        { \
                auto idx = Idx(v); \
                LOCK_GUARD(_listMutex[idx], idx, _flag); \
                auto it = _list[idx].find(v); \
                if (_list[idx].end() != it) \
                { \
                        auto ret = *it; \
                        _list[idx].erase(it); \
                        return ret; \
                } \
                return _Vy(); \
        } \
 \
        FORCE_INLINE _Vy Get(const _Vy& v) \
        { \
                auto idx = Idx(v); \
                LOCK_GUARD(_listMutex[idx], idx, _flag); \
                auto it = _list[idx].find(v); \
                return _list[idx].end() != it ? *it : _Vy(); \
        } \
 \
        FORCE_INLINE _Vy Get(const _Vy& v, bool& found) \
        { \
                found = false; \
                auto idx = Idx(v); \
                LOCK_GUARD(_listMutex[idx], idx, _flag); \
                auto it = _list[idx].find(v); \
                if (_list[idx].end() != it) { \
                        found = true; \
                        return *it; \
                } else \
                        return _Vy(); \
        } \
 \
        FORCE_INLINE void Clear() \
        { \
                for (int64_t i=0; i<_cnt+1; ++i) \
                { \
                        LOCK_GUARD(_listMutex[i], i, _flag); \
                        _list[i].clear(); \
                } \
        } \
 \
        FORCE_INLINE std::size_t Size() \
        { \
                int64_t sum = 0; \
                for (int64_t i=0; i<_cnt+1; ++i) \
                { \
                        LOCK_GUARD(_listMutex[i], i, _flag); \
                        sum += _list[i].size(); \
                } \
                return sum; \
        } \
 \
	template <typename _Fy> \
	FORCE_INLINE void Foreach(const _Fy& cb) \
	{ \
                for (int64_t i=0; i<_cnt+1; ++i) \
                { \
                        _listMutex[i].lock(); \
                        _Ly<_Vy> tmpList = _list[i]; \
                        _listMutex[i].unlock(); \
 \
                        for (auto& val : tmpList) \
                                cb(val); \
                } \
	} \
 \
        template <typename _Fy> \
        FORCE_INLINE void ForeachAndClear(_Fy cb) \
        { \
                for (int64_t i=0; i<_cnt+1; ++i) \
                { \
                        _listMutex[i].lock(); \
                        _Ly<_Vy> tmpList = std::move(_list[i]); \
                        _listMutex[i].unlock(); \
\
                        for (auto& val : tmpList) \
                                cb(val); \
                } \
        } \
 \
private : \
        FORCE_INLINE int64_t Idx(uint64_t id) const { return (id >> _Offset) & _cnt; } \
private : \
        const int64_t _cnt = 0; \
        SpinLock* _listMutex = nullptr; \
        _Ly<_Vy>* _list = nullptr; \
        const std::string _flag; \
}

#include <set>
#include <unordered_set>

ThreadSafeSetCommon(Set, std::set);
ThreadSafeSetCommon(UnorderedSet, std::unordered_set);

template <typename _Vy, std::size_t _Offset=8>
using ThreadSafeSet = ThreadSafeSetInternalSet<_Vy, _Offset>;

template <typename _Vy, std::size_t _Offset=8>
using ThreadSafeUnorderedSet = ThreadSafeSetInternalUnorderedSet<_Vy, _Offset>;

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
