#pragma once

#include <stdint.h>
#include <deque>

class FrameControllerInitArg;
class FrameController
{
public :
        explicit FrameController(const FrameControllerInitArg&);

        void SetFrameCntPerSec(int cnt);
        void Update(bool updateTime);

        inline uint64_t GetCurFrameCnt() { return mFrameCnt; }
        int GetAverageFrameCnt();
private :
        double mFrameDiffTime;
        double mPreFrameBeginTime;
        uint64_t mFrameCnt;
        std::deque<double> mLastTenFrameTimeStep;
};

FrameController& GetFrameController();

// vim: fenc=utf8:expandtab:ts=8:noma
