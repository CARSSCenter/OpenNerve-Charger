/**
 * @name Hornet / WPT Charger
 * @file state_slow_charge_and_scan.cpp
 * @brief Implementation file for the slow charge and scan state
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "state_slow_charge_and_scan.h"

#include "app_port.h"
#include "app_state_machine.h"
#include "hal_dfu.h"
#include "hal_led.h"
#include "svc_ble_manager.h"
#include "svc_ble_subsystem.h"
#include "svc_pmc_port.h"
#include "svc_pmc_subsystem.h"
#include "svc_wpt_subsystem.h"

namespace app
{
    struct StatePointers;

    void StateSlowChargeAndScan::Entry()
    {
        // Use a longer scan timeout so the IPG has time to power up, initialize,
        // and start advertising after the wireless power wakes it from off state.
        svc::BleManager::SetScanTimeout(SCAN_TIMEOUT_SLOW_CHARGE_MS);

        svc::BleSubsystem &mBleSubsystem = svc::BleSubsystem::Instance();
        mBleSubsystem.mBlePort.SendEvent(svc::BlePort::Event_e::START_SCANNING, NULL);

        svc::WptSubsystem &mWptSubsystem = svc::WptSubsystem::Instance();
        mWptSubsystem.mWptPort.SendEvent(svc::WptPort::Event_e::WPT_SLOW_CHARGE, NULL);

        svc::PmcSubSystem &mPmcSubSystem = svc::PmcSubSystem::Instance();
        mPmcSubSystem.mPmcPort.SendEvent(svc::PmcPort::Event_e::PMC_POWER_ON, NULL);

        hal::Leds::GetInstance().LedChargingSlow(true);
    }

    void StateSlowChargeAndScan::DispatchEvent(uint32_t eventId, uint32_t optDataAddress)
    {
        SystemStateMachine *stateMachine = reinterpret_cast<SystemStateMachine *>(mStateMachine);

        StatePointers *states = stateMachine->GetStates();

        switch (static_cast<SystemPort::Event_e>(eventId))
        {
        case SystemPort::Event_e::BLE_SCAN_TIMEOUT:
        {
            svc::BleSubsystem &mBleSubsystem = svc::BleSubsystem::Instance();
            mBleSubsystem.mBlePort.SendEvent(svc::BlePort::Event_e::STOP_SCANNING, NULL);

            mWptManager.StopIpgTemperaturePgoodMonitoringTimer();
            svc::WptSubsystem &mWptSubsystemBle = svc::WptSubsystem::Instance();
            mWptSubsystemBle.mWptPort.SendEvent(svc::WptPort::Event_e::WPT_POWER_OFF, NULL);

            svc::PmcSubSystem &mPmcSubSystemBle = svc::PmcSubSystem::Instance();
            mPmcSubSystemBle.mPmcPort.SendEvent(svc::PmcPort::Event_e::PMC_POWER_OFF, NULL);

            stateMachine->ChangeState(states->pStateWait);
        }
        break;
        case SystemPort::Event_e::BLE_DEVICE_FOUND:
        {
            // WPT stays enabled — do not send WPT_POWER_OFF here.
            // StateCharge::Entry() will send WPT_POWER_ON to ensure the service
            // layer is active regardless of which path led here.
            mWptManager.StartIpgTemperaturePgoodMonitoringTimer();
            stateMachine->ChangeState(states->pStateCharge);
        }
        break;
        case SystemPort::Event_e::WPT_SCAN_TIMEOUT:
        {
            svc::WptSubsystem &mWptSubsystemWpt = svc::WptSubsystem::Instance();
            mWptSubsystemWpt.mWptPort.SendEvent(svc::WptPort::Event_e::WPT_POWER_OFF, NULL);

            svc::PmcSubSystem &mPmcSubSystemWpt = svc::PmcSubSystem::Instance();
            mPmcSubSystemWpt.mPmcPort.SendEvent(svc::PmcPort::Event_e::PMC_POWER_OFF, NULL);

            stateMachine->ChangeState(states->pStateWait);
        }
        break;
        case SystemPort::Event_e::BUTTON_DFU_PRESSED:
        {
            if (!hal::Dfu::Instance().is_dfu_active())
            {
                hal::Dfu::Instance().start_dfu_mode();
            }
        }
        break;
        }
    }

    void StateSlowChargeAndScan::Exit()
    {
        // WPT_POWER_OFF is sent explicitly in BLE_SCAN_TIMEOUT and WPT_SCAN_TIMEOUT
        // dispatch cases, not here. On BLE_DEVICE_FOUND the WPT stays enabled so
        // StateCharge can take over without an enable/disable/enable round-trip.

        // Restore the default scan timeout for all other states.
        svc::BleManager::SetScanTimeout(SCAN_TIMEOUT_MS);

        hal::Leds::GetInstance().LedChargingSlow(false);
    }

}