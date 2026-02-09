/**
 * @name Hornet / WPT Charger
 * @file svc_pmc_subsystem.h
 * @brief PmcSubsystem class implementation
 * 
 * @copyright Copyright (c) 2024
 *
 */

#ifndef PMC_SUBSYSTEM_H
#define PMC_SUBSYSTEM_H

#include "eda_active_object.h"

#include "state_machine/svc_pmc_state_machine.h"
#include "svc_pmc_port.h"

namespace svc
{
    class PmcSubSystem
    {
    public:
        static PmcSubSystem &Instance();
        void Init();
        void DispatchEvent(uint32_t eventId, uint32_t optDataAddress);
        
        PmcPort mPmcPort;

    private:
        PmcSubSystem();

        eda::ActiveObject mPmcActiveObject;

        PmcStateMachine mPmcStateMachine;
    };
}
#endif // PMC_SUBSYSTEM_H