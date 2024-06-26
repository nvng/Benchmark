#pragma once

#include "PlayerMgr.h"
#include "PlayerBase.h"

class Player : public PlayerBase
{
        typedef PlayerBase SuperType;
public :
        Player(GUID_TYPE guid, const MsgClientLogin& msg);
        ~Player() override;

        bool Init() override;
        void OnCreateAccount(const std::shared_ptr<MsgClientLogin>& msg) override;

        void Online() override;
        using SuperType::Push;
        void Push()
        {
        }

public :
        EXTERN_ACTOR_MAIL_HANDLE();
        DECLARE_SHARED_FROM_THIS(Player);
};

typedef std::shared_ptr<Player> PlayerPtr;
typedef std::weak_ptr<Player> PlayerWeakPtr;

// vim: fenc=utf8:expandtab:ts=8
