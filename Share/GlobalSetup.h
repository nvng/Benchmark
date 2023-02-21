#pragma once

#include "Share/GlobalDef.h"

class GlobalSetup_CH : public Singleton<GlobalSetup_CH>
{
        GlobalSetup_CH() {}
        ~GlobalSetup_CH() {}
        friend class Singleton<GlobalSetup_CH>;

public :
        bool Init();

public :
        time_t _openServerTime = 0;
        time_t _openServerHour = 0;
        time_t _openServerDay = 0;

        int64_t _dataResetNonZero = 0;
};

// vim: fenc=utf8:expandtab:ts=8
