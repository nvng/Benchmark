#include "Redis.h"

#if defined (REDIS_SERVICE_LOCAL) || defined (REDIS_SERVICE_CLIENT) || defined (REDIS_SERVICE_SERVER)

#ifdef REDIS_SERVICE_CLIENT

/*
 * 内网环境下，redis 单线程，连接数16，QPS 400万左右。
 */

ACTOR_MAIL_HANDLE(RedisActor, 0xfff, 0x0)
{
        while (true)
        {
                int64_t i = 0;
                for (; i<10; ++i)
                {
                        auto ret = RedisCmd("GET", "ybt.user:100071");
                        auto [data, err] = ret->GetStr();
                        RedisCmd("SET", "ybt.user:100071", data);
                }
                GetApp()->_cnt += i;
        }

        return nullptr;
}

#endif

#endif
