/**
 * @name Hornet / WPT Charger
 * @file svc_wpt_state_charging.cpp
 * @brief Implementation file for the wpt charging state
 *
 * @copyright Copyright (c) 2024
 */

#include "svc_wpt_state_charging.h"

#include "eda_manager_log_config.h"
#include "svc_wpt_state_machine.h"
#include "svc_wpt_manager.h"
#include "svc_wpt_port.h"

namespace svc
{
    //==================================================================================================
    // Charge State implementation
    //==================================================================================================

    struct StatePointers;   

    void StateCharging::Entry()
    {
        LOG_DEBUG("WPT State Machine: Charging Entry\n");
        mWptManager.EnableWpt();
    }

    void StateCharging::DispatchEvent(uint32_t eventId, uint32_t optDataAddress)
    {        
        WptStateMachine *stateMachine = reinterpret_cast<WptStateMachine *>(mStateMachine);
        
        LOG_DEBUG("WPT State Machine:StateCharging DispatchEvent\n");
        
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
            /** @TODO: Handle if the wpt scan needs a timeout and the possible scenarios */
            LOG_DEBUG("WPT State Machine: WPT Start Scan\n");
            break;
        }
        case WptPort::Event_e::WPT_STOP_SCAN:
        {
            mWptManager.StopWptScan();
            WptPort::SendEvent(WptPort::Event_e::WPT_FAULT_CONDITION, NULL);
            break;
        }
        case WptPort::Event_e::WPT_POWER_OFF:
        {
            mWptManager.DisableWpt();
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
        case WptPort::Event_e::WPT_BATTERY_CHARGING:
        {
            LOG_DEBUG("WPT State Machine: Battery Charging\n");
            break;
        }
        case WptPort::Event_e::WPT_SLOW_CHARGE:
        {
            // Clamp PTH1/PTH2 to minimum power (400mV, step 0) during slow charge mode.
            // The LTC4125 continues its internal search cycles but its weak drive (~5-40uA)
            // is overridden by the low-impedance DAC, holding pulse width at minimum.
            LOG_DEBUG("WPT State Machine Charging state: WPT Slow Charge - setting minimum power\n");
            mWptManager.AdjustWptPowerTransfer(0);
            break;
        }
        case WptPort::Event_e::WPT_BATTERY_CHARGED:
        {
            // Do NOT send WPT_POWER_OFF here. Battery-full is handled by the application
            // layer: ProcessNewBleData (CHG1==1) sends BATTERY_CHARGED to the system port,
            // which causes StateCharge::Exit() -> WPT_POWER_OFF through the proper path.
            // Sending WPT_POWER_OFF directly from the WPT SM races with the white->yellow
            // transition: multiple DEVICE_FOUND callbacks can fire while STOP_SCANNING is
            // still queued, causing a second WPT_BATTERY_CHARGED to disable WPT after
            // StateCharge::Entry() has already re-enabled it via WPT_POWER_ON.
            LOG_DEBUG("WPT State Machine: Battery Charged (WPT shutdown via app layer)\n");
            break;
        }
        case WptPort::Event_e::WPT_SCAN_TIMEOUT:
        {
            LOG_DEBUG("WPT State Machine SlowCharge state: WPT Scan Timeout\n");
            WptPort::SendEvent(WptPort::Event_e::WPT_POWER_OFF, NULL);
            break;
        }
        case WptPort::Event_e::WPT_ADJUST_POWER:
        {
            LOG_DEBUG("WPT State Machine SlowCharge state: WPT Adjust Power\n");
            mWptManager.AdjustWptPowerTransfer(static_cast<uint8_t>(optDataAddress));
            break;
        }
        default:
        {
            break;
        }
        }
    }

    void StateCharging::Exit()
    {
        LOG_DEBUG("WPT State Machine: Charging Exit\n");
    }
}
