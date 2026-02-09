/**
 * @name Hornet / WPT Charger
 * @file svc_pmc_state_charging_battery.cpp
 * @brief Implementation file for the pmc charging battery state
 *
 * @copyright Copyright (c) 2024
 */

#include "svc_pmc_state_charging_battery.h"

#include "eda_manager_log_config.h"

#include "svc_pmc_port.h"
#include "svc_pmc_state_machine.h"

namespace svc
{
    //==================================================================================================
    // Charge State implementation
    //==================================================================================================

    struct PmcStatePointers;

    void PmcStateChargingBattery::Entry()
    {
        mPmcManager.EnableBatteryCharger();
        LOG_DEBUG("PMC State Machine: Charging Battery Entry\n");
    }

    void PmcStateChargingBattery::DispatchEvent(uint32_t eventId, uint32_t optDataAddress)
    {
        PmcStateMachine *stateMachine = reinterpret_cast<PmcStateMachine *>(mStateMachine);

        LOG_DEBUG("PMC State Machine:State Charging Battery DispatchEvent\n");

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
            stateMachine->ChangeState(states->pStateEnable);
            break;
        }
        case PmcPort::Event_e::PMC_START_CHARGING:
        {
            LOG_DEBUG("PMC State Machine Charging Battery state: Battery Start Charging\n");
            break;
        }
        case PmcPort::Event_e::PMC_BATTERY_CHARGED:        
        {
            LOG_DEBUG("PMC State Machine Charging Battery state: Battery Full Charged\n");
            stateMachine->ChangeState(states->pStateIdle);
            break;
        }        
        case PmcPort::Event_e::PMC_BATTERY_CHARGING:        
        {
            LOG_DEBUG("PMC State Machine Charging Battery state: Battery Charging\n");
            break;
        }
        case PmcPort::Event_e::PMC_READ_CHARGER_PRESENT_INDICATOR:        
        {
            LOG_DEBUG("PMC State Machine Charging Battery state: Battery Present Indicator\n");
            break;
        }
        case PmcPort::Event_e::PMC_FAULT_CONDITION:
        {
            /** @TODO: Handle the PMC fault conditions */
            LOG_DEBUG("PMC State Machine Charging Battery state: PMC Fault Condition\n");
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

    void PmcStateChargingBattery::Exit()
    {
        mPmcManager.DisableBatteryCharger();
        LOG_DEBUG("PMC State Machine: Charging Battery Exit\n");
    }
}
