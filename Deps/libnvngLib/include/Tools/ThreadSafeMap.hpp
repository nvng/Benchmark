#pragma once

#include "Util.h"

#include <type_traits>

template <typename _Ty> struct is_shared_pointer : public std::false_type { };
template <typename _Ty> struct is_shared_pointer<std::shared_ptr<_Ty>> : public std::true_type { };
template <typename _Ty> struct is_weak_pointer : public std::false_type { };
template <typename _Ty> struct is_weak_pointer<std::weak_ptr<_Ty>> : public std::true_type { };

#include <boost/unordered/concurrent_flat_map.hpp>

template <typename _Ky, typename _Vy>
class ThreadSafeUnorderedMap
{
        static_assert(is_shared_pointer<_Vy>::value || is_weak_pointer<_Vy>::value);
public :
        ThreadSafeUnorderedMap(const std::string& flag)
        {
        }

        ThreadSafeUnorderedMap(ThreadSafeUnorderedMap&& rhs)
        {
                _list = std::move(rhs._list);
        }

        ~ThreadSafeUnorderedMap() = default;

        FORCE_INLINE bool Add(_Ky k, const _Vy& v)
        { return _list.emplace(k, v); }

        FORCE_INLINE _Vy Remove(_Ky k, void* ptr)
        {
                std::shared_ptr<typename _Vy::element_type> ret;
                _list.erase_if(k, [&ret, ptr](const auto& val) {
                        decltype(ret) t;
                        if constexpr (is_weak_pointer<_Vy>::value)
                                t = val.second.lock();
                        else
                                t = val.second;
                        if (nullptr == ptr || !t || ptr == t.get())
                        {
                                ret = t;
                                return true;
                        }
                        else
                        {
                                return false;
                        }
                });
                return ret;
        }

        FORCE_INLINE _Vy Get(_Ky k)
        {
                _Vy ret;
                _list.visit(k, [&ret](auto& val) {
                        ret = val.second;
                });
                return ret;
        }

        FORCE_INLINE _Vy Get(_Ky k, bool& found)
        {
                _Vy ret;
                found = _list.visit(k, [&ret](auto& val) {
                        ret = val.second;
                });
                return ret;
        }

        FORCE_INLINE void Clear() { _list.clear(); }
        FORCE_INLINE std::size_t Empty() { return _list.empty(); }
        FORCE_INLINE std::size_t Size() { return _list.size(); }

        FORCE_INLINE void ForeachClear(const auto& cb)
        {
                decltype(_list) tmpList = std::move(_list);
                tmpList.visit_all([&cb](auto& val) {
                        cb(val.second);
                });
        }

        FORCE_INLINE void Foreach(const auto& cb)
        {
                decltype(_list) tmpList = _list;
                tmpList.visit_all([&cb](auto& val) {
                        cb(val.second);
                });
        }

private :
        boost::concurrent_flat_map<_Ky, _Vy> _list;
};

#define MapCommon(_Fy, _Ly) \
template <typename _Ky, typename _Vy> \
class MapInternal##_Fy \
{ \
public : \
        MapInternal##_Fy() = default; \
        ~MapInternal##_Fy() = default; \
        MapInternal##_Fy(MapInternal##_Fy&& rhs) { _list = std::move(rhs._list); } \
	FORCE_INLINE bool Add(_Ky k, const _Vy& v) { return _list.emplace(k, v).second; } \
	FORCE_INLINE _Vy Remove(_Ky k) \
	{ \
		_Vy ret; \
		auto it = _list.find(k); \
		if (_list.end() != it) \
		{ \
			ret = it->second; \
			_list.erase(it); \
		} \
		return ret; \
	} \
 \
	FORCE_INLINE _Vy Get(_Ky k) \
	{ \
		auto it = _list.find(k); \
		return _list.end() != it ? it->second : _Vy(); \
	} \
 \
	FORCE_INLINE void Clear() { _list.clear(); } \
 \
        FORCE_INLINE bool Empty() const { return _list.empty(); } \
	FORCE_INLINE std::size_t Size() const { return _list.size(); } \
 \
	FORCE_INLINE void Foreach(const auto& cb) \
	{ \
		for (auto& val : _list) \
			cb(val.second); \
	} \
 \
private : \
	_Ly<_Ky, _Vy> _list; \
}

MapCommon(Map, std::map);
MapCommon(UnorderedMap, std::unordered_map);

template <typename _Ky, typename _Vy>
using Map = MapInternalMap<_Ky, _Vy>;

template <typename _Ky, typename _Vy>
using UnorderedMap = MapInternalUnorderedMap<_Ky, _Vy>;

// vim: fenc=utf8:expandtab:ts=8
