#pragma once

#define FORCE_INLINE    BOOST_FORCEINLINE

template <typename _Tag, typename _Ty>
class GuidWapper
{
public:
        typedef _Ty value_type;

        GuidWapper() = default;
        ~GuidWapper() = default;

        FORCE_INLINE explicit GuidWapper(value_type d) : _data(d) {}

        FORCE_INLINE GuidWapper& operator=(const GuidWapper& rhs)
        {
                _data = rhs._data;
                return *this;
        }

        FORCE_INLINE GuidWapper& operator=(const value_type& rhs)
        {
                _data = rhs;
                return *this;
        }

public:
        value_type _data = value_type();
};

#define DEFINE_OPERATOR(ct, opt) \
template <typename _Tag, typename _Ty> \
FORCE_INLINE bool operator opt(const ct<_Tag, _Ty>& lhs, const typename ct<_Tag, _Ty>& rhs) \
{ return lhs._data opt rhs._data; } \
\
template <typename _Tag, typename _Ty> \
FORCE_INLINE bool operator opt(const typename ct<_Tag, _Ty>::value_type& lhs, const ct<_Tag, _Ty>& rhs) \
{ return lhs opt rhs._data; } \
\
template <typename _Tag, typename _Ty> \
FORCE_INLINE bool operator opt(const ct<_Tag, _Ty>& lhs, const typename ct<_Tag, _Ty>::value_type& rhs) \
{ return lhs._data opt rhs; }

DEFINE_OPERATOR(GuidWapper, < );
DEFINE_OPERATOR(GuidWapper, <= );
DEFINE_OPERATOR(GuidWapper, > );
DEFINE_OPERATOR(GuidWapper, >= );
DEFINE_OPERATOR(GuidWapper, == );
DEFINE_OPERATOR(GuidWapper, != );

#undef DEFINE_OPERATOR

template <typename _Tag, typename _Ty>
struct hash<GuidWapper<_Tag, _Ty>>
{
	typedef GuidWapper<_Tag, _Ty> argument_type;
	typedef std::size_t result_type;

    FORCE_INLINE result_type operator()(argument_type const& s) const
    {
        return std::hash<_Ty>()(s._data);
    }
};

// vim: se enc=utf8 expandtab ts=8
