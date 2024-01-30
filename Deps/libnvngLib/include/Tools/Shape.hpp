#pragma once

#include <boost/assign.hpp>
#include <boost/geometry/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/geometries/ring.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/algorithms/detail/azimuth.hpp>

namespace Shape {

namespace bg = boost::geometry;
typedef bg::model::d2::point_xy<double> Point;
typedef bg::model::linestring<Point> LineString;
typedef bg::model::box<Point> Box;
//这里的ring就是我们通常说的多边形闭合区域(内部不存在缕空)，模板参数为true，表示顺时针存储点，为false，表示逆时针存储点，由于MM_TEXT坐标系与传统上的坐标系的Y轴方向是相的，所以最后为false，将TopLeft、TopRight、BottomRight、BottomLeft、TopLeft以此存储到ring中，以便能正确计算
typedef bg::model::ring<Point, false> Ring;
// polygon 模板参数 false，也是由上面相同的原因得出来的
typedef bg::model::polygon<Point, false> Polygon;

constexpr double PI = 3.14;

enum EShapType
{
        E_ST_None = 0,
        E_ST_Circle = 1, // 圆形。
        E_ST_Sector = 2, // 扇形。
        E_ST_Rectangle = 3, // 矩形。
};

class IShape
{
public :
        virtual ~IShape() { }

        virtual void Print() { }

        virtual bool WithIn(const Point& pnt) { return false; }
        virtual void Rotate(double angle) { }
        virtual void Translate(double x, double y) { }
        virtual void Scale(double s) { }
};

class Circle : public IShape
{
public :
        Circle(const Point& pnt, int64_t r)
                : _center(pnt), _radius(r)
        {
        }

        ~Circle() override { }

        void Print() override
        {
                LOG_INFO("Circle center[{},{}] r[{}]", _center.x(), _center.y(), _radius);
        }

        bool WithIn(const Point& pnt) override
        { return bg::distance(_center, pnt) <= _radius; }

private :
        Point _center;
        double _radius = 0.0;
};

class Sector : public IShape
{
public :
        Sector(const Point& pnt, const Point& target, int64_t r, double angle)
                : _center(pnt), _target(target), _radius(r), _angle(angle)
        {
        }

        ~Sector() override { }

        void Print() override
        {
                LOG_INFO("Sector center[{},{}] target[{},{}] r[{}] angle[{}]",
                         _center.x(), _center.y(), _target.x(), _target.y(), _radius, _angle);
        }

        bool WithIn(const Point& pnt) override
        {
                if (bg::distance(_center, pnt) >= _radius)
                        return false;

                auto baseBearing = bg::detail::azimuth<double>(_center, _target);
                auto targetBearing = bg::detail::azimuth<double>(_center, pnt);
                auto diff = std::fabs(baseBearing - targetBearing);
                return diff <= (_angle * PI / 180.0 )/ 2.0;
        }

private :
        Point _center;
        Point _target;
        double _radius = 0.0;
        double _angle = 0.0;
};

template <typename _Ty>
class ShapeWapper : public IShape
{
public :
        ShapeWapper() = default;
        ~ShapeWapper() override { }

        bool WithIn(const Point& pnt) override
        { return bg::within(pnt, _obj); }

        void Rotate(double angle) override
        {
                _Ty src = std::move(_obj);
                bg::strategy::transform::rotate_transformer<bg::degree, double, 2, 2> rotate(angle);
                bg::transform(src, _obj, rotate);
        }

        void Translate(double x, double y) override
        {
                _Ty src = std::move(_obj);
                bg::strategy::transform::translate_transformer<double, 2, 2> rotate(x, y);
                bg::transform(src, _obj, rotate);
        }

        void Scale(double s) override
        {
                _Ty src = std::move(_obj);
                bg::strategy::transform::scale_transformer<double, 2, 2> scale(s);
                bg::transform(src, _obj, scale);
        }

protected :
        _Ty _obj;
};

class Ractangle : public ShapeWapper<Ring>
{
public :
        explicit Ractangle(const std::vector<Point>& pList)
        {
                _obj.assign(pList.begin(), pList.end());
        }

        static boost::shared_ptr<Ractangle> Create(const Point& src, const Point& dst, double w, double h=0.0, double b=0.0)
        {
                const double tw = w/2.0 + b;
                double th = h;
                if (DoubleEqual(0.0, th))
                        th = bg::distance(src, dst);
                th += b;
                std::vector<Point> pList = {
                        { tw, -b },
                        { tw, th },
                        { -tw, th },
                        { -tw, -b },
                        { tw, -b },
                };

                auto bearing = bg::detail::azimuth<double>(src, dst);
                auto ret = boost::make_shared<Ractangle>(pList);
                ret->Rotate(bearing * 180 / PI);
                ret->Translate(src.x(), src.y());
                return ret;
        }

        void Print() override
        {
                std::string str;
                for (int64_t i=0; i<static_cast<int64_t>(_obj.size()); ++i)
                        str += fmt::format("[{},{}] ", bg::get<0>(_obj[i]), bg::get<1>(_obj[i]));
                LOG_INFO("Ractangle {}", str);
        }
};

class Segment : public ShapeWapper<bg::model::segment<Point>>
{
public :
        Segment(const Point& src, const Point& dst)
        {
                _obj.first = src;
                _obj.second = dst;
        }

        static boost::shared_ptr<Segment> Create(const Point& src, const Point& dst, double distance)
        {
                auto ret = boost::make_shared<Shape::Segment>(Shape::Point(0, 0), Shape::Point(0, distance));
                auto bearing = bg::detail::azimuth<double>(src, dst);
                ret->Rotate(bearing * 180 / PI);
                ret->Translate(src.x(), src.y());
                return ret;
        }

        FORCE_INLINE const Point& First() const { return _obj.first; }
        FORCE_INLINE const Point& Second() const { return _obj.second; }
};

} // end namespace Shape

// vim: fenc=utf8:expandtab:ts=8
