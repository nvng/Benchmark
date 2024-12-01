#include "GenGuidService.h"

#ifdef GEN_GUID_SERVICE_SERVER

bool GenGuidActor::Init()
{
        if (!SuperType::Init())
                return false;

        const auto dbGuid = MySqlMgr::GetInstance()->GenDataKey(E_MIMT_GenGuid) + _idx;
        std::string sql = fmt::format("SELECT data FROM data_0 WHERE id={};", dbGuid);
        auto result = MySqlMgr::GetInstance()->Exec(sql);
        if (result->rows().empty())
        {
                auto sql = fmt::format("INSERT INTO data_0(id, v, data) VALUES({}, 0, \"\")", dbGuid);
                MySqlMgr::GetInstance()->Exec(sql);
        }
        else
        {
                for (auto row : result->rows())
                {
                        auto data = row.at(0).as_string();

                        DBGenGuid dbInfo;
                        Compress::UnCompressAndParseAlloc<Compress::E_CT_Zstd>(dbInfo, Base64Decode(data));

                        _itemList.reserve(dbInfo.item_list_size());
                        for (auto& msgItem : dbInfo.item_list())
                        {
                                auto item = std::make_shared<stGenGuidItem>();
                                item->UnPack(msgItem);
                                _itemList.emplace_back(item);
                        }
                }
        }

        return true;
}

void GenGuidActor::Save2DB()
{
        if (_inTimer)
                return;

        _inTimer = true;
        std::weak_ptr<GenGuidActor> weakThis = shared_from_this();
        _timer.Start(weakThis, 30.0, [weakThis]() {
                auto thisPtr = weakThis.lock();
                if (thisPtr)
                {
                        thisPtr->_inTimer = false;
                        thisPtr->Flush2DB();
                }
        });
}

void GenGuidActor::Flush2DB()
{
        DBGenGuid dbInfo;
        for (auto& item : _itemList)
                item->Pack(*dbInfo.add_item_list());

        const auto dbGuid = MySqlMgr::GetInstance()->GenDataKey(E_MIMT_GenGuid) + _idx;
        auto [bufRef, bufSize] = Compress::SerializeAndCompress<Compress::E_CT_Zstd>(dbInfo);
        std::string sql = fmt::format("UPDATE data_0 SET data=\"{}\" WHERE id={};", Base64Encode(bufRef.get(), bufSize), dbGuid);
        MySqlMgr::GetInstance()->Exec(sql);
}

uint64_t GenGuidActor::GenGuid()
{
        if (_itemList.empty())
                return GEN_GUID_SERVICE_INVALID_GUID;

        int64_t idx = RandInRange(0, _itemList.size());
        auto& info = _itemList[idx];
        uint64_t guid = info->_cur++;
        if (info->_cur > info->_max)
        {
                _itemList.erase(std::remove_if(_itemList.begin(), _itemList.end()
                                               , [info](const auto& v) { return v == info; }), _itemList.end());
        }

        // Save2DB();
        return guid;
}

void GenGuidActor::Terminate()
{
        std::weak_ptr<GenGuidActor> weakThis = shared_from_this();
        SendPush([weakThis]() {
                auto thisPtr = weakThis.lock();
                if (thisPtr)
                {
                        thisPtr->Flush2DB();
                        thisPtr->SuperType::Terminate();
                }
        });
}

SPECIAL_ACTOR_MAIL_HANDLE(GenGuidActor, E_MIGGST_Req, GenGuidService::SessionType::stServiceMessageWapper)
{
        auto ses = msg->_agent->GetSession();
        if (!ses)
                return nullptr;

        auto ret = std::make_shared<MsgGenGuid>();
        ret->set_id(GenGuid());
        ret->set_error_type(E_IET_Success);
        ses->SendPB(ret,
                    E_MIMT_GenGuid,
                    E_MIGGST_Req,
                    GenGuidService::SessionType::MsgHeaderType::E_RMT_CallRet,
                    msg->_msgHead._guid,
                    msg->_msgHead._to,
                    msg->_msgHead._from);

        return nullptr;
}

SERVICE_NET_HANDLE(GenGuidService::SessionType, E_MIMT_GenGuid, E_MIGGST_Req, MsgGenGuid)
{
        auto act = GenGuidService::GetInstance()->GetActor(msg->type());
        if (act)
        {
                auto m = std::make_shared<GenGuidService::SessionType::stServiceMessageWapper>();
                m->_agent = std::make_shared<typename GenGuidService::SessionType::ActorAgentType>(msgHead._from, shared_from_this());
                m->_agent->BindActor(act);
                AddAgent(m->_agent);

                m->_msgHead = msgHead;
                m->_pb = msg;
                act->SendPush(E_MIGGST_Req, m);
        }
}

#endif

template <>
bool GenGuidService::Init()
{
        if (!SuperType::Init())
                return false;

        return true;
}

// vim: fenc=utf8:expandtab:ts=8:noma
