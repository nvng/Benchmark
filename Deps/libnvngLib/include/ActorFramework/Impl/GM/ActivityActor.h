#pragma once

#include "GMLobbySession.h"

struct stMailSyncActivity : public stActorMailBase
{
        std::weak_ptr<GMLobbySession> _ses;
};

struct stActivityInfo
{
        uint64_t _id = 0;
        int64_t _state = 0;
        int64_t _timeType = 0;
        time_t _endTime = 0;

        std::string _data;
};
typedef std::shared_ptr<stActivityInfo> stActivityInfoPtr;

struct by_id;
struct by_end_time_type;

typedef boost::multi_index::multi_index_container<
	stActivityInfoPtr,
	boost::multi_index::indexed_by <

        boost::multi_index::ordered_unique<
        boost::multi_index::tag<by_id>,
        BOOST_MULTI_INDEX_MEMBER(stActivityInfo, uint64_t, _id)
        >,

	boost::multi_index::ordered_non_unique
	<
	boost::multi_index::tag<by_end_time_type>,
	boost::multi_index::composite_key
	<
	stActivityInfo,
	BOOST_MULTI_INDEX_MEMBER(stActivityInfo, int64_t, _timeType),
	BOOST_MULTI_INDEX_MEMBER(stActivityInfo, time_t, _endTime)
	>
	>

	>
> ActivityListType;

SPECIAL_ACTOR_DEFINE_BEGIN(ActivityActor, 0xeff);
        void PackActivity(MsgActivityFestivalCfg& msg);
        void Broadcast2Lobby();

        std::string Query(const std::map<std::string, std::string>& paramList);
        std::string Save(const std::string& body);
        std::string UpdateState(const std::string& body, int64_t state);
        void LoadFromDB();

        ActivityListType _activityList;
        int64_t _maxGroupID = 0;
        int64_t _version = 0;
SPECIAL_ACTOR_DEFINE_END(ActivityActor);

// vim: fenc=utf8:expandtab:ts=8
