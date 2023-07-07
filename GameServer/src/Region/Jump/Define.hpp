#pragma once

namespace Jump {

struct StateEventInfo
{
        explicit StateEventInfo(int eventType)
                : _eventType(eventType)
        {
        }

        int       _eventType = 0;
        double    _dlParam = 0.0;
        bool      _bParam = false;
        int64_t   _param = 0;
        std::string_view _strParam;
        std::shared_ptr<void> _data;
        EClientErrorType _errorType = E_CET_Success;
};

}; // end namespace Jump

// vim: fenc=utf8:expandtab:ts=8
