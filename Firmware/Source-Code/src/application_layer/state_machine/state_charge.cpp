/**
 * @name Hornet / WPT Charger
 * @file state_charge.cpp
 * @brief Implementation file for the charge state
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "state_charge.h"

#include "app_port.h"
#include "app_state_machine.h"
#include "eda_manager_log_config.h"
#include "hal_dfu.h"
#include "hal_led.h"
#include "svc_ble_port.h"
#include "svc_ble_subsystem.h"
#include "svc_pmc_port.h"
#include "svc_pmc_subsystem.h"
#include "svc_wpt_port.h"
#include "svc_wpt_subsystem.h"

namespace app
{
    struct StatePointers;

    void StateCharge::Entry()
    {
        svc::PmcSubSystem& mPmcSubSystem = svc::PmcSubSystem::Instance();
        mPmcSubSystem.mPmcPort.SendEvent(svc::PmcPort::Event_e::PMC_POWER_ON, NULL);
        // Call WPT IC Service methods to start charge
        LOG_INFO("Charge State: PMC power on\n");
        hal::Leds::GetInstance().LedCharging(true);
    }

    void StateCharge::DispatchEvent(uint32_t eventId, uint32_t optDataAddress)
    {
        SystemStateMachine *stateMachine = reinterpret_cast<SystemStateMachine *>(mStateMachine);

        StatePointers *states = stateMachine->GetStates();

        switch (static_cast<SystemPort::Event_e>(eventId))
        {
        case SystemPort::Event_e::BUTTON_PRESSED:
            stateMachine->ChangeState(states->pStateWait);
            break;
        case SystemPort::Event_e::BATTERY_CHARGED:
            hal::Leds::GetInstance().LedCharged(true);
            stateMachine->ChangeState(states->pStateWait);
            break;
        case SystemPort::Event_e::TURN_OFF:
            stateMachine->ChangeState(states->pStateWait);
            break;
        case SystemPort::Event_e::BLE_SCAN_TIMEOUT:
            stateMachine->ChangeState(states->pStateWait);
            break;
        case SystemPort::Event_e::WPT_SCAN_TIMEOUT:
            stateMachine->ChangeState(states->pStateWait);
            break;
        case SystemPort::Event_e::BUTTON_DFU_PRESSED:
            if (!hal::Dfu::Instance().is_dfu_active())
            {
                hal::Dfu::Instance().start_dfu_mode();
            }
            break;
        }
    }

    void StateCharge::Exit()
    {
        svc::BleSubsystem &mBleSubsystem = svc::BleSubsystem::Instance();
        mBleSubsystem.mBlePort.SendEvent(svc::BlePort::Event_e::STOP_SCANNING, NULL);

        svc::WptSubsystem &mWptSubsystem = svc::WptSubsystem::Instance();
        mWptSubsystem.mWptPort.SendEvent(svc::WptPort::Event_e::WPT_POWER_OFF, NULL);
        hal::Leds::GetInstance().LedCharging(false);
    }
} // namespace app
