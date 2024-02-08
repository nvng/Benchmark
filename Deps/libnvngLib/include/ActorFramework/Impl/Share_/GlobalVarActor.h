#pragma once

namespace nl::af::impl {

SPECIAL_ACTOR_DEFINE_BEGIN(GlobalVarActor);

public :
        GlobalVarActor() : SuperType(SpecialActorMgr::GenActorID(), IActor::scMailQueueMaxSize) { }

        bool Init() override
        {
                if (!SuperType::Init())
                        return false;

                std::string cmd = fmt::format("global_var:{}:{}", GetAppBase()->GetRID(), GetAppBase()->GetSID());
                auto ret = RedisCmd("HGETALL", cmd);
                if (ret->IsArr())
                {
                        ret->ForeachArr([this](std::string_view k, std::string_view v) {
                                _globalVarList.emplace(k, v);
                        });
                        GetAppBase()->_startPriorityTaskList->Finish(_priorityTaskKey);
                }
                else
                {
                        LOG_FATAL("load global var fail!!!");
                }

                return true;
        }

        inline void AddVar(const std::string& key, const std::string& val)
        {
                std::lock_guard l(_globalVarListMutex);
                _globalVarList[key] = val;
                Save();
        }

        inline void AddVar(const std::string& key, int64_t val)
        {
                AddVar(key, fmt::format_int(val).str());
        }

        inline void RemoveVar(const std::string& key)
        {
                std::lock_guard l(_globalVarListMutex);
                _globalVarList.erase(key);
                Save();
        }

        inline std::string GetVar(const std::string& key)
        {
                std::lock_guard l(_globalVarListMutex);
                auto it = _globalVarList.find(key);
                return _globalVarList.end()!=it ? it->second : std::string();
        }

        inline void Save()
        {
                SendPush(0, nullptr);
        }

        inline int64_t GetOrCreateIfNotExist(const std::string& key, int64_t initVal)
        {
                std::string val = GetVar(key);
                if (val.empty())
                {
                        AddVar(key, initVal);
                        return initVal;
                }
                else
                {
                        return atoll(val.c_str());
                }
        }

        inline std::string GetOrCreateIfNotExist(const std::string& key, const std::string& initVal)
        {
                std::string val = GetVar(key);
                if (val.empty())
                {
                        val = initVal;
                        AddVar(key, val);
                }

                return val;
        }

private :
        SpinLock _globalVarListMutex;
        std::map<std::string, std::string> _globalVarList;

public :
        static std::string _priorityTaskKey;

SPECIAL_ACTOR_DEFINE_END(GlobalVarActor);

}; // end of namespace nl::af::impl

// vim: fenc=utf8:expandtab:ts=8:noma
