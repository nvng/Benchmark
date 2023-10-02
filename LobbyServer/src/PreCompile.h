#ifndef __PRE_COMPILE_H__
#define __PRE_COMPILE_H__

// #define ____PRINT_ACTOR_MAIL_COST_TIME____
#define ____BENCHMARK____

#define PING_PONG_SERVICE_CLIENT
#define PING_PONG_BIG_SERVICE_CLIENT
#define REDIS_SERVICE_CLIENT

#define BOOST_SP_DISABLE_THREADS
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/make_shared.hpp>

#include <unordered_map>
#include <algorithm>
#include <cmath>

#include <boost/program_options.hpp>
#include <boost/fiber/all.hpp>

// multi_index_container
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/composite_key.hpp>

#include <rapidjson/document.h>

// 有些 pb 会在库中使用。
#include "msg_internal_type.pb.h"
#include "msg_internal.pb.h"
#include "msg_client_type.pb.h"
#include "msg_client.pb.h"
#include "msg_db.pb.h"

#include "nvngLib.h"

using namespace nl::db;
using namespace nl::util;
using namespace nl::net;
using namespace nl::net::server;

#include "GlobalDef.h"
#include "GlobalVarActor.h"
#include "LobbyGateSession.h"
#include "LobbyDBSession.h"

#include "Share/GlobalDef.h"
#include "Share/GlobalSetup.h"

using namespace nl::af::impl;

#include "App.h"

#endif // __PRE_COMPILE_H__

// vim: fenc=utf8:expandtab:ts=8
