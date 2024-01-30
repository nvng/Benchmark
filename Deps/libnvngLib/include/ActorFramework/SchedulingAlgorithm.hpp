#pragma once

#include <boost/fiber/algo/work_stealing.hpp>
#include <boost/fiber/algo/shared_work.hpp>

namespace nl::af
{

class work_stealing : public boost::fibers::algo::work_stealing
{
        typedef boost::fibers::algo::work_stealing SuperType;
public:
        work_stealing(std::uint32_t threadCnt)
                : SuperType(threadCnt, true)
        {
        }

        void suspend_until( std::chrono::steady_clock::time_point const& time_point) noexcept override
        {
                if (std::chrono::steady_clock::time_point::max() == time_point)
                        SuperType::suspend_until(std::chrono::steady_clock::now() + std::chrono::milliseconds(100));
                else
                        SuperType::suspend_until(time_point);
        }
};

class shared_work : public boost::fibers::algo::shared_work
{
        typedef boost::fibers::algo::shared_work SuperType;
public:
        shared_work()
                : SuperType(true)
        {
        }

        void suspend_until( std::chrono::steady_clock::time_point const& time_point) noexcept override
        {
                if (std::chrono::steady_clock::time_point::max() == time_point)
                        SuperType::suspend_until(std::chrono::steady_clock::now() + std::chrono::milliseconds(100));
                else
                        SuperType::suspend_until(time_point);
        }
};

}; // end of namespace nl::af

// vim: fenc=utf8:expandtab:ts=8:noma
