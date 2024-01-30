#pragma once

#include <vector>

#include "Tools/Util.h"

#define DOUBLE_QUEUE_SAME       \
typedef std::vector<value_type> queue_type; \
\
DoubleQueue() = default; \
~DoubleQueue() = default; \
\
FORCE_INLINE queue_type* GetQueue() \
{ \
        mPopQueue.clear(); \
        if (mPopQueue.capacity()>cacheSizeLimit) \
        { \
                decltype(mPopQueue) tmpList; \
                tmpList.reserve(cacheSizeLimit); \
                mPopQueue.swap(tmpList); \
        } \
        { \
                std::lock_guard<mutex_type> lock(mMutex); \
                mPopQueue.swap(mPushQueue); \
        } \
        return &mPopQueue; \
} \
\
FORCE_INLINE bool IsEmpty() \
{ \
        std::lock_guard<mutex_type> lock(mMutex); \
        return mPushQueue.empty() && mPopQueue.empty(); \
} \
\
FORCE_INLINE size_t GetTotalSize() \
{ \
        std::lock_guard<mutex_type> lock(mMutex); \
        return mPushQueue.size() + mPopQueue.size(); \
} \
\
FORCE_INLINE size_t GetTotalCapacity() \
{ \
        std::lock_guard<mutex_type> lock(mMutex); \
        return mPushQueue.capacity() + mPopQueue.capacity(); \
} \
 \
private: \
        mutex_type mMutex; \
        queue_type mPushQueue; \
        queue_type mPopQueue; \
\
protected: \
        DISABLE_COPY_AND_ASSIGN(DoubleQueue)


template <typename T, typename Mutex=SpinLock, std::size_t cacheSizeLimit=1024>
class DoubleQueue
{
public:
        typedef T value_type;
        typedef Mutex mutex_type;

        FORCE_INLINE void Push(value_type&& item) { std::lock_guard<mutex_type> lock(mMutex); mPushQueue.emplace_back(std::forward<value_type>(item)); }

        DOUBLE_QUEUE_SAME;
};

template <typename T, typename Mutex, std::size_t cacheSizeLimit>
class DoubleQueue<T*, Mutex, cacheSizeLimit>
{
public :
        typedef T* value_type;
        typedef Mutex mutex_type;

        FORCE_INLINE void Push(value_type item) { std::lock_guard<mutex_type> lock(mMutex); mPushQueue.emplace_back(item); }

        DOUBLE_QUEUE_SAME;
};

#undef DOUBLE_QUEUE_SAME

template <typename T, typename Mutex=SpinLock, std::size_t cacheSizeLimit=1024>
class DoubleQueueMultiGet
{
};

template <typename T, typename Mutex, std::size_t cacheSizeLimit>
class DoubleQueueMultiGet<T*, Mutex, cacheSizeLimit>
{
public :
        typedef T* value_type;
        typedef Mutex mutex_type;
        typedef typename DoubleQueue<value_type, mutex_type, cacheSizeLimit>::queue_type queue_type;

        DoubleQueueMultiGet()
        {
        }

        FORCE_INLINE void Push(value_type item) { mQueue.Push(item); }

        FORCE_INLINE value_type TryGet()
        {
                std::lock_guard<mutex_type> lock(mMutex);
                if (mCurQueue->size() == mIdx)
                {
                        mCurQueue = mQueue.GetQueue();
                        mIdx = 0;

                        if (mCurQueue->empty())
                                return nullptr;
                }
                return (*mCurQueue)[mIdx++];
        }

        FORCE_INLINE bool IsEmpty() { return mQueue.IsEmpty(); }
        FORCE_INLINE std::size_t GetTotalSize() { return mQueue.GetTotalSize(); }
        FORCE_INLINE std::size_t GetTotalCapacity() { return mQueue.GetTotalCapacity(); }

private :
        std::size_t mIdx = 0;
        mutex_type  mMutex;
        DoubleQueue<value_type, mutex_type, cacheSizeLimit> mQueue;
        queue_type* mCurQueue = mQueue.GetQueue();
};

// vim: se enc=utf8 expandtab ts=8
