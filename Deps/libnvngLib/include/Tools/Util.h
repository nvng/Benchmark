#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <random>
#include <memory>

#define FMT_USE_INTERNAL
#define FMT_HEADER_ONLY
#define FMT_STRING_ALIAS        1
#include <fmt/format.h>
#include <fmt/printf.h>
using namespace fmt::literals;

struct stDefaultTag;

#include "LogHelper.h"

#define INVALID_SERVER_SID -1
#define INVALID_SERVER_RID -1
#define INVALID_SERVER_IDX -1

#define FORCE_INLINE BOOST_FORCEINLINE

#ifndef DISABLE_COPY_AND_ASSIGN
#define DISABLE_COPY_AND_ASSIGN(ct) \
        private : \
        ct(ct&) = delete; \
        ct(ct&&) = delete; \
        ct& operator=(ct&) = delete; \
        ct& operator=(ct&&) = delete
#endif

#define FULL_DOUBLE(x) std::fixed << std::setprecision(7) << (x)
#define CHECK_2N(x)      ((x)>0 && 0 == ((x)&((x)-1)))

constexpr static FORCE_INLINE uint64_t Next2N(uint64_t x)
{
        --x;
        x |= x>>1;
        x |= x>>2;
        x |= x>>4;
        x |= x>>8;
        x |= x>>16;
        x |= x>>32;
        uint64_t ret = ++x;
        assert(CHECK_2N(ret));
        return ret;
}

extern thread_local std::mt19937_64 RandGen;

void PrintBit(const char* p, size_t size, int perLine = 10);

inline bool DoubleEqual(double lhs, double rhs)
{ return fabs(lhs - rhs) < 0.00001; }

template <typename T>
inline T LimitRange(T val, T min_, T max_)
{ if (max_ < min_) std::swap(min_, max_); return std::max(min_, std::min(val, max_)); }

// [low, up)
inline double RandInRange(double low, double up)
{ if (up < low) std::swap(low, up); return std::uniform_real_distribution<double>(low, up)(RandGen); }

// [low, up)
template <typename T>
inline T RandInRange(T low, T up)
{ if (up < low) std::swap(low, up); return std::uniform_int_distribution<T>(low, up - 1)(RandGen); }

template <typename _Ty>
inline std::vector<_Ty> Random(std::vector<_Ty>& tList, int64_t x)
{
        assert(x > 0);
        if (x <= 0)
                return std::vector<_Ty>();

        if (static_cast<int64_t>(tList.size()) > x)
        {
                std::vector<_Ty> retList;
                retList.reserve(x);
                for (int i=0; i<x; ++i)
                {
                        int64_t idx = RandInRange(0, tList.size());
                        retList.emplace_back(tList[idx]);
                        tList[idx] = tList[tList.size() - 1];
                        tList.pop_back();
                }
                return retList;
        }
        else
        {
                return std::move(tList);
        }
}

std::string GenRandStr(int64_t size);

#define FLAG_ADD(val, flag) ((val) |= (flag))
#define FLAG_SET(val, flag) ((val) = (flag))
#define FLAG_DEL(val, flag) ((val) &= ~(flag))
#define FLAG_HAS(val, flag) (((val) & (flag)) == (flag))

// {{{ tokenizer

#include <vector>
void TokenizerBase(std::vector<std::string>& Des,
        const std::string& str,
        const std::string& delimiters);

#include <boost/lexical_cast.hpp>
#include <google/protobuf/generated_enum_util.h>
template <typename T>
inline bool check_and_lexical_cast(const std::string& str, T& out)
{
        try
        {
                if constexpr (google::protobuf::is_proto_enum<T>::value)
                {
                        auto t = boost::lexical_cast<int64_t>(str);
                        out = static_cast<T>(t);
                }
                else
                {
                        out = boost::lexical_cast<T>(str);
                }
                return true;
        }
        catch (boost::bad_lexical_cast&)
        {
                return false;
        }
}

std::vector<std::string> Tokenizer(const std::string& str, const std::string& delimiters);

template <typename T>
inline std::vector<T> Tokenizer(const std::string& str, const std::string& delimiters)
{
        std::vector<std::string> strList = Tokenizer(str, delimiters);

        std::vector<T> retList;
        retList.reserve(strList.size());

        T out;
        for (auto& s : strList)
        {
                if (check_and_lexical_cast(s, out))
                        retList.emplace_back(out);
        }
        
        return retList;
}

bool ReadTo(std::stringstream& ss, const std::string& sp);
bool ReadFileToSS(std::stringstream& ss, const std::string& fileName);
bool CheckConfigEnd(std::stringstream& ss, std::string fileName);

#include "rapidjson/document.h"
bool ReadFileToJson(rapidjson::Document& root, const std::string& fileName);

template <typename _Ty>
concept StringstreamSpecialOutputConcept = !std::is_same<std::string, _Ty>::value
                                        && !std::is_floating_point<_Ty>::value
                                        && !std::is_integral<_Ty>::value;

template <typename _Ty>
concept StringstreamTypeConcept = std::is_same<std::stringstream, _Ty>::value
                           || std::is_same<std::basic_istream<char>, _Ty>::value;

template <StringstreamTypeConcept _Sy, StringstreamSpecialOutputConcept _Ty>
inline _Sy& operator>>(_Sy& ss, _Ty& dst)
{
        int64_t t = 0;
        ss >> t;
        dst = static_cast<_Ty>(t);
        return ss;
}

#include <set>
template <StringstreamTypeConcept _Sy, typename _Ty>
inline _Sy& operator>>(_Sy& ss, std::set<_Ty>& dst)
{
        std::vector<_Ty> tmpList;
        ss >> tmpList;
        for (_Ty v : tmpList)
                dst.emplace(v);
        return ss;
}

#include <unordered_set>
template <StringstreamTypeConcept _Sy, typename _Ty>
inline _Sy& operator>>(_Sy& ss, std::unordered_set<_Ty>& dst)
{
        std::vector<_Ty> tmpList;
        ss >> tmpList;
        for (_Ty v : tmpList)
                dst.emplace(v);
        return ss;
}

template <StringstreamTypeConcept _Sy, typename _Ty>
inline _Sy& operator>>(_Sy& ss, std::vector<_Ty>& dst)
{
        std::string str;
        ss >> str;
        dst = Tokenizer<_Ty>(str, "|");
        if (0 == dst[0])
                dst.clear();
        return ss;
}

template <StringstreamTypeConcept _Sy, typename _Ty>
inline _Sy& operator>>(_Sy& ss, std::vector<std::vector<_Ty>>& dst)
{
        std::string str;
        ss >> str;
        for (auto& tStr : Tokenizer<std::string>(str, "_"))
        {
                auto tList = Tokenizer<_Ty>(tStr, "|");
                if (0 != tList[0])
                        dst.emplace_back(tList);
        }
        return ss;
}

// }}}

std::string MD5(const char* const buffer, std::size_t buffer_size);
std::string MD5(std::string_view data);

double ConvertDoubleFromE(std::string str); // 科学计数法

int64_t CheckUTF8WordLength(const char* s);
int64_t CheckUTF8WordCnt(const std::string& str);

template <typename _Ty>
_Ty GetValueOrLast(const std::vector<_Ty>& tList, int64_t idx)
{
        if (!tList.empty())
        {
        if (0<=idx && idx<static_cast<int64_t>(tList.size()))
                return tList[idx];
        else
                return tList[tList.size()-1];
        }
        else
        {
                static const _Ty emptyRet = _Ty();
                return emptyRet;
        }
}

class AutoRelease final
{
public:
        typedef std::function<void(void)> DealFuncType;
        AutoRelease(const DealFuncType& create_func, const DealFuncType& release_func);
        AutoRelease(const DealFuncType& release_func);

        ~AutoRelease();

private:
        DealFuncType release_func_;
};
typedef std::shared_ptr<AutoRelease> AutoReleasePtr;

// {{{ Lock
template <typename _Ly, typename _Ky>
struct stLockGuard
{
        FORCE_INLINE stLockGuard(_Ly& l, _Ky key, const std::string& flag) : _l(l)
        {
                if (!_l.try_lock())
                {
                        DLOG_INFO("11111111111111111111 try_lock fail!!! key[{}] flag[{}]", key, flag);
                        _l.lock();
                }
        }
        FORCE_INLINE ~stLockGuard() { _l.unlock(); }

        _Ly& _l;
};

#define LOCK_GUARD(l, key, flag)        stLockGuard<decltype(l), decltype(key)> lockGuard##key(l, key, flag)
// #define LOCK_GUARD(l, key, flag)        std::lock_guard<decltype(l)> lockGuard##key##flag(l)

// boost spinlock 有bug。
class SpinLock
{
public :
        SpinLock() { }
        FORCE_INLINE void lock() { while (_l.test_and_set(std::memory_order_acquire)) { } }
        FORCE_INLINE bool try_lock() { return !_l.test_and_set(std::memory_order_acquire); }
        FORCE_INLINE void unlock() { _l.clear(std::memory_order_release); }
private :
        std::atomic_flag _l = ATOMIC_FLAG_INIT;
        DISABLE_COPY_AND_ASSIGN(SpinLock);
};

class NullMutex
{
public :
        NullMutex() { }
	FORCE_INLINE void lock() { }
        FORCE_INLINE bool try_lock() { return true; }
	FORCE_INLINE void unlock() { }
private :
        DISABLE_COPY_AND_ASSIGN(NullMutex);
};
// }}}

#define DECLARE_SHARED_FROM_THIS(ct) \
        protected : \
        FORCE_INLINE std::shared_ptr<ct> shared_from_this() \
        { return std::reinterpret_pointer_cast<ct>(SuperType::shared_from_this()); }

#define DECLARE_BOOST_SHARED_FROM_THIS(ct) \
        protected : \
        FORCE_INLINE boost::shared_ptr<ct> shared_from_this() \
        { return boost::reinterpret_pointer_cast<ct>(SuperType::shared_from_this()); }

// {{{ base64

#include <boost/beast/core/detail/base64.hpp>

inline std::pair<std::shared_ptr<char[]>, std::size_t> Base64EncodeExtra(const char* buf, std::size_t bufSize)
{
        auto ret = std::make_shared<char[]>(boost::beast::detail::base64::encoded_size(bufSize));
        auto retSize = boost::beast::detail::base64::encode(ret.get(), buf, bufSize);
        return { ret, retSize };
}

inline std::pair<std::shared_ptr<char[]>, std::size_t> Base64EncodeExtra(const std::string_view buf)
{ return Base64EncodeExtra(buf.data(), buf.length()); }

inline std::pair<std::shared_ptr<char[]>, std::size_t> Base64DecodeExtra(const char* buf, std::size_t bufSize)
{
        auto ret = std::make_shared<char[]>(boost::beast::detail::base64::decoded_size(bufSize));
        auto result = boost::beast::detail::base64::decode(ret.get(), buf, bufSize);
        return { ret, result.first };
}

inline std::pair<std::shared_ptr<char[]>, std::size_t> Base64DecodeExtra(const std::string_view buf)
{ return Base64DecodeExtra(buf.data(), buf.length()); }

inline std::string Base64Encode(const char* buf, std::size_t bufSize)
{
        std::string ret;
        ret.resize(boost::beast::detail::base64::encoded_size(bufSize));
        ret.resize(boost::beast::detail::base64::encode(&ret[0], buf, bufSize));
        return ret;
}

inline std::string Base64Encode(std::string_view buf)
{ return Base64Encode(buf.data(), buf.length()); }

inline std::string Base64Decode(const char* buf, std::size_t bufSize)
{
        std::string ret;
        ret.resize(boost::beast::detail::base64::decoded_size(bufSize));
        auto result = boost::beast::detail::base64::decode(&ret[0], buf, bufSize);
        ret.resize(result.first);
        return ret;
}

inline std::string Base64Decode(std::string_view buf)
{ return Base64Decode(buf.data(), buf.length()); }

// }}}

const std::string RSA1Sign(const std::string& content, const std::string& key);
int32_t RSA1VerifyString(const std::string& signString, const std::string& sign, const std::string& publicKey);

std::string RunShellCmd(const std::string& cmd);
std::string RunPHPFile(const std::string& phpFileName, const std::vector<std::string>& paramList);

// {{{ SetThreadName

#ifdef __linux__
#include <sys/prctl.h>
#define SetThreadName(f, ...) do { \
                auto threadName = fmt::format(f, ## __VA_ARGS__); \
                prctl(PR_SET_NAME, threadName.c_str()); \
        } while (0)
#endif

// }}}

#include <sched.h>
bool BindThreadToCpuCore(int32_t i = -1);

std::string GetIP(int32_t family, const std::string& name);

class Proc;
Proc*& GetCurProcRef();
Proc* GetCurProc();

static constexpr std::size_t GetThreadLocalBuffSize() { return 1024 * 1024; }
inline char* MallocThreadLocalBuff(std::size_t s)
{
        assert(nullptr != GetCurProc());
        if (s <= GetThreadLocalBuffSize())
        {
                static thread_local char buf[GetThreadLocalBuffSize()];
                return buf;
        }
        return new char[s];
}

inline void ReleaseThreadLocalBuff(const char* buf) { if (buf != MallocThreadLocalBuff(0)) delete[] buf; }

// {{{ namespace Compress
namespace Compress
{
enum ECompressType
{
        E_CT_None,
        E_CT_ZLib,
        E_CT_Zstd,
        E_CT_LZ4,
};

template <ECompressType _Ty>
inline bool NeedCompress(int64_t s) { return s >= 128; }

template <ECompressType _Ty> int64_t CompressedSize(int64_t size) { return 0; }
template <ECompressType _Ty> inline int64_t UnCompressedSize(const char* src) { return *(uint32_t*)src; }

template <ECompressType _Ty> bool
Compress(const char* dst, int64_t& dstSize, const char* src, int64_t srcSize) { return true; }
template <ECompressType _Ty> bool
UnCompress(const char* dst, const char* src, int64_t srcSize) { return true; }

template <ECompressType _Ty> char*
AllocUnCompressBuf(const char* buf, int64_t len)
{
        if (len <= 0)
                return nullptr;
        char* ret = new char[len + UnCompressedSize<_Ty>(buf)];
        memcpy(ret, buf, len);
        return ret;
}

template <ECompressType _Ty> bool
UnCompressAndParseExtra(google::protobuf::MessageLite& pb, std::string_view data)
{
        if (data.empty())
                return true;
        char* buf = (char*)data.data();
        auto size = UnCompressedSize<_Ty>(buf);
        bool r = UnCompress<_Ty>(buf+data.length(), buf, data.length());
        if (r)
        {
                bool ret = pb.ParseFromArray(buf+data.length(), size);
                LOG_ERROR_IF(!ret, "protobuf 反序列化失败!!!");
                return ret;
        }
        else
        {
                LOG_ERROR("解压失败 ct[{}]!!!", _Ty);
        }
        return false;
}

template <ECompressType _Ty> bool
UnCompressAndParseAlloc(google::protobuf::MessageLite& pb, const char* src, int64_t srcSize)
{
        if (srcSize <= 0)
                return true;

        auto size = UnCompressedSize<_Ty>(src);
        auto tBuf = new char[size+1];
        bool r = UnCompress<_Ty>(tBuf, src, srcSize);
        if (r)
        {
                bool ret = pb.ParseFromArray(tBuf, size);
                LOG_ERROR_IF(!ret, "protobuf 反序列化失败!!!");
                delete[] tBuf;
                return ret;
        }
        else
        {
                LOG_ERROR("解压失败 ct[{}]!!!", _Ty);
                delete[] tBuf;
                return false;
        }
}

template <ECompressType _Ty> FORCE_INLINE bool
UnCompressAndParseAlloc(google::protobuf::MessageLite& pb, std::string_view data)
{ return UnCompressAndParseAlloc<_Ty>(pb, data.data(), data.length()); }

template <ECompressType _Ty> std::pair<std::shared_ptr<char[]>, std::size_t>
SerializeAndCompress(const google::protobuf::MessageLite& pb, int64_t extraLen=0)
{
        const int64_t pbSize = pb.ByteSizeLong();
        auto compressedSize = CompressedSize<_Ty>(pbSize);
        auto buf = std::make_shared<char[]>(extraLen+compressedSize+pbSize);
        if (pb.SerializeToArray(buf.get()+compressedSize+extraLen, pbSize))
        {
                bool r = Compress<_Ty>(buf.get()+extraLen, compressedSize, buf.get()+compressedSize+extraLen, pbSize);
                if (r)
                        // return std::string_view((const char*)buf, compressedSize+extraLen);
                        return { buf, compressedSize+extraLen };
                else
                        LOG_ERROR("压缩失败!!! ct[{}]", _Ty);
        }
        else
        {
                LOG_ERROR("protobuf 序列化失败!!!");
        }

        return { nullptr, 0 };
}

template <ECompressType _Ft, ECompressType _Tt> std::string_view
CovertCompress(const char* src, int64_t srcSize, int64_t extraLen=0)
{
        auto uncompressedSize = UnCompressedSize<_Ft>(src);
        auto dstSize = CompressedSize<_Tt>(uncompressedSize);
        char* tmpBuf = new char[extraLen + dstSize + uncompressedSize];
        if (UnCompress<_Ft>(tmpBuf+extraLen+dstSize, src, srcSize))
        {
                if (Compress<_Tt>(tmpBuf+extraLen, dstSize, tmpBuf+extraLen+dstSize, uncompressedSize))
                        return std::string_view((const char*)tmpBuf, dstSize);
        }

        delete[] tmpBuf;
        return std::string_view();
}

// {{{ zlib
template <> inline bool NeedCompress<E_CT_ZLib>(int64_t s) { return s >= 128; }
template <> int64_t CompressedSize<E_CT_ZLib>(int64_t size);
template <> bool Compress<E_CT_ZLib>(const char* dst, int64_t& dstSize, const char* src, int64_t srcSize);
template <> bool UnCompress<E_CT_ZLib>(const char* dst, const char* src, int64_t srcSize);
// }}}

// {{{ zstd
template <> inline bool NeedCompress<E_CT_Zstd>(int64_t s) { return s >= 128; }
template <> int64_t CompressedSize<E_CT_Zstd>(int64_t size);
template <> bool Compress<E_CT_Zstd>(const char* dst, int64_t& dstSize, const char* src, int64_t srcSize);
template <> bool UnCompress<E_CT_Zstd>(const char* dst, const char* src, int64_t srcSize);
// }}}

// {{{ lz4
// 压缩率低，速度最快。
template <> inline bool NeedCompress<E_CT_LZ4>(int64_t s) { return s >= 128; }
template <> int64_t CompressedSize<E_CT_LZ4>(int64_t size);
template <> bool Compress<E_CT_LZ4>(const char* dst, int64_t& dstSize, const char* src, int64_t srcSize);
template <> bool UnCompress<E_CT_LZ4>(const char* dst, const char* src, int64_t srcSize);
// }}}

inline int64_t UnCompressedSize(Compress::ECompressType ct, const char* src)
{
        switch (ct)
        {
        case E_CT_ZLib : return UnCompressedSize<E_CT_ZLib>(src); break;
        case E_CT_Zstd : return UnCompressedSize<E_CT_Zstd>(src); break;
        case E_CT_LZ4  : return UnCompressedSize<E_CT_LZ4>(src);  break;
        default: break;
        }
        return 0;
}

inline bool UnCompress(Compress::ECompressType ct, const char* dst, const char* src, int64_t srcSize)
{
        switch (ct)
        {
        case E_CT_ZLib : return UnCompress<E_CT_ZLib>(dst, src, srcSize); break;
        case E_CT_Zstd : return UnCompress<E_CT_Zstd>(dst, src, srcSize); break;
        case E_CT_LZ4  : return UnCompress<E_CT_LZ4>(dst, src, srcSize);  break;
        default: break;
        }
        return false;
}

inline bool UnCompressAndParseExtra(Compress::ECompressType ct, google::protobuf::MessageLite& pb, std::string_view data)
{
        switch (ct)
        {
        case E_CT_ZLib : return UnCompressAndParseExtra<E_CT_ZLib>(pb, data); break;
        case E_CT_Zstd : return UnCompressAndParseExtra<E_CT_Zstd>(pb, data); break;
        case E_CT_LZ4  : return UnCompressAndParseExtra<E_CT_LZ4>(pb, data);  break;
        default: break;
        }
        return pb.ParseFromArray(data.data(), data.length());
}

inline bool UnCompressAndParseAlloc(Compress::ECompressType ct, google::protobuf::MessageLite& pb, const char* src, int64_t srcSize)
{
        switch (ct)
        {
        case E_CT_ZLib : return UnCompressAndParseAlloc<E_CT_ZLib>(pb, src, srcSize); break;
        case E_CT_Zstd : return UnCompressAndParseAlloc<E_CT_Zstd>(pb, src, srcSize); break;
        case E_CT_LZ4  : return UnCompressAndParseAlloc<E_CT_LZ4>(pb, src, srcSize);  break;
        default: break;
        }
        return pb.ParseFromArray(src, srcSize);
}

inline bool UnCompressAndParseAlloc(Compress::ECompressType ct, google::protobuf::MessageLite& pb, std::string_view data)
{ return UnCompressAndParseAlloc(ct, pb, (const char*)data.data(), data.length()); }

} // }}} end namespace Compress

struct stActorMailBase : public google::protobuf::MessageLite
{
public :
  ~stActorMailBase() override = default;

private :
  std::string GetTypeName() const override { assert(false); return ""; }
  // google::protobuf::MessageLite* New() const override { assert(false); return nullptr; }
  google::protobuf::MessageLite* New(google::protobuf::Arena*) const override { assert(false); return nullptr; }
  void Clear() override { assert(false); }
  bool IsInitialized() const override { assert(false); return false; }
  void CheckTypeAndMergeFrom(const google::protobuf::MessageLite& other) override { assert(false); }
  // bool MergePartialFromCodedStream(google::protobuf::io::CodedInputStream* input) override { assert(false); return false; }
  google::protobuf::uint8* _InternalSerialize(google::protobuf::uint8* ptr, google::protobuf::io::EpsCopyOutputStream* stream) const override { assert(false); return nullptr; }
  size_t ByteSizeLong() const override { assert(false); return 0; }
  int GetCachedSize() const override { assert(false); return 0; }
};

template <typename T>
concept NonCopyableConcept = !std::is_trivially_copy_constructible<T>::value && !std::is_trivially_copy_assignable<T>::value;

template <typename T>
concept ArithmeticConcept = std::is_arithmetic<T>::value;

// {{{ 循环单链表
#define LINKED_LIST_IS_EMPTY(tail)              (nullptr == (tail))
#define LINKED_LIST_HEAD_BASE(tail, next)       ((tail)->next)

#define LINKED_LIST_PUSH_BASE(tail, item, next) \
        if (LINKED_LIST_IS_EMPTY(tail)) { (tail) = (item); LINKED_LIST_HEAD_BASE(tail, next) = (item); } \
        else { (item)->next = LINKED_LIST_HEAD_BASE(tail, next); LINKED_LIST_HEAD_BASE(tail, next) = (item); (tail) = (item); }

#define LINKED_LIST_PUSH_LIST_BASE(tail, list, next) \
        if (LINKED_LIST_IS_EMPTY(tail)) (tail) = (list); \
        else { auto* listHead = LINKED_LIST_HEAD_BASE(list, next); LINKED_LIST_HEAD_BASE(list, next) = LINKED_LIST_HEAD_BASE(tail, next); LINKED_LIST_HEAD_BASE(tail, next) = listHead; tail = list; }

#define LINKED_LIST_POP_BASE(tail, next) \
        if (tail == LINKED_LIST_HEAD_BASE(tail, next)) (tail) = nullptr; \
        else LINKED_LIST_HEAD_BASE(tail, next) = LINKED_LIST_HEAD_BASE(tail, next)->next;

#define LINKED_LIST_HEAD(tail)                  LINKED_LIST_HEAD_BASE(tail, next_)
#define LINKED_LIST_PUSH(tail, item)            LINKED_LIST_PUSH_BASE(tail, item, next_)
#define LINKED_LIST_PUSH_LIST(tail, list)       LINKED_LIST_PUSH_LIST_BASE(tail, list, next_)
#define LINKED_LIST_POP(tail)                   LINKED_LIST_POP_BASE(tail, next_)
// }}}

// {{{ 循环双向链表
#define DOUBLE_LIST_IS_EMPTY(head)      (nullptr == (head))
#define DOUBLE_LIST_TAIL(head)          ((head)->prev_)

#define DOUBLE_LIST_PUSH_FRONT(head, item) \
        if (DOUBLE_LIST_IS_EMPTY(head)) \
        { \
                (head) = (item); \
                (head)->next_ = (item); \
                (head)->prev_ = (item); \
        } \
        else \
        { \
                (item)->prev_ = DOUBLE_LIST_TAIL(head); \
                DOUBLE_LIST_TAIL(head)->next_ = (item); \
                (item)->next_ = (head); \
                (head)->prev_ = (item); \
                (head) = (item); \
        }

#define DOUBLE_LIST_POP_FRONT(head) \
        if (!DOUBLE_LIST_IS_EMPTY(head)) \
        { \
                DOUBLE_LIST_TAIL(head)->next_ = (head)->next_; \
                (head)->next_->prev_ = DOUBLE_LIST_TAIL(head); \
        }

#define DOUBLE_LIST_PUSH_BACK(head, item) \
        if (DOUBLE_LIST_IS_EMPTY(head)) \
        { \
                (head) = (item); \
                (head)->next_ = (item); \
                (head)->prev_ = (item); \
        } \
        else \
        { \
                DOUBLE_LIST_TAIL(head)->next_ = (item); \
                (item)->prev_ = DOUBLE_LIST_TAIL(head); \
                (head)->prev_ = (item); \
                (item)->next_ = (head); \
        }

#define DOUBLE_LIST_POP_BACK(head) \
        if (!DOUBLE_LIST_IS_EMPTY(head)) \
        { \
                head->prev_ = head->prev_->prev_; \
                head->prev_->prev_->next_ = head; \
        }

#define DOUBLE_LIST_DEL(item) \
        item->next_->prev_ = item->prev_; \
        item->prev_->next_ = item->next_
// }}}

#define DEFINE_MSG_TYPE_MERGE(offset) \
        constexpr static uint64_t scBitCnt = 16; \
        constexpr static uint64_t scArraySize = 1 << scBitCnt; \
        static_assert(0<offset && offset<scBitCnt); \
        constexpr static uint64_t scMainTypeMax = (1<<offset) - 1; \
        constexpr static uint64_t scSubTypeMax = (1<<(scBitCnt-offset)) - 1; \
        FORCE_INLINE static uint64_t MsgTypeMerge(uint64_t mt, uint64_t st) { \
                assert(0<=mt&&mt<=scMainTypeMax && 0<=st&&st<=scSubTypeMax); \
                LOG_ERROR_IF(!(0<=mt&&mt<=scMainTypeMax && 0<=st&&st<=scSubTypeMax), \
                             "msg type merge error!!! offset[{}] mMax[{:#x}] sMax[{:#x}] mt[{:#x}] st[{:#x}]", \
                             offset, scMainTypeMax, scSubTypeMax, mt, st); \
                return (mt<<(scBitCnt-offset)) | st; \
        } \
        template<uint64_t mt, uint64_t st> FORCE_INLINE static constexpr uint64_t MsgTypeMerge() { \
                static_assert(0<=mt&&mt<=scMainTypeMax && 0<=st&&st<=scSubTypeMax); \
                return (mt<<(scBitCnt-offset)) | st; \
        } \
        FORCE_INLINE static constexpr uint64_t MsgMainType(uint64_t msgType) { return msgType >> (scBitCnt-offset); } \
        FORCE_INLINE static constexpr uint64_t MsgSubType(uint64_t msgType) { return msgType & scSubTypeMax; }

enum EServerType
{
	E_ST_Lobby,
	E_ST_Gate,
	E_ST_Login,
	E_ST_Pay,
	E_ST_Game,
	E_ST_GameMgr,
	E_ST_DB,
	E_ST_GM,
        E_ST_Log,
	E_ST_RobotMgr,

	EServerType_ARRAYSIZE,
        E_ST_None,
};

inline bool EServerType_IsValid(EServerType t)
{ return E_ST_Lobby <= t && t <EServerType_ARRAYSIZE; }

struct stServerInfoBase
{
        virtual ~stServerInfoBase() { }

	int64_t _sid = 0;
	std::string _faName;
	std::string _ip;
	int64_t _workersCnt = 1;
	int64_t _netProcCnt = 1;
	std::vector<uint16_t> _portList;
};
typedef std::shared_ptr<stServerInfoBase> stServerInfoBasePtr;

// vim: fenc=utf8:expandtab:ts=8:noma
