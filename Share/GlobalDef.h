#pragma once

template <int16_t _Mx, int16_t _My>
union stPos2D
{
        constexpr static int16_t scMaxX = _Mx;
        constexpr static int16_t scMaxY = _My;
        constexpr static int16_t scInvalidPosX = static_cast<int16_t>(std::numeric_limits<int16_t>::max());
        constexpr static int16_t scInvalidPosY = scInvalidPosX;

        uint64_t _v = 0;
        int16_t _p[4];

        stPos2D() { _p[3] = false; }
        stPos2D(int16_t x, int16_t y)
        {
                _p[0] = x;
                _p[1] = y;
                _p[3] = IsValid(x, y);
        }

        inline void x(int16_t x) { _p[0] = x; _p[3] = IsValidX(x); }
        inline void y(int16_t y) { _p[1] = y; _p[3] = IsValidY(y); }
        inline void v(int64_t v) { _v = v; }
        inline int16_t x() const { return _p[0]; }
        inline int16_t y() const { return _p[1]; }
        inline int64_t v() const { return _v; }

        inline bool IsValid() const { return _p[3]; }
        inline static bool IsValidX(int16_t x) { return 0<=x&&x<_Mx; }
        inline static bool IsValidY(int16_t y) { return 0<=y&&y<_My; }
        inline static bool IsValid(int16_t x, int16_t y) { return IsValidX(x) && IsValidY(y); }
        
        inline bool operator!=(const stPos2D rhs) const { return _v != rhs._v; }
        inline bool operator==(const stPos2D rhs) const { return _v == rhs._v; }
        inline bool operator<(const stPos2D rhs) const { return _v < rhs._v; }

        template <typename _Py>
        inline void Pack(_Py& msg) const
        {
                msg.set_x(x());
                msg.set_y(y());
        }

        template <typename _Py>
        inline void UnPack(_Py& msg)
        {
                _p[0] = msg.x();
                _p[1] = msg.y();
                _p[3] = IsValid(x(), y());
        }
};

template <int16_t _Mx, int16_t _My, int16_t _Mz>
union stPos3D
{
        constexpr static int16_t scMaxX = _Mx;
        constexpr static int16_t scMaxY = _My;
        constexpr static int16_t scMaxZ = _Mz;
        constexpr static int16_t scInvalidPosX = static_cast<int16_t>(std::numeric_limits<int16_t>::max());
        constexpr static int16_t scInvalidPosY = scInvalidPosX;
        constexpr static int16_t scInvalidPosZ = scInvalidPosX;

        uint64_t _v = 0;
        int16_t _p[4];

        stPos3D() { _p[3] = false; }
        stPos3D(int16_t x, int16_t y, int16_t z)
        {
                _p[0] = x;
                _p[1] = y;
                _p[2] = z;
                _p[3] = IsValid(x, y, z);
        }

        inline int16_t x() const { return _p[0]; }
        inline int16_t y() const { return _p[1]; }
        inline int16_t z() const { return _p[2]; }
        inline uint64_t v() const { return _v; }

        inline bool IsValid() const { return _p[3]; }
        inline static bool IsValidX(int16_t x) { return 0<=x&&x<scMaxX; }
        inline static bool IsValidY(int16_t y) { return 0<=y&&y<scMaxY; }
        inline static bool IsValidZ(int16_t z) { return 0<=z&&z<scMaxZ; }
        inline static bool IsValid(int16_t x, int16_t y, int16_t z) { return IsValidX(x) && IsValidY(y) && IsValidZ(z); }
        
        inline bool operator!=(const stPos3D rhs) const { return _v != rhs._v; }
        inline bool operator==(const stPos3D rhs) const { return _v == rhs._v; }
        inline bool operator<(const stPos3D rhs) const { return _v < rhs._v; }

        template <typename _Py>
        inline void Pack(_Py& msg) const
        {
                msg.set_x(x());
                msg.set_y(y());
                msg.set_z(z());
        }

        template <typename _Py>
        inline void UnPack(_Py& msg)
        {
                _p[0] = msg.x();
                _p[1] = msg.y();
                _p[2] = msg.z();
                _p[3] = IsValid(x(), y(), z());
        }
};

template <typename _Ty>
struct stRandomInfo
{
        stRandomInfo() = default;
        explicit stRandomInfo(int64_t w, _Ty& v)
                : _w(w), _v(v)
        {
        }

        int64_t _w = 0;
        _Ty _v = _Ty();

	DISABLE_COPY_AND_ASSIGN(stRandomInfo);
};

template <typename _Ty>
using stRandomSelectType = RandomSelect2<stRandomInfo<_Ty>, int64_t, NullMutex>;

enum EPlayerLogModuleType
{
        E_LOG_MT_PVE = E_LOG_MT_UserDefine,
};

// vim: fenc=utf8:expandtab:ts=8:
