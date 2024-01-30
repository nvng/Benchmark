#pragma once

#include <set>

/**
 * @brief 分配可用的最小整数，以作数组下标。
 */
class SmallerInteger
{
public :
        explicit SmallerInteger(int maxSize)
                : mMaxSize(maxSize)
        {
                mCurInteger = -1;
        }

        /**
         * @return -1 表示分配失败
         */
        int GetInteger()
        {
                if (mIntegerList.empty())
                {
                        int ret = mCurInteger + 1;
                        if (ret >= mMaxSize)
                        {
                                return -1;
                        }

                        ++mCurInteger;
                        return ret;
                }
                else
                {
                        auto it = mIntegerList.begin();
                        int ret = *it;
                        mIntegerList.erase(it);
                        return ret;
                }
        }

        void ReleaseInteger(int val)
        {
                if (val>=0 && val<mMaxSize)
                        mIntegerList.insert(val);
        }

private :
        std::set<int> mIntegerList;

        /**
         * @brief 当前已分配最大值。
         */
        int mCurInteger;
        const int mMaxSize;

        static const int sBaseInteger = 0;
};

// vim: se enc=utf8 expandtab ts=8
