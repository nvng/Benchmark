#ifndef __NVNLIB_PRE_COMPILE_H__
#define __NVNLIB_PRE_COMPILE_H__

// #define ____PRINT_ACTOR_MAIL_COST_TIME____
// #define ____BENCHMARK____

#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <algorithm>

#include <iomanip>

#define BOOST_SP_DISABLE_THREADS
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/make_shared.hpp>

#include <boost/program_options.hpp>
#include <boost/uuid/detail/md5.hpp>
#include <boost/algorithm/hex.hpp>
#include <boost/serialization/singleton.hpp>

#include <boost/fiber/all.hpp>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/mysql.hpp>
#include <boost/crc.hpp>

// multi_index_container
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/composite_key.hpp>

#include <rapidjson/document.h>

#include "Tools/Util.h"
#include "ActorFramework/yield.hpp"
#include "Tools/ServerList.hpp"

#endif // __NVNGLIB_PRE_COMPILE_H__

// vim: fenc=utf8:expandtab:ts=8
