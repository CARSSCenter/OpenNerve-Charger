/**
 * @name Hornet / WPT Charger
 * @file svc_wpt_state_slow_charge.CPP
 * @brief Implementation file for the wpt slow charge state
 *
 *@copyright Copyright (c) 2024
 */

#include "svc_wpt_state_slow_charge.h"

#include "eda_manager_log_config.h"
#include "svc_wpt_state_machine.h"
#include "svc_wpt_port.h"

namespace svc
{
    //==================================================================================================
    // Slow Charge State implementation
    //==================================================================================================
    struct StatePointers;

    void StateSlowCharge::Entry()
    {
        LOG_DEBUG("WPT State Machine: Slow Charge\n");
        mWptManager.EnableWpt();
    }

    void StateSlowCharge::DispatchEvent(uint32_t eventId, uint32_t optDataAddress)
    {
        WptStateMachine *stateMachine = reinterpret_cast<WptStateMachine *>(mStateMachine);
        
        LOG_DEBUG("WPT State Machine: Slow Charge\n");
        
        StatePointers *states = stateMachine->GetStates();

        switch (static_cast<WptPort::Event_e>(eventId))
        {
        case WptPort::Event_e::INVALID:
        {
            LOG_DEBUG("WPT State Machine SlowCharge state: Invalid Event\n");
            break;
        }
        case WptPort::Event_e::PING_PORT:
        {
            LOG_DEBUG("WPT State Machine SlowCharge state: Ping Port\n");
            break;
        }
        case WptPort::Event_e::WPT_POWER_ON:
        {
            LOG_DEBUG("WPT State Machine SlowCharge state: WPT Power On\n");
            stateMachine->ChangeState(states->pStateCharging);
            break;
        }
        case WptPort::Event_e::WPT_LOAD_DETECTED:
        {
            LOG_DEBUG("WPT State Machine SlowCharge state: WPT Scan\n");
            stateMachine->ChangeState(states->pStateCharging);
            break;
        }
        case WptPort::Event_e::WPT_STOP_SCAN:
        {
            LOG_DEBUG("WPT State Machine SlowCharge state: WPT Stop Scan\n");
            break;
        }
        case WptPort::Event_e::WPT_POWER_OFF:
        {
            mWptManager.DisableWpt();
            stateMachine->ChangeState(states->pStateIdle);
            break;
        }
        case WptPort::Event_e::WPT_SCAN_TIMEOUT:
        {
            LOG_DEBUG("WPT State Machine SlowCharge state: WPT Scan Timeout\n");
            mWptManager.StopIpgTemperaturePgoodMonitoringTimer();
            stateMachine->ChangeState(states->pStateIdle);
            break;
        }
        case WptPort::Event_e::WPT_FAULT_CONDITION:
        {
            /** @TODO: Handle the WPT fault conditions */
            LOG_DEBUG("WPT State Machine SlowCharge state: WPT Fault Condition\n");
            WptPort::SendEvent(WptPort::Event_e::WPT_POWER_OFF, NULL);
            break;
        }
        default:
        {
            //ASSERT(false);
            break;
        }
        }
    }

    void StateSlowCharge::Exit()
    {
        LOG_DEBUG("WPT State Machine: Slow Charge \n");
    }

}