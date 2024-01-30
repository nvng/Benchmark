#pragma once

#include <vector>
#include <unordered_map>

#include "Tools/Singleton.hpp"

struct stStringReplaceInfo
{
        FORCE_INLINE stStringReplaceInfo(const char* data, int32_t size, int32_t cnt)
                : _data(data), _size(size), _cnt(cnt)
        {
        }

        const char* _data = nullptr;
        const int32_t _size = 0;
        const int32_t _cnt = 0;
};

class WordFilter : public Singleton<WordFilter>
{
        WordFilter() = default;
        ~WordFilter() = default;
        friend class Singleton<WordFilter>;

public:
        bool Init(const std::vector<std::string>& specialWordList)
        {
                for (const std::string& str : specialWordList)
                {
                        const auto wordList = SplitWord(str);
                        uint64_t val = *reinterpret_cast<uint64_t*>((char*)str.c_str());
                        switch (wordList.size())
                        {
                        [[unlikely]] case 0:
                                continue;
                                break;
                        [[unlikely]] case 1:
                        {
                                val = CalValue(val, wordList[0].length());
                                break;
                        }
                        [[likely]] default:
                        {
                                val = CalValue(val, wordList[0].length() + wordList[1].length());
                                break;
                        }
                        }

                        auto& valPair = _specialWordList[val];
                        valPair.first.emplace(wordList.size());
                        valPair.second.emplace(std::string_view(str), str);
                }

                return true;
        }

        template <bool isReplace=false, bool fixCnt=false, char ch='*'>
        inline bool CheckString(std::string& str)
        {
                std::vector<stStringReplaceInfo> replaceInfoList;

                const auto wordList = SplitWord(str);
                const int32_t cnt = wordList.size();

                for (int32_t i = 0; i < cnt; ++i)
                {
                        const std::string_view& sv = wordList[i];
                        uint64_t val = *reinterpret_cast<uint64_t*>((char*)sv.data());

                        bool isFound = false;
                        if (i + 1 < cnt)
                        {
                                size_t len = sv.length() + wordList[i + 1].length();
                                auto it = _specialWordList.find(CalValue(val, len));
                                if (_specialWordList.end() != it)
                                {
                                        isFound = true;
                                        auto cntIt = it->second.first.rbegin();
                                        for (; it->second.first.rend() != cntIt; ++cntIt)
                                        {
                                                const int32_t cntVal = *cntIt;
                                                switch (cntVal)
                                                {
                                                [[unlikely]] case 0:
                                                [[unlikely]] case 1:
                                                {
                                                        assert(false);
                                                        break;
                                                }
                                                [[likely]] case 2:
                                                {
                                                        if constexpr (isReplace)
                                                        {
                                                                replaceInfoList.emplace_back(sv.data(), len, 2);
                                                                i += 2 - 1;
                                                                goto __continue__;
                                                        }
                                                        else
                                                        {
                                                                return false;
                                                        }
                                                }
                                                [[likely]] default:
                                                {
                                                        const int32_t idx = i + cntVal - 1;
                                                        if (idx >= cnt)
                                                                continue;

                                                        size_t tmpLen = wordList[idx].data() + wordList[idx].length() - sv.data();
                                                        std::string_view tmpSV(sv.data(), tmpLen);
                                                        auto tmpIt_ = it->second.second.find(tmpSV);
                                                        if (it->second.second.end() != tmpIt_)
                                                        {
                                                                if constexpr (isReplace)
                                                                {
                                                                        replaceInfoList.emplace_back(sv.data(), tmpLen, cntVal);
                                                                        i = idx;
                                                                        goto __continue__;
                                                                }
                                                                else
                                                                {
                                                                        return false;
                                                                }
                                                        }

                                                        break;
                                                }
                                                }
                                        }

                                        if constexpr (!isReplace)
                                                return false;
                                }
                        }

                        if (!isFound)
                        {
                                auto it = _specialWordList.find(CalValue(val, sv.length()));
                                if (_specialWordList.end() != it)
                                {
                                        if constexpr (isReplace)
                                        {
                                                replaceInfoList.emplace_back(sv.data(), sv.length(), 1);
                                        }
                                        else
                                        {
                                                return false;
                                        }
                                }
                        }

                        goto __continue__;
                __continue__:
                        ;
                }

                if constexpr (isReplace)
                {
                        if constexpr (fixCnt)
                        {
                                auto rIt = replaceInfoList.rbegin();
                                for (; replaceInfoList.rend() != rIt; ++rIt)
                                {
                                        stStringReplaceInfo& info = *rIt;
                                        str.replace(info._data - str.c_str(), info._size, info._cnt, ch);
                                }
                        }
                        else
                        {
                                for (stStringReplaceInfo& info : replaceInfoList)
                                        str.replace(info._data - str.c_str(), info._size, info._size, ch);
                        }
                }

                return true;
        }

        static inline std::vector<std::string_view> SplitWord(const std::string& str)
        {
                std::vector<std::string_view> retList;
                const int32_t num = str.size();
                retList.reserve(num/2);

                int32_t i = 0;
                int32_t s = 1;
                char ch;
                while (i < num)
                {
                        ch = str[i];
#if 0
                        s = 1;
                        if (ch & 0x80)
                        {
                                ch <<= 1;
                                do
                                {
                                        ch <<= 1;
                                        ++s;
                                } while (ch & 0x80);
                        }
#else
                        if (0xE0 == (ch & 0xE0))
                        {
                                // 111x xxxx
                                s = 3;
                        }
                        else
                        {
                                if (0 == (ch & 0x80))
                                {
                                        // 0xxx xxxx£¬¼´asciiÂë
                                        s = 1;
                                }
                                else if (0xF0 == (ch & 0xF0))
                                {
                                        // 1111 xxxx
                                        s = 4;
                                }
                                else if (0xC0 == (ch & 0xC0))
                                {
                                        // 11xx xxxx
                                        s = 2;
                                }
                                else if (0xF8 == (ch & 0xF8))
                                {
                                        // 1111 1xxx
                                        s = 5;
                                }
                                else
                                {
                                        // 1111 11xx
                                        s = 6;
                                }
                        }
#endif

                        retList.emplace_back(str.data() + i, s);
                        i += s;
                }

                return retList;
        }

        static FORCE_INLINE uint64_t CalValue(uint64_t val, size_t len)
        {
                switch (len)
                {
                [[likely]] case 1: return val & 0xFF; break;
                [[likely]] case 2: return val & 0xFFFF; break;
                [[likely]] case 3: return val & 0xFFFFFF; break;
                [[likely]] case 4: return val & 0xFFFFFFFF; break;
                [[unlikely]] case 5: return val & 0xFFFFFFFFFF; break;
                [[likely]] case 6: return val & 0xFFFFFFFFFFFF; break;
                [[unlikely]] case 7: return val & 0xFFFFFFFFFFFFFF; break;
                [[unlikely]] case 8: return val; break;
                [[unlikely]] default: break;
                }

                return val;
        }

private :
        typedef std::pair<std::set<int32_t>, std::map<std::string_view, std::string>> WordListValueType;
        std::unordered_map<uint64_t, WordListValueType> _specialWordList;
};

// vim: fenc=utf8:expandtab:ts=8:noma