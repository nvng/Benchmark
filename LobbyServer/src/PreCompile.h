#ifndef __PRE_COMPILE_H__
#define __PRE_COMPILE_H__

// #define ____PRINT_ACTOR_MAIL_COST_TIME____
#define ____BENCHMARK____
// #define ENABLE_MULTI_SEND_MUTEX

#define USE_FESTIVAL_IMPL

#define PING_PONG_SERVICE_CLIENT
#define PING_PONG_BIG_SERVICE_CLIENT
#define REDIS_SERVICE_CLIENT
#define LOG_SERVICE_CLIENT
#define GEN_GUID_SERVICE_CLIENT
#define MYSQL_SERVICE_CLIENT
#define MYSQL_BENCHMARK_SERVICE_CLIENT

// 有些 pb 会在库中使用。
#include "msg_internal_type.pb.h"
#include "msg_internal.pb.h"
#include "msg_client_type.pb.h"
#include "msg_client.pb.h"
#include "msg_db.pb.h"
#include "msg_log.pb.h"

#include "nvngLib.h"

using namespace nl::db;
using namespace nl::util;
using namespace nl::net;
using namespace nl::net::server;

#include "GlobalDef.h"
#include "GlobalVarActor.h"
#include "LobbyGateSession.h"

#include "Share/GlobalDef.h"
#include "Share/GlobalSetup.h"

#include "GenGuidService.h"
#include "MySqlService.h"
#include "LogService.h"

using namespace nl::af::impl::_1;

#include "App.h"

#endif // __PRE_COMPILE_H__

// vim: fenc=utf8:expandtab:ts=8
