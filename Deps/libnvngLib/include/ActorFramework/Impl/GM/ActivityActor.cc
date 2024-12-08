#include "ActivityActor.h"

#include "App.h"

std::string ActivityActor::Query(const std::map<std::string, std::string>& paramList)
{
        std::string retStr;
        int64_t page = 0;
        int64_t cntPerPage = 1000;
        auto it = paramList.find("page");
        if (paramList.end() != it &&  !it->second.empty())
        {
                page = atoll(it->second.c_str());
                cntPerPage = 10;
        }

        std::string sqlStr = fmt::format("SELECT data FROM activity LIMIT {} OFFSET {};",
                                         cntPerPage, page * cntPerPage);

        std::string jsonStr;
        auto ret = MySqlMgr::GetInstance()->Exec(sqlStr);
        for (auto row : ret->rows())
        {
                jsonStr += Base64Decode(row.at(0).as_string());
                jsonStr += ",";
        }
        if (!jsonStr.empty())
                jsonStr.pop_back();
        LOG_INFO("333333333333333 jsonStr:{}", jsonStr);
        return "[" + jsonStr + "]";
}

std::string ActivityActor::Save(const std::string& body)
{
        std::string retStr = "{ \"result\": \"error\" }";
        rapidjson::Document root;
        if (root.Parse(body).HasParseError())
                return retStr;

        if (!root.HasMember("group_id")
            || !root.HasMember("state"))
                return retStr;

        const auto groupID = root["group_id"].GetUint64();
        const auto state = root["state"].GetUint64();
        std::string sqlStr = fmt::format("INSERT INTO activity(id, state, data) VALUES({}, {}, '{}');",
                                         groupID, state, Base64Encode(body));

        MySqlMgr::GetInstance()->Exec(sqlStr, false);
        MySqlMgr::GetInstance()->Exec(fmt::format("UPDATE global_var SET v={} WHERE name='festival_version';", ++_version));
        LoadFromDB();
        Broadcast2Lobby();
        return "{ \"result\": \"ok\" }";
}

std::string ActivityActor::UpdateState(const std::string& body, int64_t state)
{
        std::string retStr = "{ \"result\": \"error\" }";
        rapidjson::Document root;
        if (root.Parse(body).HasParseError())
                return retStr;

        if (!root.HasMember("group_id"))
                return retStr;

        // { "group_id":1, "cfg_id":2 }
        int64_t groupState = state;
        const auto groupID = root["group_id"].GetUint64();
        auto fesID = 0;
        if (root.HasMember("cfg_id"))
                fesID = root["cfg_id"].GetUint64();
        std::string sqlStr = fmt::format("SELECT data from activity WHERE id={};", groupID);
        std::string dataStr;
        auto ret = MySqlMgr::GetInstance()->Exec(sqlStr);
        for (auto row : ret->rows())
        {
                rapidjson::Document root;
                if (!root.Parse(Base64Decode(row.at(0).as_string())).HasParseError())
                {
                        if (0 != fesID)
                        {
                                auto& arr = root["activity_list"];
                                if (!arr.IsNull())
                                {
                                        for (int64_t i=0; i<arr.Size(); ++i)
                                        {
                                                auto& item = arr[i];
                                                if (item["cfg_id"].GetInt64() == fesID)
                                                {
                                                        item["state"] = state;
                                                        break;
                                                }
                                        }
                                }
                                groupState = root["state"].GetUint64();
                        }
                        else
                        {
                                root["state"] = state;
                        }

                        rapidjson::StringBuffer strBuf;
                        rapidjson::Writer<rapidjson::StringBuffer> writer(strBuf);
                        root.Accept(writer);
                        dataStr = Base64Encode(strBuf.GetString());
                }
        }

        if (!dataStr.empty())
        {
                std::string sqlStr = fmt::format("UPDATE activity SET state={}, data='{}' WHERE id={};", groupState, dataStr, groupID);
                MySqlMgr::GetInstance()->Exec(sqlStr);
                MySqlMgr::GetInstance()->Exec(fmt::format("UPDATE global_var SET v={} WHERE name='festival_version';", ++_version));
                LoadFromDB();
                Broadcast2Lobby();
                return "{ \"result\": \"ok\" }";
        }
        return retStr;
}

void ActivityActor::LoadFromDB()
{
        std::string sqlStr = fmt::format("SELECT * FROM activity WHERE state=1 OR state=3;");
        _activityList.clear();
        auto ret = MySqlMgr::GetInstance()->Exec(sqlStr);
        for (auto row : ret->rows())
        {
                auto info = std::make_shared<stActivityInfo>();
                info->_data = Base64Decode(row.at(2).as_string());

                rapidjson::Document root;
                if (root.Parse(info->_data).HasParseError())
                        continue;

                if (!root.HasMember("group_id")
                    || !root.HasMember("state")
                    || !root.HasMember("time")
                    || !root["time"].HasMember("type")
                    || !root["time"].HasMember("end_time")
                   )
                        continue;

                info->_id = root["group_id"].GetUint64();
                info->_state = root["state"].GetUint64();
                info->_timeType = root["time"]["type"].GetUint64();
                info->_endTime = root["time"]["end_time"].GetUint64();
                _activityList.emplace(info);
        }

}

SPECIAL_ACTOR_MAIL_HANDLE(ActivityActor, 0xf, stMailHttpReq)
{
        auto ret = MySqlMgr::GetInstance()->Exec("SELECT MAX(id) FROM activity;");
        if (ret->rows()[0][0].is_uint64())
                _maxGroupID = ret->rows()[0][0].as_uint64();

        std::string sqlStr = "SELECT v FROM global_var WHERE name='festival_version'";
        ret = MySqlMgr::GetInstance()->Exec(sqlStr);
        if (ret->rows().empty())
        {
                        _version = 1;
                        MySqlMgr::GetInstance()->Exec("INSERT INTO global_var(name, v) VALUES('festival_version', 1);");
        }
        else
        {
                        _version = ret->rows()[0][0].as_int64();
        }

        LoadFromDB();

        auto gmServerInfo = GetApp()->GetServerInfo<stGMServerInfo>();
        NetMgr::GetInstance()->HttpListen(gmServerInfo->_http_activity_port, [](stHttpReqPtr&& req) {
                auto m = std::make_shared<stMailHttpReq>();
                m->_httpReq = req;
                GetApp()->_activityActor->SendPush(0, m);
        });

        return nullptr;
}

void ActivityActor::PackActivity(MsgActivityFestivalCfg& msg)
{
        auto parseTimeFromJson = [](auto& msg, const auto& item) {
                msg.set_type(item["type"].GetUint64());
                msg.set_active_time(item["active_time"].GetUint64());
                msg.set_start_time(item["start_time"].GetUint64());
                msg.set_opt_end_time(item["opt_end_time"].GetUint64());
                msg.set_end_time(item["end_time"].GetUint64());
                msg.set_publish_time(item["publish_time"].GetUint64());
        };

        auto parseGoodsListFromJson = [](auto& msg, const auto& arr) {
                for (int64_t i=0; i<arr.Size(); ++i)
                {
                        auto& jt = arr[i];
                        auto ft = msg.Add();
                        ft->set_guid(jt["guid"].GetUint64());
                        auto t = ft->mutable_goods_item();
                        t->set_id(jt["id"].GetUint64());
                        t->set_type(jt["type"].GetUint64());
                        t->set_num(jt["cnt"].GetUint64());
                }
        };

        auto parseActivityFromJson = [&parseTimeFromJson, &parseGoodsListFromJson](auto& act, const auto& item) {
                act.set_cfg_id(item["cfg_id"].GetUint64());
                act.set_state(item["state"].GetUint64());
                parseTimeFromJson(*act.mutable_time(), item["time"]);
                act.set_param(item["param"].GetUint64());
                act.set_param_1(item["param_1"].GetInt64());
                act.set_param_2(item["param_2"].GetInt64());

                auto& rewardArr = item["reward_list"];
                for (int64_t i=0; i<rewardArr.Size(); ++i)
                {
                        auto& tmp = rewardArr[i];
                        auto msgReward = act.add_reward_list();
                        msgReward->set_task_id(tmp["task_id"].GetUint64());
                        if (tmp.HasMember("target"))
                                msgReward->set_target(tmp["target"].GetUint64());
                        if (tmp.HasMember("target_arg1"))
                                msgReward->set_target_arg1(tmp["target_arg1"].GetUint64());
                        if (tmp.HasMember("target_arg2"))
                                msgReward->set_target_arg2(tmp["target_arg2"].GetUint64());
                        msgReward->set_name(tmp["name"].GetString());
                        if (tmp.HasMember("event_type"))
                                msgReward->set_event_type(tmp["event_type"].GetUint64());
                        auto& paramArr = tmp["param_list"];
                        for (int64_t j=0; j<paramArr.Size(); ++j)
                                msgReward->add_param_list(paramArr[i].GetInt64());
                        parseGoodsListFromJson(*msgReward->mutable_goods_list(), tmp["goods_list"]);
                }

                if (item.HasMember("cum_rewards") && !item["cum_rewards"].IsNull())
                {
                        auto& cumRewardArr = item["cum_rewards"];
                        for (int64_t i=0; i<cumRewardArr.Size(); ++i)
                        {
                                auto& tmp = cumRewardArr[i];
                                auto msgReward = act.add_cum_rewards();
                                msgReward->set_icon(tmp["icon"].GetString());
                                parseGoodsListFromJson(*msgReward->mutable_goods_list(), tmp["goods_list"]);
                        }
                }
        };

        auto parseFromJsonFunc = [&msg, &parseTimeFromJson, &parseActivityFromJson](const auto& it) {
                LOG_INFO("{}", (*it)->_data);
                rapidjson::Document root;
                if (root.Parse((*it)->_data).HasParseError())
                        return;

                if (!root.HasMember("group_id")
                    || !root.HasMember("type")
                    || !root.HasMember("state")
                    || !root.HasMember("icon")
                    || !root.HasMember("name")
                    || !root.HasMember("desc")
                    || !root.HasMember("time")
                   )
                        return;

                // auto msgGroup = msg.add_group_list();
                MsgActivityFestivalGroupCfg msgGroup;
                msgGroup.set_group_guid(root["group_id"].GetUint64());
                msgGroup.set_type(root["type"].GetUint64());
                msgGroup.set_state(root["state"].GetUint64());
                msgGroup.set_icon(root["icon"].GetString());
                msgGroup.set_name(root["name"].GetString());
                msgGroup.set_desc(root["desc"].GetString());

                parseTimeFromJson(*msgGroup.mutable_time(), root["time"]);
                if (root.HasMember("activity") && !root["activity"].IsNull())
                        parseActivityFromJson(*msgGroup.mutable_activity(), root["activity"]);
                else if (root.HasMember("activity_list") && !root["activity_list"].IsNull())
                {
                        auto& arr = root["activity_list"];
                        for (int64_t i=0; i<arr.Size(); ++i)
                        {
                                MsgActivityFestivalActivityCfg item;
                                parseActivityFromJson(item, arr[i]);
                                (*msgGroup.mutable_activity_list())[item.cfg_id()] = item;
                        }
                }
                (*msg.mutable_group_list())[msgGroup.group_guid()] = msgGroup;
        };

        msg.set_version(_version);
        auto now = GetClock().GetTimeStamp();
        auto& seq = _activityList.get<by_end_time_type>();
        for (auto it = seq.upper_bound(boost::make_tuple(2, now)); it!=seq.end(); ++it)
                parseFromJsonFunc(it);

        auto range = seq.equal_range(1);
        for (auto it = range.first; it!=range.second; ++it)
                parseFromJsonFunc(it);
}

void ActivityActor::Broadcast2Lobby()
{
        auto sendMsg = std::make_shared<MsgActivityFestivalCfg>();
        PackActivity(*sendMsg);
        GetApp()->_lobbySesList.Foreach([&sendMsg](const auto& ws) {
                auto ses = ws.lock();
                if (ses)
                        ses->SendPB(sendMsg, E_MIMT_Internal, E_MIIST_SyncRelation, GMLobbySession::MsgHeaderType::E_RMT_Send, 0, 0, 0);
        });
}

SPECIAL_ACTOR_MAIL_HANDLE(ActivityActor, 0xe, stMailSyncActivity)
{
        auto ses = msg->_ses.lock();
        if (!ses)
                return nullptr;

        auto sendMsg = std::make_shared<MsgActivityFestivalCfg>();
        PackActivity(*sendMsg);
        ses->SendPB(sendMsg, E_MIMT_Internal, E_MIIST_SyncRelation, GMLobbySession::MsgHeaderType::E_RMT_Send, 0, 0, 0);
        return nullptr;
}

SPECIAL_ACTOR_MAIL_HANDLE(ActivityActor, 0, stMailHttpReq)
{
        auto targetArr = Tokenizer(msg->_httpReq->_req.target(), "?");
        if (targetArr.size() <= 1)
                return nullptr;

        std::map<std::string, std::string> paramList;
        for (auto& val : Tokenizer(targetArr[1], "&"))
        {
                auto paramArr = Tokenizer(val, "=");
                if (paramArr.size() >= 2)
                        paramList.emplace(paramArr[0], paramArr[1]);
        }

        auto it = paramList.find("opt");
        if (paramList.end() == it)
        {
                LOG_WARN("缺少参数 opt !!!");
                return nullptr;
        }

        std::string retStr;
        switch (atoll(it->second.c_str()))
        {
        case 0 : // Query
                retStr = Query(paramList);
                break;
        case 1 : // Save
                retStr = Save(msg->_httpReq->_req.body());
                break;
        case 2 : // Stop
                retStr = UpdateState(msg->_httpReq->_req.body(), 2);
                break;
        case 3 : // Offline
                retStr = UpdateState(msg->_httpReq->_req.body(), 3);
                break;
        case 4 : // Online
                retStr = UpdateState(msg->_httpReq->_req.body(), 1);
                break;
        case 100 : // req group id
                retStr = "{ \"group_id\": " + fmt::format("{}", ++_maxGroupID) + " }";
                break;
        case 101 : // req time
                retStr = "{ \"time\": " + fmt::format("{}", GetClock().GetTimeStamp()) + " }";
                break;
        default :
                break;
        }

        msg->_httpReq->Reply(retStr);
        return nullptr;
}

// vim: fenc=utf8:expandtab:ts=8
