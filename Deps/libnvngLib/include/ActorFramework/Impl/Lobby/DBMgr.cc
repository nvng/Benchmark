#include "DBMgr.h"

#include "PlayerBase.h"
#include "LobbyDBSession.h"

namespace nl::af::impl {

bool DBMgr::Init()
{
        _dbSesArr.resize(ServerListCfgMgr::GetInstance()->GetSize<stDBServerInfo>());
	return true;
}

bool DBMgr::LoadPlayer(const PlayerBasePtr& p)
{
	auto ses = DistSession(p->GetID());
	if (!ses)
        {
                int64_t cnt = 0;
                (void)cnt;
                for (auto& ws : _dbSesArr)
                {
                        if (ws.lock())
                                ++cnt;
                }
                PLAYER_LOG_WARN(p->GetID(), "玩家[{}] Load时，db ses 分配失败!!! sesCnt[{}]", p->GetID(), cnt);
		return false;
        }

	auto agent = std::make_shared<LobbyDBSession::ActorAgentType>(GenReqID(), ses);
	agent->BindActor(p);
	ses->AddAgent(agent);

	auto msg = std::make_shared<MsgDBDataVersion>();
	msg->set_guid(p->GetID());
	auto versionRet = Call(MsgDBDataVersion, p, agent, E_MIMT_DB, E_MIDBST_DBDataVersion, msg);
        if (!versionRet)
        {
                PLAYER_LOG_INFO(p->GetID(), "玩家[{}] 请求数据版本超时!!!", p->GetID());
                return false;
        }

        auto loadFromMysqlFunc = [p, agent, versionRet]() {
                auto loadDBData = std::make_shared<MsgDBData>();
                loadDBData->set_guid(p->GetID());
                auto loadMailRet = p->CallInternal(agent, E_MIMT_DB, E_MIDBST_LoadDBData, loadDBData);
                if (!loadMailRet)
                {
                        PLAYER_LOG_WARN(p->GetID(), "玩家[{}] load from mysql 时，超时!!!", p->GetID());
                        return false;
                }

                MsgDBData loadRet;
                auto [bufRef, buf, ret] = loadMailRet->ParseExtra(loadRet);
                if (!ret)
                {
                        PLAYER_LOG_WARN(p->GetID(), "玩家[{}] load from mysql 时，解析出错!!!", p->GetID());
                        return false;
                }

                if (E_IET_DBDataSIDError != loadRet.error_type())
                {
                        if (buf.empty())
                        {
                                p->OnCreateAccount();
                                p->Flush2DB(false);
                        }
                        else
                        {
                                DBPlayerInfo dbInfo;
                                Compress::UnCompressAndParseAlloc<Compress::E_CT_Zstd>(dbInfo, Base64Decode(buf));

                                PLAYER_LOG_WARN_IF(p->GetID(), versionRet->version() != dbInfo.version(), "玩家[{}] 从 MySql 获取数据 size:{}，版本不匹配!!! v:{} dbv:{}",
                                             p->GetID(), buf.length(), versionRet->version(), dbInfo.version());
                                p->InitFromDB(dbInfo);
                        }
                }
                else
                {
                        // p->KickOut(E_CET_StateError); // 无效，还没登录成功，_clientActor 为空。
                        return false;
                }
                return true;
        };

        std::string cmd = fmt::format("p:{}", p->GetID());
        auto loadRet = p->RedisCmd("GET", cmd);
        if (!loadRet)
                return false;

        if (loadRet->IsNil())
        {
                if (0 == versionRet->version())
                {
                        p->OnCreateAccount();
                        p->Flush2DB(false);
                }
                else
                {
                        PLAYER_LOG_WARN(p->GetID(), "玩家[{}] 从 redis 获取数据为空!!! v:{}",
                                 p->GetID(), versionRet->version());
                        if (!loadFromMysqlFunc())
                                return false;
                }
        }
        else
        {
                auto [data, err] = loadRet->GetStr();
                if (!err)
                {
                        DBPlayerInfo dbInfo;
                        Compress::UnCompressAndParseAlloc<Compress::E_CT_Zstd>(dbInfo, data);
                        if (versionRet->version() > dbInfo.version())
                        {
                                PLAYER_LOG_WARN(p->GetID(), "玩家[{}] 从 redis 获取数据，版本不匹配!!! v:{} dbv:{}",
                                                p->GetID(), versionRet->version(), dbInfo.version());
                                if (!loadFromMysqlFunc())
                                        return false;
                        }
                        else
                        {
                                p->InitFromDB(dbInfo);
                        }
                }
        }
        p->AfterInitFromDB();
        return true;
}

bool DBMgr::SavePlayer(const PlayerBasePtr& p, bool isDelete)
{
	auto ses = DistSession(p->GetID());
	if (!ses)
        {
                int64_t cnt = 0;
                (void)cnt;
                for (auto& ws : _dbSesArr)
                {
                        if (ws.lock())
                                ++cnt;
                }
                PLAYER_LOG_WARN(p->GetID(), "玩家[{}] 存储时，db ses 分配失败!!! sesCnt[{}]", p->GetID(), cnt);
		return false;
        }

	auto agent = std::make_shared<LobbyDBSession::ActorAgentType>(GenReqID(), ses);
	agent->BindActor(p);
	ses->AddAgent(agent);

        DBPlayerInfo dbInfo;
        p->Pack2DB(dbInfo);

        auto [bufRef, bufSize] = Compress::SerializeAndCompress<Compress::E_CT_Zstd>(dbInfo);

        auto saveDBData = std::make_shared<MsgDBData>();
        if (isDelete)
                saveDBData->set_error_type(E_IET_DBDataDelete);
        saveDBData->set_guid(p->GetID());
        saveDBData->set_version(dbInfo.version());
        auto [base64DataRef, base64DataSize] = Base64EncodeExtra(bufRef.get(), bufSize);
        // saveDBData->set_data(base64Data);
        /* 有问题。
        saveDBData->set_allocated_data(&base64Data);
        */

        auto saveRet = ParseMailData<MsgDBData>(p->CallInternal(agent, E_MIMT_DB, E_MIDBST_SaveDBData, saveDBData, base64DataRef, base64DataRef.get(), base64DataSize).get(), E_MIMT_DB, E_MIDBST_SaveDBData);
        if (!saveRet)
        {
                PLAYER_LOG_WARN(p->GetID(), "玩家[{}] 存储到 DBServer 超时!!!", p->GetID());
                return false;
        }

        std::string key = fmt::format("p:{}", p->GetID());
        p->RedisCmd("SET", key, std::string_view(bufRef.get(), bufSize));
        p->RedisCmd("EXPIRE", key, fmt::format_int(7*24*3600).c_str());

        return true;
}

bool DBMgr::AddSession(const std::shared_ptr<LobbyDBSession>& ses)
{
        if (!ses)
                return false;
        auto idx = ServerListCfgMgr::GetInstance()->CalSidIdx<stDBServerInfo>(ses->GetSID());
        if (INVALID_SERVER_IDX == idx)
                return false;
        _dbSesArr[idx] = ses;
        return true;
}

std::shared_ptr<LobbyDBSession> DBMgr::RemoveSession(int64_t sid)
{
        auto idx = ServerListCfgMgr::GetInstance()->CalSidIdx<stDBServerInfo>(sid);
        if (INVALID_SERVER_IDX == idx)
                return nullptr;
        auto ret = _dbSesArr[idx].lock();
        _dbSesArr[idx].reset();
        return ret;
}

}; // end of namespace nl::af::impl;

// vim: fenc=utf8:expandtab:ts=8
