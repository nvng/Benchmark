#pragma once

#include "LobbyGateSessionBase.h"

class LobbyGateSession : public nl::af::impl::LobbyGateSessionBase
{
	typedef nl::af::impl::LobbyGateSessionBase SuperType;
public :
        LobbyGateSession();
        ~LobbyGateSession() override;

public :
	void OnConnect() override;
	void OnClose(int32_t reasonType) override;

	EXTERN_MSG_HANDLE();
        DECLARE_SHARED_FROM_THIS(LobbyGateSession);
};

// vim: fenc=utf8:expandtab:ts=8
