#include "PkgStatusService.h"

#ifdef PKG_STATUS_SERVICE_SERVER

bool PkgStatusActor::Init()
{
        if (!SuperType::Init())
                return false;

        InitLoadTimer();
        return true;
}

void PkgStatusActor::InitLoadTimer()
{
        std::weak_ptr<PkgStatusActor> weakThis = shared_from_this();
        _timer.Start(weakThis, 1.0, [weakThis]() {
                auto thisPtr = weakThis.lock();
                if (thisPtr)
                {
                        auto tmpList = std::make_shared<decltype(PkgStatusService::_statusList)::element_type>("PkgStatus");
                        std::string sql = fmt::format("SELECT v,s FROM pkg_status;");
                        auto result = MySqlMgr::GetInstance()->Exec(sql);
                        for (auto row : result->rows())
                        {
                                auto info = std::make_shared<stPkgStatusInfo>();
                                info->_version = row.at(0).as_string();
                                info->_status = row.at(1).as_int64();
                                info->_reqRetStr = fmt::format("{}\"v\":{},\"s\":{}{}", "{", info->_version, info->_status, "}");
                                tmpList->Add(info->_version, info);
                        }

                        if (!tmpList->Empty())
                                PkgStatusService::GetInstance()->_statusList = tmpList;

                        thisPtr->InitLoadTimer();
                }
        });
}

#endif
