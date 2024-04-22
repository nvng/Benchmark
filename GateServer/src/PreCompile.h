#ifndef __PRE_COMPILE_H__
#define __PRE_COMPILE_H__

// #define ROBOT_TEST
#define ____BENCHMARK____
// #define ____USE_IMPL_GATE_LOBBY_SESSION_ONRECV___

// #define ____CLIENT_USE_WS____
// #define ____CLIENT_USE_WSS____

// 有些 pb 会在库中使用。
#include "msg_login.pb.h"
#include "msg_client_type.pb.h"
#include "msg_client.pb.h"
#include "msg_internal_type.pb.h"
#include "msg_internal.pb.h"

#include "nvngLib.h"

#include "GlobalDef.h"
#include "GateClientSession.h"
#include "GateLobbySession.h"
#include "GateGameSession.h"
#include "NetMgrImpl.h"

#include "Share/GlobalDef.h"
#include "Share/GlobalSetup.h"

#include "App.h"

using namespace nl::util;
using namespace nl::net;
using namespace nl::net::server;
using namespace nl::af::impl::_1;

#endif // __PRE_COMPILE_H__

// vim: fenc=utf8:expandtab:ts=8:
