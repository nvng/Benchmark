#pragma once

#include "IActor.h"

namespace nl::af
{

class IService
{
public :
        virtual ~IService() { }

        virtual bool Init() { return true; }

        virtual bool AddActor(const IActorPtr& act) = 0;
        virtual void RemoveActor(const IActorPtr& act) = 0;

        virtual void Terminate() = 0;
        virtual void WaitForTerminate() = 0;
};

}; // end of namespace nl::af

// vim: fenc=utf8:expandtab:ts=8:
