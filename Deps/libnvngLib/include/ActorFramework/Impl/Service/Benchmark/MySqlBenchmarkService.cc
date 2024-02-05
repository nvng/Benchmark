#include "MySqlBenchmarkService.h"

#ifdef MYSQL_BENCHMARK_SERVICE_CLIENT

ACTOR_MAIL_HANDLE(MySqlBenchmarkActor, 0xfff, 0x0)
{
        boost::this_fiber::sleep_for(std::chrono::seconds(10));

        static constexpr int64_t cnt = 1000;
        auto mail = std::make_shared<MailReqDBDataList>();
        for (int64_t i=0; i<cnt; ++i)
        {
                auto item = mail->add_list();
                item->set_task_type(E_MySql_TT_Version);
                item->set_guid((i+1)*(1000 * 1000 * 1000LL) + 1000 * 1000 + GetID());
        }
        auto agent = MySqlBenchmarkService::GetInstance()->GetActor(shared_from_this(), GetID());
        auto versionRet = Call(MailReqDBDataList, agent, E_MIMT_DB, E_MIDBST_ReqDBDataList, mail);
        if (!versionRet)
        {
                LOG_INFO("玩家[{}] 请求数据版本超时!!!", GetID());
                return nullptr;
        }

        static std::string str = GenRandStr(1024);
        MsgClientLogin saveMsg;
        saveMsg.set_nick_name(str);

        // auto [bufRef, bufSize] = Compress::SerializeAndCompress<Compress::E_CT_Zstd>(saveMsg);
        // auto [base64DataRef, base64DataSize] = Base64EncodeExtra(bufRef.get(), bufSize);
        auto [base64DataRef, base64DataSize] = Base64EncodeExtra(str.c_str(), str.size());

        auto saveDBData = std::make_shared<MailReqDBDataList>();
        for (int64_t i=0; i<cnt; ++i)
        {
                auto saveItem = saveDBData->add_list();
                saveItem->set_task_type(E_MySql_TT_Save);
                saveItem->set_guid((i+1)*(1000 * 1000 * 1000LL) + 1000 * 1000 + GetID());
                saveItem->set_data(base64DataRef.get(), base64DataSize);
        }
        Call(MailReqDBData, agent, E_MIMT_DB, E_MIDBST_ReqDBDataList, saveDBData);
        while (true)
        {
                // int64_t i = 0;
                // for (; i<10; ++i)
                        // Call(MsgDBData, agent, E_MIMT_DB, E_MIDBST_SaveDBData, saveDBData);

                Call(MailReqDBData, agent, E_MIMT_DB, E_MIDBST_ReqDBDataList, saveDBData);
                // ParseMailData<MsgDBData>(CallInternal(agent, E_MIMT_DB, E_MIDBST_SaveDBData, saveDBData, base64DataRef, base64DataRef.get(), base64DataSize).get(), E_MIMT_DB, E_MIDBST_SaveDBData);

                /*
                saveDBData->set_task_type(E_MySql_TT_Load);
                auto loadRet = Call(MailReqDBData, agent, E_MIMT_DB, E_MIDBST_ReqDBDataList, saveDBData);
                if (loadRet->data() == std::string(base64DataRef.get(), base64DataSize))
                        LOG_INFO("1111111111111111111");
                else
                        LOG_INFO("2222222222222222222 ret[{}] base[{}]"
                                 , loadRet->data(), std::string(base64DataRef.get(), base64DataSize));
                                 */

                GetApp()->_cnt += cnt;
        }

        return nullptr;
}

ACTOR_MAIL_HANDLE(MySqlBenchmarkActor, 0xfff, 0x1)
{
        /*
        boost::this_fiber::sleep_for(std::chrono::seconds(10));

        static constexpr int64_t cnt = 10000;
        std::unordered_map<uint64_t, std::pair<std::shared_ptr<MailReqDBDataList>, MySqlService::EMySqlServiceStatus>> dataList;
        dataList.reserve(cnt);
        for (int64_t i=0; i<cnt; ++i)
        {
                uint64_t id = (i+1)*(1000 * 1000 * 1000LL) + 1000 * 1000 + GetID();
                auto dbInfo = std::make_shared<MailReqDBDataList>();
                auto v = std::make_pair(dbInfo, MySqlService::E_MySqlS_None);
                dataList.emplace(id, v);
        }

        {
                TimeCost l("load");
        MySqlService::GetInstance()->LoadBatch(shared_from_this(), dataList, "test");
        }

        std::vector<std::tuple<uint64_t, std::shared_ptr<MailReqDBDataList>, bool>> saveList;
        saveList.reserve(cnt);
        for (int64_t i=0; i<cnt; ++i)
        {
                uint64_t id = (i+1)*(1000 * 1000 * 1000LL) + 1000 * 1000 + GetID();
                auto dbInfo = std::make_shared<MailReqDBDataList>();
                auto item = dbInfo->add_list();
                item->set_data(fmt::format("{}_{}", id, "abc"));
                auto v = std::make_tuple(id, dbInfo, true);
                saveList.emplace_back(v);
        }

        {
                TimeCost l("save");
                MySqlService::GetInstance()->SaveBatch(shared_from_this(), saveList, "test");
        }

        boost::this_fiber::sleep_for(std::chrono::seconds(2));

        dataList.clear();
        for (int64_t i=0; i<cnt; ++i)
        {
                uint64_t id = (i+1)*(1000 * 1000 * 1000LL) + 1000 * 1000 + GetID();
                auto dbInfo = std::make_shared<MailReqDBDataList>();
                auto v = std::make_pair(dbInfo, MySqlService::E_MySqlS_None);
                dataList.emplace(id, v);
        }
        {
                TimeCost l("load");
                MySqlService::GetInstance()->LoadBatch(shared_from_this(), dataList, "test");
                for (auto& v : dataList)
                {
                        auto id = v.first;
                        auto dbInfo = v.second.first;
                        for (auto& item : dbInfo->list())
                        {
                                auto data = fmt::format("{}_{}", id, "abc");
                                if (item.data() != data)
                                        LOG_INFO("77777777777777777777");
                                else
                                        ; // LOG_INFO("88888888888888888888");
                        }
                }

        }
        */

        return nullptr;
}

#endif
