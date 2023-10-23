#include "CDKeyService.h"

#include "GMService.h"

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

                m->_pb = msg;
                act->CallPushInternal(m->_agent, E_MCMT_CDKey, E_MCCKST_ReqUse, m, msgHead._guid);
        }
}

/*
{
        "cdkey": [],
        "end_time": 123,
        "platform": [],
        "name": "abc",
        "group_id": 1,
        "use_count": 1,
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
        return ret;
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
        ret->set_max_use_count_limit(data["use_count"].GetInt64());

        const auto& platformArr = data["platform"];
        for (int64_t i=0; i<platformArr.Size(); ++i)
                ret->mutable_platform_list()->emplace(data["platform"].GetInt64(), false);

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
        }

        return ret;
}

bool CDKeyActor::Init()
{
        if (!SuperType::Init())
                return false;

        return true;
}

bool CDKeyActor::AddInfo(const std::shared_ptr<MsgCDKeyInfo>& info)
{
        bool ret = _cdkeyList.Add(info->group_id(), info);
        if (ret)
        {
                for (auto& key : info->cdkey_list())
                        _cdkeyListByKey.Add(key, info->group_id());
        }
        return ret;
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
                        break;

                auto groupID = _cdkeyListByKey.Get(pb->cdkey());
                if (0 == groupID)
                        break;

                auto info = _cdkeyList.Get(groupID);
                if (!info)
                        break;

                auto it = info->platform_list().find(pb->platform());
                if (info->platform_list().end() == it)
                        break;

                if (info->cur_use_count() >= info->max_use_count_limit())
                        break;

                info->set_cur_use_count(info->cur_use_count() + 1);
                pb->mutable_reward_list()->CopyFrom(*info->mutable_reward_list());
        } while (0);

        return pb;
}

SPECIAL_ACTOR_MAIL_HANDLE(CDKeyActor, 0xd)
{
        Flush2DB();
        SuperType::Terminate();
        return nullptr;
}

SPECIAL_ACTOR_MAIL_HANDLE(CDKeyActor, 0xe, MailInt)
{
        auto groupInfo = _cdkeyList.Remove(msg->val());
        for (auto& key : groupInfo->cdkey_list())
                _cdkeyListByKey.Remove(key);

        Save2DB();
        return nullptr;
}

SPECIAL_ACTOR_MAIL_HANDLE(CDKeyActor, 0xf, MsgCDKeyInfo)
{
        AddInfo(msg);
        Save2DB();
        return nullptr;
}

#endif

template <>
bool CDKeyService::Init()
{
        if (!SuperType::Init())
                return false;

#ifdef CDKEY_SERVICE_SERVER

        GMService::GetInstance()->RegistOpt(E_MCMT_CDKey, 0, [](const rapidjson::Value& root) {
                auto act = CDKeyService::GetInstance()->GetActor_();
                if (!act)
                        return "{}";

                if (root.HasMember("data") && root["data"].IsObject())
                {
                        auto cdkeyInfo = CDKeyActor::UnPackCDKeyInfoFromJson(root["data"]);
                        act->SendPush(0xf, cdkeyInfo);
                }
                return "{}";
        });

        GMService::GetInstance()->RegistOpt(E_MCMT_CDKey, 1, [](const rapidjson::Value& root) {
                auto act = CDKeyService::GetInstance()->GetActor_();
                if (!act)
                        return "{}";

                if (root.HasMember("group_id") && root["group_id"].IsInt64())
                {
                        auto m = std::make_shared<MailInt>();
                        m->set_val(root["group_id"].GetInt64());
                        act->SendPush(0xe, m);
                }
                return "{}";
        });

        GMService::GetInstance()->RegistOpt(E_MCMT_CDKey, 2, [](const rapidjson::Value& root) {
                // 根据 group_id 获取 使用数。
                return "{}";
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
                LogService::GetInstance()->Log<E_LSLMT_Content>(GetID(), Base64Encode(str), E_LSLST_CDKey, 0, GetClock().GetTimeStamp(), E_LSOT_CDKey, logGuid);

                auto playerChange = ret->mutable_player_change();
                for (auto& info : ret->reward_list())
                        AddDrop(*playerChange, info.reward_type(), info.reward_id(), info.num(), E_LSOT_CDKey, logGuid);
        } while (0);

        return ret;
}

#endif
