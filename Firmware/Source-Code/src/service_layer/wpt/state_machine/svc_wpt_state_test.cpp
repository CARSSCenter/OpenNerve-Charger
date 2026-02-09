/**
 * @name Hornet / WPT Charger
 * @file svc_wpt_state_test.cpp
 * @brief Implementation file for the wpt test state
 *
 * @copyright Copyright (c) 2024
 */

#include "svc_wpt_state_test.h"

#include "eda_manager_log_config.h"
#include "svc_wpt_state_machine.h"
#include "svc_wpt_port.h"

namespace svc
{
 
    //==================================================================================================
    // Test state implementation
    //==================================================================================================

    struct StatePointers;

    void StateTest::Entry()
    {
        LOG_DEBUG("WPT State Machine: Test Entry\n");
    }

    void StateTest::DispatchEvent(uint32_t eventId, uint32_t optDataAddress)
    {
        WptStateMachine *stateMachine = reinterpret_cast<WptStateMachine *>(mStateMachine);
        
        LOG_DEBUG("WPT State Machine: Test DispatchEvent\n");
        
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
        case WptPort::Event_e::WPT_POWER_ON:
        {
            LOG_DEBUG("WPT State Machine: WPT Power On\n");
            break;
        }
        case WptPort::Event_e::WPT_LOAD_DETECTED:
        {
            LOG_DEBUG("WPT State Machine: WPT Start Scan\n");
            break;
        }
        case WptPort::Event_e::WPT_STOP_SCAN:
        {
            LOG_DEBUG("WPT State Machine: WPT Stop Scan\n");
            break;
        }
        case WptPort::Event_e::WPT_POWER_OFF:
        {
            LOG_DEBUG("WPT State Machine: WPT Power Off\n");
            break;
        }
        case WptPort::Event_e::WPT_FAULT_CONDITION:
        {
            LOG_DEBUG("WPT State Machine: WPT Fault Condition\n");
            break;
        }
        default:
        {
            break;
        }
        }
    }

    void StateTest::Exit()
    {
        LOG_DEBUG("WPT State Machine: Test Exit\n");
    }    
}
