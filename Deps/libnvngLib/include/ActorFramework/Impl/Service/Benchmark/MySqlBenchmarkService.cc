#include "MySqlBenchmarkService.h"

#ifdef MYSQL_BENCHMARK_SERVICE_CLIENT

ACTOR_MAIL_HANDLE(MySqlBenchmarkActor, 0xfff, 0x0)
{
        boost::this_fiber::sleep_for(std::chrono::seconds(10));

        auto msg = std::make_shared<MsgDBDataVersion>();
        msg->set_guid(GetID());
        auto agent = MySqlBenchmarkService::GetInstance()->GetActor(shared_from_this(), GetID());
        auto versionRet = Call(MsgDBDataVersion, agent, E_MIMT_DB, E_MIDBST_DBDataVersion, msg);
        if (!versionRet)
        {
                LOG_INFO("玩家[{}] 请求数据版本超时!!!", GetID());
                return nullptr;
        }

        static std::string str = GenRandStr(1024 * 10);
        MsgClientLogin saveMsg;
        saveMsg.set_nick_name(str);

        auto [bufRef, bufSize] = Compress::SerializeAndCompress<Compress::E_CT_Zstd>(saveMsg);
        auto saveDBData = std::make_shared<MsgDBData>();
        saveDBData->set_guid(GetID());
        auto [base64DataRef, base64DataSize] = Base64EncodeExtra(bufRef.get(), bufSize);
        // saveDBData->set_data(base64Data);
        while (true)
        {
                int64_t i = 0;
                // for (; i<10; ++i)
                        // Call(MsgDBData, agent, E_MIMT_DB, E_MIDBST_SaveDBData, saveDBData);

                ++i;
                // Call(MsgDBData, agent, E_MIMT_DB, E_MIDBST_SaveDBData, saveDBData);
                ParseMailData<MsgDBData>(CallInternal(agent, E_MIMT_DB, E_MIDBST_SaveDBData, saveDBData, base64DataRef, base64DataRef.get(), base64DataSize).get(), E_MIMT_DB, E_MIDBST_SaveDBData);

                GetApp()->_cnt += i;
        }

        return nullptr;
}

#endif
