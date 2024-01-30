#pragma once

#include <limits>
#include <mutex>

template <typename _Ty, std::size_t _Smax = std::numeric_limits<_Ty>::max()>
class ArrayIndexPool final
{
public:
        ArrayIndexPool()
        {
                for (_Ty i = 0; i < static_cast<_Ty>(_Smax); ++i)
                {
                        _intList[i] = false;
                        _emptyList[i] = i;
                }
        }

        inline _Ty Malloc()
        {
                // std::lock_guard l(_mutex);
                if (0<=_idx && _idx<static_cast<_Ty>(_Smax))
                {
                        _Ty ret = _emptyList[_idx];
                        if (!_intList[ret])
                        {
                                ++_idx;
                                _intList[ret] = true;
                                return ret;
                        }
                        else
                        {
                                return _Smax;
                        }
                }
                else
                {
                        return _Smax;
                }
        }

        inline void Free(_Ty v)
        {
                // std::lock_guard l(_mutex);
                if (0<_idx && _idx<=static_cast<_Ty>(_Smax)
                    && 0<=v && v<static_cast<_Ty>(_Smax)
                    && _intList[v])
                {
                        _intList[v] = false;
                        _emptyList[--_idx] = v;
                }
        }

private:
        bool _intList[_Smax];
        _Ty _emptyList[_Smax];
        _Ty _idx = _Ty(0);
        SpinLock _mutex;

public :
        static const _Ty m_sInvalidIdx = _Smax;
private:
        ArrayIndexPool(const ArrayIndexPool &other) = delete;
        ArrayIndexPool &operator=(const ArrayIndexPool &other) = delete;
};

// vim: fenc=utf8:expandtab:ts=8:noma
