#ifndef __PRE_COMPILE_H__
#define __PRE_COMPILE_H__

// #define ____PRINT_ACTOR_MAIL_COST_TIME____
#define ____BENCHMARK____

#define BOOST_SP_DISABLE_THREADS
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/make_shared.hpp>

#include <unordered_map>
#include <algorithm>
#include <cmath>

#include <boost/program_options.hpp>
#include <boost/fiber/all.hpp>

#include <rapidjson/document.h>

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

using namespace nl::af::impl;
using namespace nl::util;
using namespace nl::net;
using namespace nl::net::client;

#endif // __PRE_COMPILE_H__

// vim: fenc=utf8:expandtab:ts=8:
