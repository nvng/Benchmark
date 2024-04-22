#ifndef __PRE_COMPILE_H__
#define __PRE_COMPILE_H__

// #define ____PRINT_ACTOR_MAIL_COST_TIME____
#define ____BENCHMARK____
#define USE_BENCHMARK_APP

// 有些 pb 会在库中使用。
#include "msg_client_type.pb.h"
#include "msg_client.pb.h"
#include "msg_internal_type.pb.h"
#include "msg_internal.pb.h"
#include "msg_jump.pb.h"

#include "nvngLib.h"
#include "GlobalDef.h"

#include "Share/GlobalDef.h"

#include "App.h"

using namespace nl::util;
using namespace nl::net;
using namespace nl::net::client;

using namespace nl::af::impl::_1;

#endif // __PRE_COMPILE_H__

// vim: fenc=utf8:expandtab:ts=8:
