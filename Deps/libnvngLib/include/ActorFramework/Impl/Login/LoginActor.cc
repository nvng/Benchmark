#include "LoginActor.h"

#include "App.h"
#include "LoginGateSession.h"
#include "LoginDBSession.h"

struct stClientLoginCheckMail : public stActorMailBase
{
        std::weak_ptr<LoginGateSession> _ses;
        std::shared_ptr<MsgClientLoginCheck> _msg;
        LoginGateSession::MsgHeaderType _msgHead;
};

SPECIAL_ACTOR_MAIL_HANDLE(LoginActor, E_MCLST_Login, stClientLoginCheckMail)
{
        auto pb = msg->_msg;
        auto thisPtr = shared_from_this();
        LoginActorWeakPtr weakThis = thisPtr;
        const auto accountGuid = MySqlMgr::GenDataKey(1, pb->user_id());

        auto dbSes = GetApp()->DistSession(pb->user_id());
        if (!dbSes)
        {
                LOG_WARN("玩家[{}]登录时，db ses 分配失败!!!", accountGuid);
                return nullptr;
        }

        auto agent = std::make_shared<LoginDBSession::ActorAgentType>(accountGuid, dbSes);
        agent->BindActor(thisPtr);
        dbSes->AddAgent(agent);

        auto return2ClientFunc = [msg, agent](const auto& ret) {
                auto ses = msg->_ses.lock();
                if (!ses)
                        return;

                ses->SendPB(ret,
                            E_MCMT_Login,
                            E_MCLST_Login,
                            LoginGateSession::MsgHeaderType::E_RMT_Send,
                            msg->_msgHead._guid,
                            msg->_msgHead._to,
                            msg->_msgHead._from);
        };

        auto save2MysqlFunc = [weakThis, agent, msg, accountGuid](const auto& bufRef, std::size_t bufSize) {
                auto thisPtr = weakThis.lock();
                if (!thisPtr)
                        return;

                auto pb = msg->_msg;
                auto saveDBData = std::make_shared<MsgDBData>();
                saveDBData->set_guid(accountGuid);
                saveDBData->set_version(0);
                auto [base64DataRef, base64DataSize] = Base64EncodeExtra(bufRef.get(), bufSize);
                auto saveRet = ParseMailData<MsgDBData>(thisPtr->CallInternal(agent, E_MIMT_DB, E_MIDBST_SaveDBData, saveDBData, base64DataRef, base64DataRef.get(), base64DataSize).get(), E_MIMT_DB, E_MIDBST_SaveDBData);
                if (!saveRet)
                {
                        LOG_WARN("玩家[{}] 登录数据存储到 DBServer 超时!!!", accountGuid);
                        return;
                }
        };

        auto save2RedisFunc = [this, accountGuid](std::string_view data = {}) {
                std::string key = fmt::format("a:{}", accountGuid);
                bredis::command_container_t cmdList;
                cmdList.reserve(2);
                if (!data.empty())
                        cmdList.emplace_back(bredis::single_command_t("SET", key, data));
                cmdList.emplace_back(bredis::single_command_t("EXPIRE", key, EXPIRE_TIME_STR));
                RedisCmd(std::move(cmdList));
        };

        auto loadFromMySqlFunc = [weakThis, agent, msg, accountGuid, save2RedisFunc, save2MysqlFunc, return2ClientFunc]() {
                auto thisPtr = weakThis.lock();
                if (!thisPtr)
                        return;

                auto loadDBVersion = std::make_shared<MsgDBDataVersion>();
                loadDBVersion->set_guid(accountGuid);
                auto versionRet = Call(MsgDBDataVersion, thisPtr, agent, E_MIMT_DB, E_MIDBST_DBDataVersion, loadDBVersion);
                if (!versionRet)
                {
                        LOG_INFO("玩家[{}] 请求数据版本超时!!!", accountGuid);
                        return;
                }

                auto loadDBData = std::make_shared<MsgDBData>();
                loadDBData->set_guid(accountGuid);
                auto loadMailRet = thisPtr->CallInternal(agent, E_MIMT_DB, E_MIDBST_LoadDBData, loadDBData);
                if (!loadMailRet)
                {
                        LOG_WARN("玩家[{}] load from mysql 时，超时!!!", thisPtr->GetID());
                        return;
                }

                MsgDBData loadRet;
                auto [bufRef, buf, parseRet] = loadMailRet->ParseExtra(loadRet);
                if (!parseRet)
                {
                        LOG_WARN("玩家[{}] load from mysql 时，解析出错!!!", thisPtr->GetID());
                        return;
                }

                auto pb = msg->_msg;
                DBLoginInfo dbInfo;
                uint64_t playerGuid = 0;
                if (buf.empty())
                {
                        playerGuid = GenGuidService::GetInstance()->GenGuid<LoginActor, E_GGT_PlayerGuid>(thisPtr);
                        if (GEN_GUID_SERVICE_INVALID_GUID == playerGuid)
                                return;

                        dbInfo.set_app_id(pb->app_id());
                        dbInfo.set_user_id(pb->user_id());
                }
                else
                {
                        Compress::UnCompressAndParseAlloc<Compress::E_CT_Zstd>(dbInfo, Base64Decode(buf));
                        playerGuid = dbInfo.player_guid();
                }

                LOG_INFO("1111111111111111111111111111111 player_guid[{}]", playerGuid);
                dbInfo.set_token(pb->token());
                dbInfo.set_sign(pb->sign());
                dbInfo.set_player_guid(playerGuid);

                {
                        auto [bufRef, bufSize] = Compress::SerializeAndCompress<Compress::E_CT_Zstd>(dbInfo);
                        save2MysqlFunc(bufRef, bufSize);
                        save2RedisFunc(std::string_view(bufRef.get(), bufSize));
                }

                auto ret = std::make_shared<MsgClientLoginCheckRet>();
                ret->set_code(200);
                ret->set_id(playerGuid);
                return2ClientFunc(ret);
        };

        auto httpVerifyFunc = [this, msg, loadFromMySqlFunc, return2ClientFunc]() {
                auto pb = msg->_msg;
                std::string body = fmt::format("appid={}&userid={}&times={}&token={}&sign={}",
                                               pb->app_id(), pb->user_id(), pb->times(), pb->token(), pb->sign());
                auto verifyRet = HttpReq("http://sdk2.muugame.com/api/verifyToken", body, "application/x-www-form-urlencoded;charset=UTF-8");
                if (!verifyRet)
                        return;

                std::string retMsg;
                int64_t retCode = 1;
                do
                {
                        if (verifyRet->_body.empty())
                                break;

                        rapidjson::Document root;
                        if (root.Parse(verifyRet->_body.data(), verifyRet->_body.length()).HasParseError())
                                break;

                        if (!root.HasMember("code") || !root.HasMember("msg"))
                                break;

                        retCode = root["code"].GetInt64();
                        if (200 == retCode)
                        {
                                loadFromMySqlFunc();
                                return;
                        }
                        else
                        {
                                retMsg = root["msg"].GetString();
                                LOG_INFO("111111111111 retMsg[{}]", retMsg);
                        }
                } while (0);

                auto ret = std::make_shared<MsgClientLoginCheckRet>();
                ret->set_code(retCode);
                ret->set_msg(retMsg);
                return2ClientFunc(ret);
        };

        auto redisCmd = fmt::format("a:{}", accountGuid);
        auto redisRet = RedisCmd("GET", redisCmd);
        if (redisRet && !redisRet->IsNil())
        {
                auto [data, err] = redisRet->GetStr();
                if (!err)
                {
                        DBLoginInfo dbInfo;
                        Compress::UnCompressAndParseAlloc<Compress::E_CT_Zstd>(dbInfo, data);

                        save2RedisFunc();

                        auto ret = std::make_shared<MsgClientLoginCheckRet>();
                        ret->set_code(200);
                        ret->set_id(dbInfo.player_guid());

                        return2ClientFunc(ret);
                        return nullptr;
                }
        }

        httpVerifyFunc();
        return nullptr;
}

NET_MSG_HANDLE(LoginGateSession, E_MCMT_Login, E_MCLST_Login, MsgClientLoginCheck)
{
        auto act = GetApp()->DistLoginActor(msg->user_id());
        if (act)
        {
                auto m = std::make_shared<stClientLoginCheckMail>();
                m->_ses = shared_from_this();
                m->_msg = msg;
                m->_msgHead = msgHead;
                act->SendPush(E_MCLST_Login, m);
        }
        else
        {
                auto ret = std::make_shared<MsgClientLoginCheckRet>();
                ret->set_code(1);
                SendPB(ret, E_MCMT_Login, E_MCLST_Login, MsgHeaderType::E_RMT_Send, msgHead._guid, msgHead._to, msgHead._from);
        }
}

// vim: fenc=utf8:expandtab:ts=8
