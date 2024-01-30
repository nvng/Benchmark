#include "ServerDiscoveryActor.h"

namespace nl::af::impl {

#if 0

std::string ServiceDistcoveryActor::_priorityTaskKey = "load_sid";

SPECIAL_ACTOR_MAIL_HANDLE(ServiceDistcoveryActor, 1, MsgRedisCmd)
{
        RedisCmd(msg->cmd(), false);
        // TODO: 失败返回啥
        return nullptr;
}

SPECIAL_ACTOR_MAIL_HANDLE(ServiceDistcoveryActor, 2, MsgRedisCmd)
{
        RedisCmd(msg->cmd(), false);
        return nullptr;
}

SPECIAL_ACTOR_MAIL_HANDLE(ServiceDistcoveryActor, 3)
{
        return nullptr;
}

#endif

}; // end of namespace nl::af::impl

// vim: fenc=utf8:expandtab:ts=8:noma
