#pragma once

#include "PlayerMgr.h"
#include "PlayerBase.h"
#include "Activity.h"

#define INVALID_MONEY_VAL INT64_MIN

struct stTest
{
        stTest() : _ch(128) { }
        void Run()
        {
                go [this]() {
                        std::shared_ptr<int64_t> v;
                        while (true)
                                _ch >> v;
                };
        }
        co_chan<std::shared_ptr<int64_t>> _ch;
};

class Player : public PlayerBase
{
        typedef PlayerBase SuperType;
public :
        Player(uint64_t guid, const std::string& nickName, const std::string& icon);
        ~Player() override;

        bool Init() override;
        void OnCreateAccount() override;

        void Online() override;
        using SuperType::Push;
        void Push()
        {
                for (auto t : testList)
                {
                        auto m = std::make_shared<int64_t>(1);
                        if (!t->_ch.TryPush(m))
                        {
                                // LOG_INFO("111111111111111111111111111111111111111111");
                                t->_ch << m;
                        }
                }
        }

        std::vector<stTest*> testList;

public :
        ActivityMgr _activityMgr;

        EXTERN_ACTOR_MAIL_HANDLE();
        DECLARE_SHARED_FROM_THIS(Player);
};

typedef std::shared_ptr<Player> PlayerPtr;
typedef std::weak_ptr<Player> PlayerWeakPtr;

// vim: fenc=utf8:expandtab:ts=8
