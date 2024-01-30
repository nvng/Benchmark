#pragma once

#include <string>
#include <unordered_map>

#include "Tools/Singleton.hpp"
#include "Tools/DoubleQueue.hpp"
#include "Tools/Util.h"
#include "Tools/Timer.h"

#ifndef GM_PREFIX
#define GM_PREFIX       "//"
#endif

class GMMgr;
typedef void (GMMgr::* GMHandleType)(const std::vector<std::string>&);
struct stGMInfo
{
        std::string _cmd;
        GMHandleType _cb = nullptr;
        std::string _desc;
};

class GMMgr : public Singleton<GMMgr>
{
        GMMgr();
        virtual ~GMMgr();
        friend class Singleton<GMMgr>;

public:
        bool Init();
        void FrameUpdate();

        void OnRecvCmdFromClient(const std::string& cmdStr)
        {
                std::vector<std::string> params = Tokenizer(cmdStr, " ");

                if (params.empty())
                        return;

                std::string& cmd = params[0];
                if (GM_PREFIX != cmd.substr(0, 2))
                        return;

                _cmdList.Push(std::move(params));
        }

        void RegisterCmd(const std::string& cmd, const std::string& desc, GMHandleType cb);

private :
        std::unordered_map<std::string, stGMInfo> _GMInfoList;
        DoubleQueue<std::vector<std::string>> _cmdList;

#ifdef WIN32
        std::thread mThread;
#endif

public :
        template <typename T>
        void GMHandle(const std::vector<std::string>& params);
};

#define GM_HANDLE(cmd, desc) \
struct st_regist_cmd_handle_proxy_##cmd; \
template <> void GMMgr::GMHandle<st_regist_cmd_handle_proxy_##cmd>(const std::vector<std::string>& params); \
struct st_regist_cmd_handle_proxy_##cmd { \
    st_regist_cmd_handle_proxy_##cmd() { \
    GMMgr::GetInstance()->RegisterCmd(GM_PREFIX#cmd, #desc, &GMMgr::GMHandle<st_regist_cmd_handle_proxy_##cmd>); \
} \
} _regist_cmd_handle_proxy_##cmd; \
template <> void GMMgr::GMHandle<st_regist_cmd_handle_proxy_##cmd>(const std::vector<std::string> & params)

// vim: fenc=utf8:expandtab:ts=8:noma
