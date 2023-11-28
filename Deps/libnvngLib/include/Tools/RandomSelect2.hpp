#pragma once

#include <map>

#include "Tools/Util.h"

template <NonCopyableConcept _Sy, ArithmeticConcept _Ty, typename _My>
class RandomSelect2 final
{
public :
	typedef _Sy ValueType;
	typedef _Ty RatioType;

	// [min, max)
	typedef RatioType(*RandInRangeFuncType)(RatioType, RatioType);
	explicit RandomSelect2(RandInRangeFuncType randFunc = RandInRange<RatioType>)
		: _randInRange(randFunc)
	{
	}

	template <typename... Args>
	FORCE_INLINE bool Add(RatioType r, const Args& ... args)
	{
		std::lock_guard<_My> l(_mutex);
		auto obj = std::make_shared<ValueType>(r, std::forward<const Args&>(args)...);
		bool ret = _randomList.emplace(_sum+r, obj).second;
		if (ret)
			_sum += r;
		return ret;
	}

	FORCE_INLINE bool Add(RatioType r, const std::shared_ptr<const ValueType>& obj)
	{
		std::lock_guard<_My> l(_mutex);
		bool ret = _randomList.emplace(_sum+r, obj).second;
		if (ret)
			_sum += r;
		return ret;
	}

	FORCE_INLINE const ValueType& Get()
	{
		return Get(_randInRange(RatioType(), _sum));
	}

	FORCE_INLINE const ValueType& Get(RatioType r)
	{
		std::lock_guard<_My> l(_mutex);
		static const ValueType emptyObj;
		auto it = _randomList.upper_bound(r);
		return _randomList.end() != it ? *(it->second) : emptyObj;
	}

	FORCE_INLINE RatioType GetSum()
	{
		std::lock_guard<_My> l(_mutex);
		return _sum;
	}

	FORCE_INLINE RatioType IsEmpty()
	{
		std::lock_guard<_My> l(_mutex);
		return RatioType() == _sum;
	}

	FORCE_INLINE void Clear()
	{
		std::lock_guard<_My> l(_mutex);
		_sum = RatioType();
		_randomList.clear();
	}

        template <typename _Fy>
        void Foreach(const _Fy& cb)
        {
                for (auto& val : _randomList)
                        cb(val.second);
        }

private :
	_My _mutex;
	RatioType _sum = RatioType();
	const RandInRangeFuncType _randInRange = nullptr;
	std::map<RatioType, std::shared_ptr<const ValueType>> _randomList;

private :
	DISABLE_COPY_AND_ASSIGN(RandomSelect2);
};

// vim: fenc=utf8:expandtab:ts=8:noma
