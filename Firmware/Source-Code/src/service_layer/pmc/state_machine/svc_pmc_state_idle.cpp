/**
 * @name Hornet / WPT Charger
 * @file svc_pmc_state_idle.cpp
 * @brief Implementation file for the pmc idle state
 *
 * @copyright Copyright (c) 2024
 */

#include "svc_pmc_state_idle.h"

#include "eda_manager_log_config.h"

#include "svc_pmc_port.h"
#include "svc_pmc_state_machine.h"

namespace svc
{
    //==================================================================================================
    // Idle State implementation
    //==================================================================================================
    struct PmcStatePointers;

    void PmcStateIdle::Entry()
    {
        LOG_DEBUG("PMC State Machine: Idle Entry\n");
    }

    void PmcStateIdle::DispatchEvent(uint32_t eventId, uint32_t optDataAddress)
    {
        PmcStateMachine *stateMachine = reinterpret_cast<PmcStateMachine *>(mStateMachine);
        
        LOG_DEBUG("PMC State Machine: Idle DispatchEvent\n");
        
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
        case PmcPort::Event_e::INITIALIZE:
        {
            mPmcManager.Init();
            LOG_DEBUG("PMC State Machine: Initialize\n");
            break;
        }
        case PmcPort::Event_e::PMC_POWER_ON:
        {
            stateMachine->ChangeState(states->pStateEnable);
            break;
        }
        case PmcPort::Event_e::PMC_START_CHARGING:
        {
            stateMachine->ChangeState(states->pStateChargingBattery);
            break;
        } 
        case PmcPort::Event_e::PMC_POWER_OFF:
        {
            break;
        }
        case PmcPort::Event_e::PMC_BATTERY_CHARGING:
        {
            break;
        }
        case PmcPort::Event_e::PMC_READ_FAST_CHARGER_INDICATOR:
        {
            break;
        }        
        case PmcPort::Event_e::PMC_READ_CHARGER_PRESENT_INDICATOR:
        {
            break;
        }
        default:
        {
            ASSERT(false);
            break;
        }
        }
    }

    void PmcStateIdle::Exit()
    {
        LOG_DEBUG("PMC State Machine: Idle Exit\n");
    }
} // namespace svc