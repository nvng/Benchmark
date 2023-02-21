#pragma once

#include "PlayerMgr.h"
#include "PlayerBase.h"

#define INVALID_MONEY_VAL INT64_MIN

class Player : public nl::af::impl::PlayerBase
{
        typedef nl::af::impl::PlayerBase SuperType;
public :
        Player(uint64_t guid, const std::string& nickName);
        ~Player() override;

        bool Init() override;

        void Online() override;

        EXTERN_ACTOR_MAIL_HANDLE();
        DECLARE_SHARED_FROM_THIS(Player);
};

typedef std::shared_ptr<Player> PlayerPtr;
typedef std::weak_ptr<Player> PlayerWeakPtr;

// vim: fenc=utf8:expandtab:ts=8
