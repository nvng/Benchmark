#pragma once

#if 0

#include <thread>

#include "Util.h"

#define FMT_USE_INTERNAL

enum ENanoCacheValueType
{
        E_NCVT_none,

        E_NCVT_int8_t,
        E_NCVT_uint8_t,
        E_NCVT_int16_t,
        E_NCVT_uint16_t,
        E_NCVT_int32_t,
        E_NCVT_uint32_t,
        E_NCVT_int64_t,
        E_NCVT_uint64_t,
        E_NCVT_float,
        E_NCVT_double,

        E_NCVT_char,
        E_NCVT_string,
        E_NCVT_string_view,
};

#define FORCE_INLINE __attribute__((always_inline))

#define NANO_CACHE_UNPACK(t) \
        do { \
        argList.emplace_back(fmt::internal::make_arg<ctx>(((stNanoCacheWapper<t>*)(buf_ + idx))->val_)); \
        idx += sizeof(stNanoCacheWapper<t>); \
        }  while (0)

#define NANO_CACHE_UNPACK_CHAR() \
        do { \
        argList.emplace_back(fmt::internal::make_arg<ctx>(((NanoCacheWapperCharType*)(buf_ + idx))->val_)); \
        idx += sizeof(NanoCacheWapperCharType); \
        }  while (0)

#define NANO_CACHE_UNPACK_STRING(t) \
        do { \
                stNanoCacheWapper<t>* w = (stNanoCacheWapper<t>*)(buf_ + idx); \
                argList.emplace_back(fmt::internal::make_arg<ctx>(w->val_)); \
                idx += w->len_ + sizeof(stNanoCacheWapper<t>); \
        }  while (0)

#define NANO_CACHE_WAPPER(t) \
template <> struct stNanoCacheWapper<t> \
{ \
        FORCE_INLINE stNanoCacheWapper(size_t& idx, const t& val) : val_(val) { idx += sizeof(stNanoCacheWapper<t>); } \
        const uint8_t type_ = E_NCVT_##t; \
        t val_ = 0; \
}

#pragma pack(push, 1)

template <typename T>
struct stNanoCacheWapper
{
        stNanoCacheWapper() = default;

        const uint8_t type_ = E_NCVT_none;
        T val_;
};

template <>
struct stNanoCacheWapper<std::string_view>
{
        FORCE_INLINE stNanoCacheWapper(size_t& idx, const std::string_view& val)
                : len_(val.length())
        {
                idx += len_ + 1 + sizeof(stNanoCacheWapper<std::string_view>);
                memcpy(val_, val.data(), len_);
                val_[len_] = '\0';
        }

        const uint8_t type_ = E_NCVT_string_view;
        const uint16_t len_;
        char val_[0];
};

template <>
struct stNanoCacheWapper<std::string>
{
        FORCE_INLINE stNanoCacheWapper(size_t& idx, const std::string& val)
                : len_(val.length() + 1)
        {
                idx += len_ + sizeof(stNanoCacheWapper<std::string>);
                memcpy(val_, val.c_str(), len_);
        }

        const uint8_t type_ = E_NCVT_string;
        const uint16_t len_;
        char val_[0];
};

template <>
struct stNanoCacheWapper<const char*>
{
        FORCE_INLINE stNanoCacheWapper(size_t& idx, const char* val)
                : val_((char*)val)
        {
                idx += sizeof(stNanoCacheWapper<const char*>);
        }

        const uint8_t type_ = E_NCVT_char;
        char* val_ = 0;
};

NANO_CACHE_WAPPER(int8_t);
NANO_CACHE_WAPPER(uint8_t);
NANO_CACHE_WAPPER(int16_t);
NANO_CACHE_WAPPER(uint16_t);
NANO_CACHE_WAPPER(int32_t);
NANO_CACHE_WAPPER(uint32_t);
NANO_CACHE_WAPPER(int64_t);
NANO_CACHE_WAPPER(uint64_t);
NANO_CACHE_WAPPER(float);
NANO_CACHE_WAPPER(double);

#pragma pack(pop)

#ifndef NANO_CACHE_CHECK_THREAD_SAFE
#define NANO_CACHE_CHECK_THREAD_SAFE()  assert(std::this_thread::get_id() == thread_id_)
#endif

using ctx = fmt::format_context;
typedef std::vector<fmt::basic_format_arg<ctx>> FormatArgListType;

template<typename T, size_t cacheSize = 1024*1024, typename CharWapperType=stNanoCacheWapper<const char*>>
class NanoCache
{
public:
        typedef T ExtraDataType;
        typedef CharWapperType NanoCacheWapperCharType;

        FORCE_INLINE NanoCache()
                : buf_(new char[cacheSize])
        {
                thread_id_ = std::this_thread::get_id();
        }

        FORCE_INLINE NanoCache(NanoCache&& rhs)
                : buf_(rhs.buf_)
                , idx_(rhs.idx_)
                , cnt_(rhs.cnt_)
                , extra_(std::move(rhs.extra_))
        {
                NANO_CACHE_CHECK_THREAD_SAFE();

                thread_id_ = std::this_thread::get_id();
                rhs.buf_ = new char[cacheSize];
                rhs.idx_ = 0;
                rhs.cnt_ = 0;
        }

        NanoCache(const NanoCache&)
        {
                NANO_CACHE_CHECK_THREAD_SAFE();
                assert(false);
        }

        NanoCache& operator=(const NanoCache&)
        {
                NANO_CACHE_CHECK_THREAD_SAFE();
                assert(false);
                return *this;
        }

        ~NanoCache()
        {
                NANO_CACHE_CHECK_THREAD_SAFE();
                delete[] buf_;
                buf_ = nullptr;
                idx_ = 0;
                cnt_ = 0;
        }

        template <typename U, typename... Args>
        FORCE_INLINE void AddHead(const Args&... args)
        {
                NANO_CACHE_CHECK_THREAD_SAFE();
                new((buf_ + idx_))U(std::forward<const Args>(args)...);
                idx_ += sizeof(U);
        }

        template <typename H, typename... Args>
        FORCE_INLINE void Add(const H& h, const Args&... args)
        {
                NANO_CACHE_CHECK_THREAD_SAFE();
                new (buf_ + idx_) stNanoCacheWapper<H>(idx_, std::forward<const H>(h));
                Add(std::forward<const Args>(args)...);
        }

        template <typename... Args>
        FORCE_INLINE void Add(const char* c, const Args&... args)
        {
                NANO_CACHE_CHECK_THREAD_SAFE();
                new (buf_ + idx_) NanoCacheWapperCharType(idx_, c);
                Add(std::forward<const Args>(args)...);
        }

        FORCE_INLINE void Add() {}

public:
        static consteval size_t CalSize() { return 0; }

        template <typename... Args>
        static FORCE_INLINE size_t CalSize(const char*, const Args&... args)
        {
                return sizeof(NanoCacheWapperCharType) + CalSize(std::forward<const Args>(args)...);
        }

        template <typename... Args>
        static FORCE_INLINE size_t CalSize(const std::string_view& h, const Args&... args)
        {
                return h.length() + 1 + sizeof(stNanoCacheWapper<std::string_view>) + CalSize(std::forward<const Args>(args)...);
        }

        template <typename... Args>
        static FORCE_INLINE size_t CalSize(const std::string& h, const Args&... args)
        {
                return h.length() + 1 + sizeof(stNanoCacheWapper<std::string>) + CalSize(std::forward<const Args>(args)...);
        }

        template <ArithmeticConcept H, typename... Args>
        static FORCE_INLINE size_t CalSize(H, const Args&... args)
        {
                return sizeof(stNanoCacheWapper<H>) + CalSize(std::forward<const Args>(args)...);
        }

public :
        FORCE_INLINE FormatArgListType UnPack(size_t& idx, int32_t cnt)
        {
                NANO_CACHE_CHECK_THREAD_SAFE();
                FormatArgListType argList;
                argList.reserve(cnt);

                for (int32_t i = 0; i < cnt; ++i)
                {
                        stNanoCacheWapper<uint8_t>* w_ = (stNanoCacheWapper<uint8_t>*)(buf_ + idx);
                        switch (w_->type_)
                        {
                        case E_NCVT_string:      NANO_CACHE_UNPACK_STRING(std::string);      break;
                        case E_NCVT_string_view: NANO_CACHE_UNPACK_STRING(std::string_view); break;
                        case E_NCVT_char:        NANO_CACHE_UNPACK_CHAR();       break;
                        case E_NCVT_int8_t:      NANO_CACHE_UNPACK(int8_t);      break;
                        case E_NCVT_uint8_t:     NANO_CACHE_UNPACK(uint8_t);     break;
                        case E_NCVT_int16_t:     NANO_CACHE_UNPACK(int16_t);     break;
                        case E_NCVT_uint16_t:    NANO_CACHE_UNPACK(uint16_t);    break;
                        case E_NCVT_int32_t:     NANO_CACHE_UNPACK(int32_t);     break;
                        case E_NCVT_uint32_t:    NANO_CACHE_UNPACK(uint32_t);    break;
                        case E_NCVT_int64_t:     NANO_CACHE_UNPACK(int64_t);     break;
                        case E_NCVT_uint64_t:    NANO_CACHE_UNPACK(uint64_t);    break;
                        case E_NCVT_float:       NANO_CACHE_UNPACK(float);       break;
                        case E_NCVT_double:      NANO_CACHE_UNPACK(double);      break;
                        default:
                                break;
                        }
                }

                return argList;
        }

public :
        FORCE_INLINE void IncCnt() { NANO_CACHE_CHECK_THREAD_SAFE(); ++cnt_; }
        FORCE_INLINE size_t GetCnt() const { NANO_CACHE_CHECK_THREAD_SAFE(); return cnt_; }

        FORCE_INLINE ExtraDataType& GetExtraRef() { NANO_CACHE_CHECK_THREAD_SAFE(); return extra_; }

        FORCE_INLINE char* GetBuf() const { NANO_CACHE_CHECK_THREAD_SAFE(); return buf_; }

        FORCE_INLINE bool IsEmpty() const { NANO_CACHE_CHECK_THREAD_SAFE(); return 0 == idx_; }
        FORCE_INLINE size_t GetSize() const { NANO_CACHE_CHECK_THREAD_SAFE(); return idx_; }
        FORCE_INLINE bool CheckSize(size_t size) const { NANO_CACHE_CHECK_THREAD_SAFE(); return idx_ + size <= cacheSize; }

        FORCE_INLINE void Clear() { NANO_CACHE_CHECK_THREAD_SAFE(); idx_ = 0; }

private :
        char* buf_ = nullptr;
        size_t idx_ = 0;
        size_t cnt_ = 0;
        ExtraDataType extra_;
        std::thread::id thread_id_;
};

#endif

// vim: fenc=utf8:expandtab:ts=8:
