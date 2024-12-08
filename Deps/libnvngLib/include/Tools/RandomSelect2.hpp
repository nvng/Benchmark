#pragma once

#include <map>

#include "Tools/Util.h"

template <NonCopyableConcept _Sy, ArithmeticConcept _Ty, typename _My>
class RandomSelect2 final
{
public :
	typedef _Sy value_type;
	typedef _Ty ratio_type;

	// [min, max)
	typedef _Ty(*RandInRangeFuncType)(_Ty, _Ty);
	explicit RandomSelect2(RandInRangeFuncType randFunc)
		: mRandInRange(randFunc)
	{
	}

	template <typename... Args>
	inline bool Add(_Ty r, const Args& ... args)
	{
		std::lock_guard<_My> l(mMutex);
		auto obj = std::make_shared<_Sy>(r, std::forward<const Args&>(args)...);
		bool ret = mRandomList.emplace(mSum+r, obj).second;
		if (ret)
			mSum += r;
		return ret;
	}

	inline bool Add(_Ty r, const std::shared_ptr<const _Sy>& obj)
	{
		std::lock_guard<_My> l(mMutex);
		bool ret = mRandomList.emplace(mSum+r, obj).second;
		if (ret)
			mSum += r;
		return ret;
	}

	inline const _Sy& Get()
	{
		return Get(mRandInRange(_Ty(), mSum));
	}

	inline const _Sy& Get(_Ty r)
	{
		std::lock_guard<_My> l(mMutex);
		static const _Sy emptyObj;
		auto it = mRandomList.upper_bound(r);
		return mRandomList.end() != it ? *(it->second) : emptyObj;
	}

	inline _Ty GetSum()
	{
		std::lock_guard<_My> l(mMutex);
		return mSum;
	}

	inline _Ty IsEmpty()
	{
		std::lock_guard<_My> l(mMutex);
		return _Ty() == mSum;
	}

	inline void Clear()
	{
		std::lock_guard<_My> l(mMutex);
		mSum = _Ty();
		mRandomList.clear();
	}

        template <typename _Fy>
        void Foreach(const _Fy& cb)
        {
                for (auto& val : mRandomList)
                        cb(val.second);
        }

private :
	_My mMutex;
	_Ty mSum = _Ty();
	const RandInRangeFuncType mRandInRange = nullptr;
	std::map<_Ty, std::shared_ptr<const _Sy>> mRandomList;

private :
	DISABLE_COPY_AND_ASSIGN(RandomSelect2);
};

// vim: fenc=utf8:expandtab:ts=8:noma
