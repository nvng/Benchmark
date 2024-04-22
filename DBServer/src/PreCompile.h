#ifndef __PRE_COMPILE_H__
#define __PRE_COMPILE_H__

// #define ____PRINT_ACTOR_MAIL_COST_TIME____

#define ____BENCHMARK____

#define GEN_GUID_SERVICE_SERVER
#define MYSQL_SERVICE_SERVER

#include "msg_internal_type.pb.h"
#include "msg_internal.pb.h"
#include "msg_client.pb.h"

#include "nvngLib.h"
#include "GlobalDef.h"

#include "Share/GlobalDef.h"

using namespace nl::db;
using namespace nl::net;
using namespace nl::net::server;
using namespace nl::util;

using namespace nl::af::impl::_1;

#include "App.h"
#include "GenGuidService.h"
#include "MySqlService.h"

#endif // __PRE_COMPILE_H__

// vim: fenc=utf8:expandtab:ts=8:
