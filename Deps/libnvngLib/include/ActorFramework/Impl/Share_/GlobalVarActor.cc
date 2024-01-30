#include "GlobalVarActor.h"

namespace nl::af::impl {

std::string GlobalVarActor::_priorityTaskKey = "global_var";

SPECIAL_ACTOR_MAIL_HANDLE(GlobalVarActor, 0)
{
        std::string key = fmt::format("global:{}:{}", GetAppBase()->GetRID(), GetAppBase()->GetSID());
        std::lock_guard l(_globalVarListMutex);
        for (auto& val : _globalVarList)
                RedisCmd("HSET", key, val.first, val.second);

        // GetAppBase()->_stopPriorityTaskList->Finish(_priorityTaskKey);
        return nullptr;
}

}; // end of namespace nl::af::impl

// vim: fenc=utf8:expandtab:ts=8:noma
