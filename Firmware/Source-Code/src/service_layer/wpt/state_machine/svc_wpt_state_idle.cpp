/**
 * @name Hornet / WPT Charger
 * @file svc_wpt_state_idle.cpp
 * @brief Implementation file for the wpt idle state
 *
 * @copyright Copyright (c) 2024
 */

#include "svc_wpt_state_idle.h"

#include "eda_manager_log_config.h"
#include "svc_wpt_state_machine.h"
#include "svc_wpt_port.h"

namespace svc
{
    //==================================================================================================
    // Idle State implementation
    //==================================================================================================
    struct StatePointers;

    void StateIdle::Entry()
    {
        LOG_DEBUG("WPT State Machine: Idle Entry\n");
    }

    void StateIdle::DispatchEvent(uint32_t eventId, uint32_t optDataAddress)
    {
        WptStateMachine *stateMachine = reinterpret_cast<WptStateMachine *>(mStateMachine);
        
        LOG_DEBUG("WPT State Machine: Idle DispatchEvent\n");
        
        StatePointers *states = stateMachine->GetStates();

        switch (static_cast<WptPort::Event_e>(eventId))
        {
        case WptPort::Event_e::INVALID:
        {
            LOG_DEBUG("WPT State Machine: Invalid Event\n");
            break;
        }
        case WptPort::Event_e::PING_PORT:
        {
            LOG_DEBUG("WPT State Machine: Ping Port\n");
            break;
        }
        case WptPort::Event_e::INITIALIZE:
        {
            LOG_DEBUG("WPT State Machine: Initialize\n");
            mWptManager.Init();
            break;
        }
        case WptPort::Event_e::WPT_POWER_ON:
        {
            stateMachine->ChangeState(states->pStateCharging);
            break;
        }
        case WptPort::Event_e::WPT_SLOW_CHARGE:
        {
            stateMachine->ChangeState(states->pStateSlowCharge);
            break;
        }
        default:
        {
            break;
        }
        }
    }

    void StateIdle::Exit()
    {
        LOG_DEBUG("WPT State Machine: Idle Exit\n");
    }
} // namespace svc