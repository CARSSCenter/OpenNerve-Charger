/**
 * @name Hornet / WPT Charger
 * @file svc_pmc_port.cpp
 * @brief PmcPort class implementation
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "svc_pmc_port.h"

#include "eda_manager_log_config.h"

#include "svc_pmc_subsystem.h"

namespace svc
{
    PmcPort::PmcPort()
    {
    }

    void PmcPort::ExecuteEvent(uint32_t eventId, uint32_t optDataAddress)
    {
        LOG_DEBUG("PMC Port: ExecuteEvent\n");
        PmcSubSystem::Instance().DispatchEvent(eventId, optDataAddress);
    }
}