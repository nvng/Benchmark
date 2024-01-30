#pragma once

#include "Tools/Singleton.hpp"
#include "Tools/Util.h"
#include "Tools/LogHelper.h"

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/containers/set.hpp>
#include <boost/interprocess/containers/list.hpp>

#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/smart_ptr/shared_ptr.hpp>
#include <boost/interprocess/smart_ptr/weak_ptr.hpp>
#include <boost/interprocess/smart_ptr/deleter.hpp>

template <typename T>
struct SharedMemoryVector
{
        typedef boost::interprocess::allocator<T, boost::interprocess::managed_shared_memory::segment_manager> allocator_type;
        typedef boost::interprocess::vector<T, allocator_type> type;

        typedef typename boost::interprocess::managed_shared_ptr<type, boost::interprocess::managed_shared_memory::segment_manager>::type shared_ptr_type;
        typedef typename boost::interprocess::managed_weak_ptr<type, boost::interprocess::managed_shared_memory::segment_manager>::type weak_ptr_type;
};

template <typename K, typename V>
struct SharedMemoryMap
{
        typedef boost::interprocess::allocator<V, boost::interprocess::managed_shared_memory::segment_manager> allocator_type;
        typedef boost::interprocess::map<K, V, std::less<K>, allocator_type> type;

        typedef typename boost::interprocess::managed_shared_ptr<type, boost::interprocess::managed_shared_memory::segment_manager>::type shared_ptr_type;
        typedef typename boost::interprocess::managed_weak_ptr<type, boost::interprocess::managed_shared_memory::segment_manager>::type weak_ptr_type;
};

template <typename T>
struct SharedMemorySet
{
        typedef boost::interprocess::allocator<T, boost::interprocess::managed_shared_memory::segment_manager> allocator_type;
        typedef boost::interprocess::set<T, std::less<T>, allocator_type> type;

        typedef typename boost::interprocess::managed_shared_ptr<type, boost::interprocess::managed_shared_memory::segment_manager>::type shared_ptr_type;
        typedef typename boost::interprocess::managed_weak_ptr<type, boost::interprocess::managed_shared_memory::segment_manager>::type weak_ptr_type;
};

template <typename T>
struct SharedMemoryList
{
        typedef boost::interprocess::allocator<T, boost::interprocess::managed_shared_memory::segment_manager> allocator_type;
        typedef boost::interprocess::list<T, allocator_type> type;

        typedef typename boost::interprocess::managed_shared_ptr<type, boost::interprocess::managed_shared_memory::segment_manager>::type shared_ptr_type;
        typedef typename boost::interprocess::managed_weak_ptr<type, boost::interprocess::managed_shared_memory::segment_manager>::type weak_ptr_type;
};

template <typename T, typename _Tag>
class DoubleQueueShm
{
public:
        typedef boost::interprocess::interprocess_mutex mutex_type;
        typedef typename SharedMemoryVector<T>::type queue_type;

        DoubleQueueShm(boost::interprocess::managed_shared_memory& mgr)
        {
                std::string prefix = typeid(_Tag).name();
                mMutex = mgr.find_or_construct<mutex_type>((prefix + "m").c_str())();

                const typename queue_type::allocator_type alloc(mgr.get_segment_manager());
                mPushQueue = mgr.find_or_construct<queue_type>((prefix + "Push").c_str())(alloc);
                mPopQueue = mgr.find_or_construct<queue_type>((prefix + "Pop").c_str())(alloc);
        }

        ~DoubleQueueShm()
        {
        }

        typedef T value_type;

        template <typename... Args>
        FORCE_INLINE void Push(const Args&... args) { std::lock_guard<mutex_type> lock(*mMutex); mPushQueue->emplace_back(std::forward<const Args>(args)...); }
        FORCE_INLINE void Push(value_type&& val) { std::lock_guard<mutex_type> lock(*mMutex); mPushQueue->emplace_back(std::move(val)); }

        FORCE_INLINE queue_type* GetQueue()
        {
                mPopQueue->clear();
                {
                        std::lock_guard<mutex_type> lock(*mMutex);
                        mPopQueue->swap(*mPushQueue);
                }
                return mPopQueue;
        }

private:
        mutex_type* mMutex = nullptr;
        queue_type* mPushQueue = nullptr;
        queue_type* mPopQueue = nullptr;

protected:
        DISABLE_COPY_AND_ASSIGN(DoubleQueueShm);
};

enum class EShmConstructType
{
        E_SHM_CT_Construct,
        E_SHM_CT_Find,
        E_SHM_CT_FindOrConstruct,
};

template <typename _Tag, size_t size>
class SharedMemoryMgr : public Singleton<SharedMemoryMgr<_Tag, size>>
{
        SharedMemoryMgr()
                : _segment(boost::interprocess::open_or_create, typeid(_Tag).name(), size)
        {
        }

        ~SharedMemoryMgr() {}
        friend class Singleton<SharedMemoryMgr>;

public :
        bool Init(const std::string& prefix)
        {
                _prefix = prefix;
                return true;
        }

        template <typename T, typename Tag, EShmConstructType Ct = EShmConstructType::E_SHM_CT_FindOrConstruct>
        inline auto ConstructVector() { return ConstructInternal<typename SharedMemoryVector<T>::type, Tag, Ct>(); }

        template <typename K, typename V, typename Tag, EShmConstructType Ct = EShmConstructType::E_SHM_CT_FindOrConstruct>
        inline auto ConstructMap() { return ConstructInternal<typename SharedMemoryMap<K, V>::type, Tag, Ct>(); }

        template <typename T, typename Tag, EShmConstructType Ct = EShmConstructType::E_SHM_CT_FindOrConstruct>
        inline auto ConstructSet() { return ConstructInternal<typename SharedMemorySet<T>::type, Tag, Ct>(); }

        template <typename T, typename Tag, EShmConstructType Ct = EShmConstructType::E_SHM_CT_FindOrConstruct>
        inline auto ConstructList() { return ConstructInternal<typename SharedMemoryList<T>::type, Tag, Ct>(); }

        template <typename T, typename Tag>
        inline auto ConstructDoubleQueue() { return std::make_shared<DoubleQueueShm<T, Tag>>(_segment); }

        template <typename ContainerType, typename Tag, EShmConstructType Ct>
        inline auto ConstructInternal()
        {
                auto ret = boost::interprocess::make_managed_shared_ptr((ContainerType*)nullptr, _segment);
                try
                {
                        assert(!_prefix.empty());
                        const std::string tagStr = _prefix + typeid(Tag).name();
                        const typename ContainerType::allocator_type alloc(_segment.get_segment_manager());
                        if constexpr (EShmConstructType::E_SHM_CT_Construct == Ct)
                                ret = boost::interprocess::make_managed_shared_ptr(_segment.construct<ContainerType>(tagStr.c_str())(alloc), _segment);
                        else if constexpr (EShmConstructType::E_SHM_CT_Find == Ct)
                                ret = boost::interprocess::make_managed_shared_ptr(_segment.find<ContainerType>(tagStr.c_str())(alloc), _segment);
                        else if constexpr (EShmConstructType::E_SHM_CT_FindOrConstruct == Ct)
                                ret = boost::interprocess::make_managed_shared_ptr(_segment.find_or_construct<ContainerType>(tagStr.c_str())(alloc), _segment);
                }
                catch (const boost::interprocess::interprocess_exception& e)
                {
                        LOG_ERROR("boost::interprocess::interprocess_exception {}", e.what());
                }
                catch (const std::exception& e)
                {
                        LOG_ERROR("std::exception {}", e.what());
                }
                catch (...)
                {
                        LOG_ERROR("SharedMemoryMgr other exception!!!");
                }

                return ret;
        }

private:
        std::string _prefix;
        boost::interprocess::managed_shared_memory _segment;
};

// vim: fenc=utf8:expandtab:ts=8:noma
