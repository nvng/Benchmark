#pragma once

#include <iconv.h>

#include "Util.h"

namespace CodeChange
{
        inline std::string ConvertInternal(const std::string& srcStr, iconv_t cd)
        {
                std::string retStr;
                do 
                {
                        if ((size_t)-1 == (size_t)cd)
                                break;

                        char* src = (char*)srcStr.c_str();
                        size_t srcLen = srcStr.length();

                        auto* origDst = MallocThreadLocalBuff(0);
                        char* dst = reinterpret_cast<char*>(origDst);
                        size_t dstLen = GetThreadLocalBuffSize();

                        size_t ret = iconv(cd, &src, &srcLen, &dst, &dstLen);
                        if ((size_t)-1 == ret)
                        {
                                ReleaseThreadLocalBuff(origDst);
                                break;
                        }

                        retStr.assign(reinterpret_cast<char*>(origDst), GetThreadLocalBuffSize() - dstLen);
                        ReleaseThreadLocalBuff(origDst);
                } while (0);

                iconv_close(cd);
                return retStr;
        }

        inline std::string ConvertUTF8ToGBK(const std::string& utf8Str)
        { return ConvertInternal(utf8Str, iconv_open("GBK", "UTF-8")); }

        inline std::string ConvertGBKToUTF8(const std::string& gbkStr)
        { return ConvertInternal(gbkStr, iconv_open("UTF-8", "GBK")); }
}

// vim: fenc=utf8:expandtab:ts=8:noma
