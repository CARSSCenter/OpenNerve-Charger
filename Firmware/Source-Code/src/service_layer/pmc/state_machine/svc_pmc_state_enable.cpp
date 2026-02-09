/**
 * @name Hornet / WPT Charger
 * @file svc_pmc_state_enable.cpp
 * @brief Implementation file for the pmc enable state
 *
 * @copyright Copyright (c) 2024
 */

#include "svc_pmc_state_enable.h"

#include "eda_manager_log_config.h"

#include "svc_pmc_port.h"
#include "svc_pmc_state_machine.h"

namespace svc
{
    //==================================================================================================
    // Charge State implementation
    //==================================================================================================

    struct PmcStatePointers;   

    void PmcStateEnable::Entry()
    {
        mPmcManager.EnableVccRegulator();
        LOG_DEBUG("PMC State Machine: Enable Entry\n");
    }

    void PmcStateEnable::DispatchEvent(uint32_t eventId, uint32_t optDataAddress)
    {        
        PmcStateMachine *stateMachine = reinterpret_cast<PmcStateMachine *>(mStateMachine);
        
        LOG_DEBUG("PMC State Machine:StateEnable DispatchEvent\n");
        
        PmcStatePointers *states = stateMachine->GetStates();

        switch (static_cast<PmcPort::Event_e>(eventId))
        {
        case PmcPort::Event_e::INVALID:
        {
            LOG_DEBUG("PMC State Machine: Invalid Event\n");
            break;
        }
        case PmcPort::Event_e::PING_PORT:
        {
            LOG_DEBUG("PMC State Machine: Ping Port\n");
            break;
        }
        case PmcPort::Event_e::PMC_POWER_ON:
        {
            break;
        }
        case PmcPort::Event_e::PMC_POWER_OFF:
        {
            stateMachine->ChangeState(states->pStateIdle);
            break;
        }
        case PmcPort::Event_e::PMC_START_CHARGING:
        {
            stateMachine->ChangeState(states->pStateChargingBattery);
            break;
        }        
        case PmcPort::Event_e::PMC_FAULT_CONDITION:
        {
            /** @TODO: Handle the PMC fault conditions */
            LOG_DEBUG("PMC State Machine Enable state: PMC Fault Condition\n");
            PmcPort::SendEvent(PmcPort::Event_e::PMC_POWER_OFF, NULL);
            break;
        }
        default:
        {
            ASSERT(false);
            break;
        }
        }
    }

    void PmcStateEnable::Exit()
    {
        mPmcManager.DisableVccRegulator();
        LOG_DEBUG("PMC State Machine: Enable Exit\n");
    }
}
