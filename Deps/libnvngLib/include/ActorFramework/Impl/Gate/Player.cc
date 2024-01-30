#include "Player.h"

#include "GateLobbySession.h"
#include "GateGameSession.h"

namespace nl::af::impl {

std::atomic_int64_t Player::_playerFlag = 0;

Player::Player(uint64_t id, const std::shared_ptr<GateClientSession>& clientSes)
        : _clientSes(clientSes)
        , _uniqueID(PlayerMgr::GetInstance()->GenUniqueID())
        , _id(id)
{
        // LOG_INFO("aaaaaaaaaaaaaaaaaa Player::Player()");
        ++_playerFlag;
}

Player::~Player()
{
	// DLOG_INFO("aaaaaaaaaaaaaaaaaa Player::~Player() id:{} ptr:{}", GetID(), (void*)this);

        --_playerFlag;
        Send2Lobby(nullptr, E_MCMT_ClientCommon, E_MCCCST_Disconnect);
}

void Player::ClearSessionRelation()
{
	auto lobbySes = _lobbySes.lock();
	if (lobbySes)
                lobbySes->RemovePlayer(GetID(), this);

	auto gameSes = _gameSes.lock();
	if (gameSes)
                gameSes->RemovePlayer(GetID(), this);
}

void Player::OnLobbyClose()
{
        ClearSessionRelation();
        Send2Client(nullptr, E_MCMT_Internal, E_MCCCST_Logout);
}

void Player::Send2Lobby(const ISession::BuffTypePtr& bufRef,
                        ISession::BuffTypePtr::element_type* buf,
                        std::size_t bufSize,
                        Compress::ECompressType ct,
                        uint64_t mainType,
                        uint64_t subType)
{
	auto ses = _lobbySes.lock();
	if (ses)
		ses->SendBuf(bufRef, buf, bufSize, ct, mainType, subType,
                             GateLobbySession::MsgHeaderType::E_RMT_Call, 0, GetUniqueID(), GetID());
}

void Player::Send2Lobby(const MessageLitePtr& msg, uint64_t mainType, uint64_t subType)
{
	auto ses = _lobbySes.lock();
	if (ses)
                ses->SendPB(msg, mainType, subType, GateLobbySession::MsgHeaderType::E_RMT_Call, 0, GetUniqueID(), GetID());
}

void Player::OnGameClose()
{
	// TODO:此处没有像 lobby 那样，game server ses 重新连接后会触发自动重连接，因此需要客户端做处理。
	auto gameSes = _gameSes.lock();
	if (gameSes)
                gameSes->RemovePlayer(GetID(), this);
	Send2Client(nullptr, E_MCMT_GameCommon, E_MCGCST_GameDisconnect);
}

void Player::Send2Game(const ISession::BuffTypePtr& bufRef,
                       ISession::BuffTypePtr::element_type* buf,
                       std::size_t bufSize,
                       Compress::ECompressType ct,
                       uint64_t mainType,
                       uint64_t subType)
{
        // Region handle 返回有歧义，返回给单个玩家还是 Region 层面的广播，黑紫默认不返回，由用户调用相应函数。
	auto ses = _gameSes.lock();
	if (ses)
		ses->SendBuf(bufRef, buf, bufSize, ct, mainType, subType,
                             GateGameSession::MsgHeaderType::E_RMT_Send, 0, GetID(), _regionGuid);
}

void Player::OnClientClose()
{
#ifdef ____BENCHMARK____
        FLAG_ADD(_internalFlag, E_GPFT_Delete);
#endif
        PlayerMgr::GetInstance()->RemovePlayer(GetID(), this);
        PlayerMgr::GetInstance()->RemoveLoginPlayer(GetID(), this);
}

void Player::Send2Client(const MessageLitePtr& msg, uint64_t mainType, uint64_t subType)
{
	auto ses = _clientSes.lock();
	if (ses)
		ses->SendPB(msg, mainType, subType);
}

}; // end of namespace nl::af::impl

// vim: fenc=utf8:expandtab:ts=8
