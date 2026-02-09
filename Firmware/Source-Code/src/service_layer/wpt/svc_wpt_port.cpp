/**
 * @name Hornet / WPT Charger
 * @file svc_wpt_port.cpp
 * @brief WptPort class implementation
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "svc_wpt_port.h"

#include "eda_manager_log_config.h"
#include "svc_wpt_subsystem.h"

namespace svc
{
    WptPort::WptPort()
    {
    }

    void WptPort::ExecuteEvent(uint32_t eventId, uint32_t optDataAddress)
    {
        LOG_DEBUG("WPT Port: ExecuteEvent\n");
        WptSubsystem::Instance().DispatchEvent(eventId, optDataAddress);
        WptPort::ExecuteCallback(eventId);
    }
}