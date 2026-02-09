/**
 * @name Hornet / WPT Charger
 * @file state_scan.cpp
 * @brief Implementation file for the scan state
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "state_scan.h"

#include "app_port.h"
#include "app_state_machine.h"
#include "hal_dfu.h"
#include "hal_led.h"
#include "svc_ble_subsystem.h"
#include "svc_wpt_subsystem.h"

namespace app
{
    struct StatePointers;

    void StateScan::Entry()
    {
        svc::BleSubsystem &mBleSubsystem = svc::BleSubsystem::Instance();
        mBleSubsystem.mBlePort.SendEvent(svc::BlePort::Event_e::START_SCANNING, NULL);
        hal::Leds::GetInstance().LedScanOn(true);

    }

    void StateScan::DispatchEvent(uint32_t eventId, uint32_t optDataAddress)
    {
        SystemStateMachine *stateMachine = reinterpret_cast<SystemStateMachine *>(mStateMachine);

        StatePointers *states = stateMachine->GetStates();
        LOG_INFO("Received System event %d in scan state", eventId);
        switch (static_cast<SystemPort::Event_e>(eventId))
        {
        case SystemPort::Event_e::BLE_SCAN_TIMEOUT:
            mWptManager.StopIpgTemperaturePgoodMonitoringTimer();

            stateMachine->ChangeState(states->pStateSlowChargeAndScan);
            break;
        case SystemPort::Event_e::BLE_DEVICE_FOUND:
            mWptManager.StartIpgTemperaturePgoodMonitoringTimer();

            stateMachine->ChangeState(states->pStateCharge);
            break;
        case SystemPort::Event_e::BUTTON_DFU_PRESSED:
            if (!hal::Dfu::Instance().is_dfu_active())
            {
                hal::Dfu::Instance().start_dfu_mode();
            }
            break;
        }
    }

    void StateScan::Exit()
    {
        svc::WptSubsystem &mWptSubsystem = svc::WptSubsystem::Instance();
        mWptSubsystem.mWptPort.SendEvent(svc::WptPort::Event_e::WPT_POWER_ON, NULL);
        hal::Leds::GetInstance().LedScanOn(false);
    }
}