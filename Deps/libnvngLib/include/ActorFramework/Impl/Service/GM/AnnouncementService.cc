#include "AnnouncementService.h"

#if defined(ANNOUNCEMENT_SERVICE_SERVER) || defined(ANNOUNCEMENT_SERVICE_CLIENT)

#include "GMService.h"
#include <rapidjson/document.h>

#ifdef ANNOUNCEMENT_SERVICE_SERVER

SERVICE_NET_HANDLE(AnnouncementService::SessionType, E_MCMT_Announcement, E_MCANNST_Sync, MsgAnnouncementList)
{
        auto act = AnnouncementService::GetInstance()->GetActor_();
        if (act)
        {
                auto cache = act->_cache;
                cache->set_error_type(E_CET_Success);
                SendPB(cache, E_MCMT_Announcement, E_MCANNST_Sync, AnnouncementService::SessionType::MsgHeaderType::E_RMT_Send, 0, 0, 0);
        }
}

/*
   {
   "guid": 123,
   "data": [
   {
   "title": "",
   "content": "",
   "isTop" : "",
   "forcePopTime":0
   }
   ]
   }
   */

bool AnnouncementActor::Init()
{
        if (!SuperType::Init())
                return false;

        _cache = std::make_shared<MsgAnnouncementList>();
        LoadFromDB();
        return true;
}

void AnnouncementActor::Sync()
{
        AnnouncementService::GetInstance()->ForeachSession([cache{_cache}](const auto& ses) {
                ses->SendPB(cache, E_MCMT_Announcement, E_MCANNST_Sync, AnnouncementService::SessionType::MsgHeaderType::E_RMT_Send, 0, 0, 0);
        });
}

void AnnouncementActor::LoadFromDB()
{
        constexpr uint64_t id = MySqlMgr::GenDataKey(E_MCMT_Announcement);
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
                        Compress::UnCompressAndParseAlloc<Compress::E_CT_Zstd>(*_cache, Base64Decode(data));
                }
        }
}

void AnnouncementActor::Flush2DB()
{
        auto [bufRef, bufSize] = Compress::SerializeAndCompress<Compress::E_CT_Zstd>(*_cache);
        std::string sql = fmt::format("UPDATE data_0 SET data=\"{}\" WHERE id={};", Base64Encode(bufRef.get(), bufSize), MySqlMgr::GenDataKey(E_MCMT_Announcement));
        MySqlMgr::GetInstance()->Exec(sql);
}


SPECIAL_ACTOR_MAIL_HANDLE(AnnouncementActor, 0xe)
{
        Flush2DB();
        return nullptr;
}

SPECIAL_ACTOR_MAIL_HANDLE(AnnouncementActor, 0xf)
{
        Flush2DB();
        SuperType::Terminate();
        return nullptr;
}

void AnnouncementActor::Terminate()
{
        SendPush(0xf, nullptr);
}

#endif

template <>
bool AnnouncementService::Init()
{
        if (!SuperType::Init())
                return false;

#ifdef ANNOUNCEMENT_SERVICE_SERVER

        GMService::GetInstance()->RegistOpt(E_MCMT_Announcement, 0, [](const GMActorPtr& gmActor, const auto& msg) -> std::pair<bool, std::string> {
                std::string ret = "{ \"error_code\" : 201 }";
                auto act = AnnouncementService::GetInstance()->GetActor_();
                if (!act)
                        return { false, ret };

                rapidjson::Document root;
                if (root.Parse(msg->body().c_str()).HasParseError())
                        return { false, ret };

                LOG_INFO("{}", msg->body());
                if (!root.HasMember("guid") || !root["guid"].IsInt64()
                    || !root.HasMember("data") || !root["data"].IsArray())
                        return { false, ret };

                auto guid = root["guid"].GetInt64();

                {
                        std::lock_guard l(act->_cacheMutex);
                        auto it = act->_cache->mutable_item_list()->find(guid);
                        if (act->_cache->mutable_item_list()->end() != it)
                                return { false, ret };

                        MsgAnnouncementList::MsgAnnouncementItem info;
                        info.set_guid(guid);
                        info.set_data(msg->body());
                        act->_cache->mutable_item_list()->emplace(guid, info);
                }
                act->Sync();
                act->SendPush(0xe, nullptr);
                return { true, "{ \"error_code\" : 200 }" };
        });

        GMService::GetInstance()->RegistOpt(E_MCMT_Announcement, 1, [](const GMActorPtr& gmActor, const auto& msg) -> std::pair<bool, std::string> {
                std::string ret = "{ \"error_code\" : 201 }";
                auto act = AnnouncementService::GetInstance()->GetActor_();
                if (!act)
                        return { false, ret };

                rapidjson::Document root;
                if (root.Parse(msg->body().c_str()).HasParseError())
                        return { false, ret };

                if (root.HasMember("guid") && root["guid"].IsInt64())
                {
                        std::lock_guard l(act->_cacheMutex);
                        act->_cache->mutable_item_list()->erase(root["guid"].GetInt64());
                        act->Sync();
                        act->SendPush(0xe, nullptr);
                }
                return { true, "{ \"error_code\" : 200 }" };
        });

#endif

        return true;
}

#ifdef ANNOUNCEMENT_SERVICE_CLIENT

SERVICE_NET_HANDLE(AnnouncementService::SessionType, E_MCMT_Announcement, E_MCANNST_Sync, MsgAnnouncementList)
{
        AnnouncementService::GetInstance()->_cache = msg;
}

#endif

#endif
