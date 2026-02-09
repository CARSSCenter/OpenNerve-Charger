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
#include "svc_ble_subsystem.h"
#include "svc_wpt_subsystem.h"

namespace app
{
    struct StatePointers;

    void StateSlowChargeAndScan::Entry()
    {
        svc::BleSubsystem &mBleSubsystem = svc::BleSubsystem::Instance();
        mBleSubsystem.mBlePort.SendEvent(svc::BlePort::Event_e::START_SCANNING, NULL);

        svc::WptSubsystem &mWptSubsystem = svc::WptSubsystem::Instance();
        mWptSubsystem.mWptPort.SendEvent(svc::WptPort::Event_e::WPT_SLOW_CHARGE, NULL);

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
            stateMachine->ChangeState(states->pStateWait);
        }
        break;
        case SystemPort::Event_e::BLE_DEVICE_FOUND:
        {
            mWptManager.StartIpgTemperaturePgoodMonitoringTimer();
            stateMachine->ChangeState(states->pStateCharge);
        }
        break;
        case SystemPort::Event_e::WPT_SCAN_TIMEOUT:
        {
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
        
        svc::WptSubsystem &mWptSubsystem = svc::WptSubsystem::Instance();
        mWptSubsystem.mWptPort.SendEvent(svc::WptPort::Event_e::WPT_POWER_OFF, NULL);
        hal::Leds::GetInstance().LedChargingSlow(false);

        // Call WPT IC Service methods to stop slow charge
        // Call BLE methods to stop scanning
    }

}