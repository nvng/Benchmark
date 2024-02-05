#include "CDKeyService.h"

#if defined(CDKEY_SERVICE_SERVER) || defined(CDKEY_SERVICE_CLIENT) || defined(CDKEY_SERVICE_LOCAL)

#include "GMService.h"
#include <rapidjson/document.h>

#ifdef CDKEY_SERVICE_SERVER

SERVICE_NET_HANDLE(CDKeyService::SessionType, E_MCMT_CDKey, E_MCCKST_ReqUse, MsgCDKeyReqUse)
{
        auto act = CDKeyService::GetInstance()->GetActor_();
        if (act)
        {
                auto m = std::make_shared<CDKeyService::SessionType::stServiceMessageWapper>();
                m->_agent = std::make_shared<typename CDKeyService::SessionType::ActorAgentType>(msgHead._from, shared_from_this());
                m->_agent->BindActor(act);
                AddAgent(m->_agent);

                m->_msgHead = msgHead;
                m->_pb = msg;
                act->SendPush(E_MCCKST_ReqUse, m);
        }
}

/*
{
        "cdkey": [],
        "end_time": 123,
        "platform": [],
        "name": "abc",
        "group_id": 1,
        "max_count": 1,
        "reward": [{
                "reward_id": 1,
                "reward_type": 2,
                "num": 3,
                "icon_name": "abc",
                "name": "def",
                "desc": "desc"
        }]
}
*/

std::string CDKeyActor::PackCDKeyInfo2Json(const MsgCDKeyInfo& msg)
{
        std::string ret;

        std::string keyListStr;
        for (auto& k : msg.cdkey_list())
                keyListStr += fmt::format("\"{}\",", k);
        if (!keyListStr.empty())
                keyListStr.pop_back();

        std::string platformListStr;
        for (auto& val : msg.platform_list())
                platformListStr += fmt::format("{},", val.first);
        if (!platformListStr.empty())
                platformListStr.pop_back();

        std::string rewardStr;
        for (auto& info : msg.reward_list())
        {
                rewardStr += fmt::format("{} \"reward_id\":{}, \"reward_type\":{}, \"num\":{}, \"icon_name\":\"{}\", \"name\":\"{}\", \"desc\":\"{}\"{},"
                                         , "{", info.reward_id(), info.reward_type(), info.num(), info.icon_name(), info.name(), info.desc(), "}");
        }
        if (!rewardStr.empty())
                rewardStr.pop_back();

        return fmt::format("{}\"cdkey\":[{}], \"end_time\":{}, \"platform\":[{}], \"name\":\"{}\", \"group_id\":{}, \"max_count\":{}, \"use_count\":{}, \"reward\":[{}]{}"
                           , "{", keyListStr, msg.end_time(), platformListStr, msg.name(), msg.group_id(), msg.max_use_count_limit(), msg.cur_use_count(), rewardStr, "}");
}

std::shared_ptr<MsgCDKeyInfo> CDKeyActor::UnPackCDKeyInfoFromJson(const rapidjson::Value& data)
{
        if (!data.HasMember("cdkey")
            || !data["cdkey"].IsArray()
            || !data.HasMember("end_time")
            || !data.HasMember("platform")
            || !data["platform"].IsArray()
            || !data.HasMember("name")
            || !data.HasMember("group_id")
            || !data.HasMember("use_count")
            || !data.HasMember("reward")
            || !data["reward"].IsArray()
           )
                return nullptr;

        auto ret = std::make_shared<MsgCDKeyInfo>();
        ret->set_end_time(data["end_time"].GetInt64());
        ret->set_group_id(data["group_id"].GetInt64());
        ret->set_max_use_count_limit(data["max_count"].GetInt64());
        ret->set_name(data["name"].GetString());

        const auto& platformArr = data["platform"];
        for (int64_t i=0; i<platformArr.Size(); ++i)
                ret->mutable_platform_list()->emplace(platformArr[i].GetInt64(), false);

        const auto& cdkeyArr = data["cdkey"];
        for (int64_t i=0; i<cdkeyArr.Size(); ++i)
        {
                const auto& item = cdkeyArr[i];
                ret->add_cdkey_list(item.GetString());
        }

        const auto& rewardArr = data["reward"];
        for (int64_t i=0; i<rewardArr.Size(); ++i)
        {
                const auto& item = rewardArr[i];
                auto info = ret->add_reward_list();
                info->set_reward_id(item["reward_id"].GetInt64());
                info->set_reward_type(item["reward_type"].GetInt64());
                info->set_num(item["num"].GetInt64());
                info->set_icon_name(item["icon_name"].GetString());
                info->set_name(item["name"].GetString());
                info->set_desc(item["desc"].GetString());
        }

        return ret;
}

bool CDKeyActor::Init()
{
        if (!SuperType::Init())
                return false;

        LoadFromDB();
        return true;
}

std::pair<bool, std::string> CDKeyActor::AddInfo(const std::shared_ptr<MsgCDKeyInfo>& info)
{
        std::string errorKey;
        bool ret = _cdkeyList.Add(info->group_id(), info);
        if (ret)
        {
                bool allNotFound = true;
                for (auto& key : info->cdkey_list())
                {
                        if (0 != _cdkeyListByKey.Get(key))
                        {
                                errorKey = key;
                                allNotFound = false;
                                break;
                        }
                }

                if (allNotFound)
                {
                        for (auto& key : info->cdkey_list())
                                _cdkeyListByKey.Add(key, info->group_id());
                }
        }
        return { errorKey.empty(), errorKey};
}

void CDKeyActor::LoadFromDB()
{
        _cdkeyList.Clear();
        _cdkeyListByKey.Clear();

        constexpr uint64_t id = MySqlMgr::GenDataKey(E_MCMT_CDKey);
        std::string sqlStr = fmt::format("SELECT data FROM data_0 WHERE id={};", id);
        auto result = MySqlMgr::GetInstance()->Exec(sqlStr);
        if (result->rows().empty())
        {
                auto sql = fmt::format("INSERT INTO data_0(id, v, data) VALUES({}, 0, \"\")", id);
                MySqlMgr::GetInstance()->Exec(sql);
        }
        else
        {
                for (auto row : result->rows())
                {
                        auto data = row.at(0).as_string();
                        MsgCDKeyInfoList dbInfo;
                        Compress::UnCompressAndParseAlloc<Compress::E_CT_Zstd>(dbInfo, Base64Decode(data));

                        _groupGuid = dbInfo.group_guid();
                        for (auto& info : dbInfo.info_list())
                        {
                                auto t = std::make_shared<MsgCDKeyInfo>();
                                t->CopyFrom(info);
                                AddInfo(t);
                        }
                }
        }
}

void CDKeyActor::Save2DB()
{
        if (_inTimer)
                return;

        _inTimer = true;
        CDKeyActorWeakPtr weakThis = shared_from_this();
        _saveTimer.Start(weakThis, 30, [weakThis]() {
                auto thisPtr = weakThis.lock();
                if (!thisPtr)
                        return;

                thisPtr->_inTimer = false;
                thisPtr->Flush2DB();
        });
}

void CDKeyActor::Flush2DB()
{
        MsgCDKeyInfoList dbInfo;
        dbInfo.set_group_guid(_groupGuid);
        _cdkeyList.Foreach([&dbInfo](const auto& info) {
                dbInfo.add_info_list()->CopyFrom(*info);
        });

        auto [bufRef, bufSize] = Compress::SerializeAndCompress<Compress::E_CT_Zstd>(dbInfo);
        std::string sql = fmt::format("UPDATE data_0 SET data=\"{}\" WHERE id={};", Base64Encode(bufRef.get(), bufSize), MySqlMgr::GenDataKey(E_MCMT_CDKey));
        MySqlMgr::GetInstance()->Exec(sql);
}

void CDKeyActor::Terminate()
{
        SendPush(0xd, nullptr);
}

SPECIAL_ACTOR_MAIL_HANDLE(CDKeyActor, E_MCCKST_ReqUse, CDKeyService::SessionType::stServiceMessageWapper)
{
        auto pb = std::dynamic_pointer_cast<MsgCDKeyReqUse>(msg->_pb);
        do
        {
                if (!pb)
                {
                        pb->set_error_type(E_CET_ParamError);
                        break;
                }

                auto groupID = _cdkeyListByKey.Get(pb->cdkey());
                if (0 == groupID)
                {
                        pb->set_error_type(E_CET_CDKeyInvalid);
                        break;
                }

                auto it = pb->already_use_list().find(groupID);
                if (pb->already_use_list().end() != it)
                {
                        pb->set_error_type(E_CET_CDKeyAlreadyUse);
                        break;
                }

                auto info = _cdkeyList.Get(groupID);
                if (!info)
                {
                        pb->set_error_type(E_CET_CDKeyInvalid);
                        break;
                }

                auto now = GetClock().GetTimeStamp();
                if (info->end_time() < now)
                {
                        pb->set_error_type(E_CET_CDKeyTimeout);
                        break;
                }

                {
                        auto it = info->platform_list().find(pb->platform());
                        if (info->platform_list().end() == it)
                        {
                                pb->set_error_type(E_CET_CDKeyPlatformError);
                                break;
                        }
                }

                if (-1 != info->max_use_count_limit())
                {
                        if (info->cur_use_count() >= info->max_use_count_limit())
                        {
                                pb->set_error_type(E_CET_CDKeyMaxCountLimit);
                                break;
                        }
                        info->set_cur_use_count(info->cur_use_count() + 1);
                }

                pb->mutable_reward_list()->CopyFrom(*info->mutable_reward_list());
                pb->set_group(groupID);
                pb->set_error_type(E_CET_Success);
        } while (0);

        auto ses = msg->_agent->GetSession();
        if (ses)
                ses->SendPB(pb, E_MCMT_CDKey, E_MCCKST_ReqUse, CDKeyService::SessionType::MsgHeaderType::E_RMT_CallRet, msg->_msgHead._guid, msg->_msgHead._to, msg->_msgHead._from);
        return nullptr;
}

SPECIAL_ACTOR_MAIL_HANDLE(CDKeyActor, 0xc)
{
        std::string cdkeyListStr;
        _cdkeyList.Foreach([this, &cdkeyListStr](const auto& info) {
                cdkeyListStr += PackCDKeyInfo2Json(*info);
                cdkeyListStr += ",";
        });
        if (!cdkeyListStr.empty())
                cdkeyListStr.pop_back();

        auto m = std::make_shared<stGMResult>();
        m->_ret = { true , fmt::format("[{}]", cdkeyListStr) };

        return m;
}

SPECIAL_ACTOR_MAIL_HANDLE(CDKeyActor, 0xd)
{
        Flush2DB();
        SuperType::Terminate();
        return nullptr;
}

struct stMailCDKeyRemove : public stActorMailBase
{
        uint64_t _groupGuid = 0;
        std::string _key;
};

SPECIAL_ACTOR_MAIL_HANDLE(CDKeyActor, 0xe, stMailCDKeyRemove)
{
        if (msg->_key.empty())
        {
                auto groupInfo = _cdkeyList.Remove(msg->_groupGuid);
                for (auto& key : groupInfo->cdkey_list())
                        _cdkeyListByKey.Remove(key);
        }
        else
        {
                auto groupInfo = _cdkeyList.Get(msg->_groupGuid);
                auto cdkeyList = std::move(groupInfo->cdkey_list());
                groupInfo->clear_cdkey_list();
                for (auto& key : cdkeyList)
                {
                        if (msg->_key != key)
                                groupInfo->add_cdkey_list(key);
                }

                if (groupInfo->cdkey_list_size() <= 0)
                        _cdkeyList.Remove(msg->_groupGuid);
                _cdkeyListByKey.Remove(msg->_key);
        }

        Save2DB();
        return nullptr;
}

SPECIAL_ACTOR_MAIL_HANDLE(CDKeyActor, 0xf, MsgCDKeyInfo)
{
        auto retMsg = std::make_shared<stGMResult>();
        auto [ret, errorKey] = AddInfo(msg);
        if (ret)
        {
                retMsg->_ret = { true, "{ \"error_code\" : 200 }" };
                Save2DB();
        }
        else
        {
                retMsg->_ret = { false, fmt::format("{} \"error_code\" : 201, \"msg\" : \"key[{}] 重复!!!\" {}", "{", errorKey, "}") };
        }

        return retMsg;
}

#endif

template <>
bool CDKeyService::Init()
{
        if (!SuperType::Init())
                return false;

#ifdef CDKEY_SERVICE_SERVER

        GMService::GetInstance()->RegistOpt(E_MCMT_CDKey, 0, [](const GMActorPtr& gmActor, const auto& msg) -> std::pair<bool, std::string> {
                std::string ret = "{ \"error_code\" : 201 }";
                auto act = CDKeyService::GetInstance()->GetActor_();
                if (!act)
                        return { false, ret };

                rapidjson::Document root;
                if (root.Parse(msg->body().c_str()).HasParseError())
                        return { false, ret };

                auto cdkeyInfo = CDKeyActor::UnPackCDKeyInfoFromJson(root);
                if (!cdkeyInfo)
                        return { false, ret };

                auto callRet = Call(stGMResult, gmActor, act, E_MCMT_CDKey, 0xf, cdkeyInfo);
                return callRet ? callRet->_ret : std::make_pair(false, ret);
        });

        GMService::GetInstance()->RegistOpt(E_MCMT_CDKey, 1, [](const GMActorPtr& gmActor, const auto& msg) -> std::pair<bool, std::string> {
                std::string ret = "{ \"error_code\" : 201 }";
                auto act = CDKeyService::GetInstance()->GetActor_();
                if (!act)
                        return { false, ret };

                rapidjson::Document root;
                if (root.Parse(msg->body().c_str()).HasParseError())
                        return { false, ret };

                if (root.HasMember("group_guid") && root["group_guid"].IsInt64())
                {
                        auto m = std::make_shared<stMailCDKeyRemove>();
                        m->_groupGuid = root["group_guid"].GetInt64();
                        if (root.HasMember("cdkey") && root["cdkey"].IsString())
                                m->_key = root["cdkey"].GetString();
                        act->SendPush(0xe, m);
                }
                return { true, "{ \"error_code\" : 200 }" };
        });

        GMService::GetInstance()->RegistOpt(E_MCMT_CDKey, 2, [](const GMActorPtr& gmActor, const auto& msg) -> std::pair<bool, std::string> {
                std::string ret = "{ \"error_code\" : 201 }";
                auto act = CDKeyService::GetInstance()->GetActor_();
                if (!act)
                        return { false, ret };

                auto callRet = Call(stGMResult, gmActor, act, E_MCMT_CDKey, 0xc, nullptr);
                return callRet ? callRet->_ret : std::make_pair(false, ret);
        });

        GMService::GetInstance()->RegistOpt(E_MCMT_CDKey, 3, [](const GMActorPtr& gmActor, const auto& msg) -> std::pair<bool, std::string> {
                std::string ret = "{ \"error_code\" : 201 }";
                auto act = CDKeyService::GetInstance()->GetActor_();
                if (!act)
                        return { false, ret };

                return { true, fmt::format("{}\"module\":{},\"group_guid\":{}{}", "{", E_MCMT_CDKey, ++act->_groupGuid, "}") };
        });

#endif

        return true;
}

#ifdef CDKEY_SERVICE_CLIENT

#include "Player.h"

ACTOR_MAIL_HANDLE(Player, E_MCMT_CDKey, E_MCCKST_ReqUse, MsgCDKeyReqUse)
{
        std::shared_ptr<MsgCDKeyReqUse> ret;
        do
        {
                auto act = CDKeyService::GetInstance()->GetActor(shared_from_this(), GetID());
                if (!act)
                {
                        msg->set_error_type(E_CET_ParamError);
                        break;
                }

                for (auto group : _cdkeyList)
                        msg->mutable_already_use_list()->emplace(group, false);

                msg->set_player_guid(GetID());
                ret = Call(MsgCDKeyReqUse, act, E_MCMT_CDKey, E_MCCKST_ReqUse, msg);
                if (!ret || E_CET_Success != ret->error_type())
                {
                        PLAYER_LOG_WARN(GetID(), "玩家[{}] 请求使用 cdkey[{}] 时，error[{}]"
                                        , GetID(), msg->cdkey(), ret ? ret->error_type() : E_CET_CallTimeOut);
                        if (!ret)
                        {
                                ret = msg;
                                ret->set_error_type(E_CET_CallTimeOut);
                        }
                        break;
                }

                auto logGuid = LogService::GetInstance()->GenGuid();
                std::string str = fmt::format("{}\"cdkey\":{},\"platform\":{}{}", "{", msg->cdkey(), msg->platform(), "}");
                LogService::GetInstance()->Log<E_LSLMT_Content>(GetID(), str, E_LSLST_CDKey, 0, E_LSOT_CDKey, logGuid);

                _cdkeyList.emplace(ret->group());
                auto playerChange = ret->mutable_player_change();
                for (auto& info : ret->reward_list())
                        AddDrop(*playerChange, info.reward_type(), info.reward_id(), info.num(), E_LSOT_CDKey, logGuid);
                Save2DB();
        } while (0);

        return ret;
}

#endif

#endif
