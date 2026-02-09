/**
 * @name Hornet / WPT Charger
 * @file svc_pmc_subsystem.cpp
 * @brief PmcSubsystem class implementation
 * 
 * @copyright Copyright (c) 2024
 *
 */

#include "svc_pmc_subsystem.h"

#include "eda_active_object_priorities.h"

namespace svc
{
    PmcSubSystem &PmcSubSystem::Instance()
    {
        static PmcSubSystem instance;
        return instance;
    }

    PmcSubSystem::PmcSubSystem() : mPmcActiveObject(), mPmcPort(), mPmcStateMachine() {}

    void PmcSubSystem::Init()
    {
        mPmcActiveObject.InitTask( eda::ActiveObjectPriorities_e::svc_1, "PMC");
        mPmcPort.Init(app::PortList_e::PMC_PORT, mPmcActiveObject);
        mPmcStateMachine.Init();
    }

    void PmcSubSystem::DispatchEvent(uint32_t eventId, uint32_t optDataAddress)
    {
        mPmcStateMachine.DispatchEvent(eventId, optDataAddress);
    }
    
}