#include "GateLoginSession.h"

#include "NetMgrImpl.h"
#include "GateClientSession.h"

namespace nl::af::impl {

const std::string GateLoginSession::scPriorityTaskKey = "gate_login_load";

GateLoginSession::~GateLoginSession()
{
}

void GateLoginSession::OnConnect()
{
	SuperType::OnConnect();
}

void GateLoginSession::OnClose(int32_t reasonType)
{
	SuperType::OnClose(reasonType);
	NetMgrImpl::GetInstance()->RemoveSession(E_GST_Login, this);
}

void GateLoginSession::MsgHandleServerInit(const ISessionPtr& ses, typename ISession::BuffTypePtr::element_type* buf, const ISession::BuffTypePtr& bufRef)
{
        SuperType::MsgHandleServerInit(ses, buf, bufRef);
	NetMgrImpl::GetInstance()->AddSession(E_GST_Login, std::dynamic_pointer_cast<GateLoginSession>(ses));
        GetAppBase()->_startPriorityTaskList->Finish(scPriorityTaskKey);
        LOG_WARN("初始化登录服 sid[{}] 成功!!!", ses->GetSID());
}

void GateLoginSession::OnRecv(typename SuperType::BuffTypePtr::element_type* buf, const typename SuperType::BuffTypePtr& bufRef)
{
        auto msgHead = *reinterpret_cast<MsgHeaderType*>(buf);
        auto clientSes = std::dynamic_pointer_cast<GateClientSession>(::nl::net::client::ClientNetMgr::GetInstance()->_sesList.Get(msgHead._to).lock());
        if (clientSes)
        {
                clientSes->SendBuf<MsgHeaderType>(bufRef, buf, msgHead._size, msgHead.CompressType(), msgHead.MainType(), msgHead.SubType(), 0);
        }
        else
        {
                LOG_ERROR("登录服返回消息后，没找到对应 clientSes!!!");
        }
}

}; // end of namespace nl::af::impl

// vim: fenc=utf8:expandtab:ts=8
