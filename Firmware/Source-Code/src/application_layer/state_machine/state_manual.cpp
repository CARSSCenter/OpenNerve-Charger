/**
 * @name Hornet / WPT Charger
 * @file state_manual.cpp
 * @brief Implementation file for the manual (bench debug) state
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "state_manual.h"

#if WPT_MANUAL_DEBUG_MODE

#include "app_port.h"
#include "app_state_machine.h"
#include "eda_manager_log_config.h"
#include "hal_dfu.h"
#include "hal_led.h"
#include "svc_ble_port.h"
#include "svc_debug_console.h"
#include "svc_ble_subsystem.h"
#include "svc_pmc_port.h"
#include "svc_pmc_subsystem.h"
#include "svc_wpt_port.h"
#include "svc_wpt_subsystem.h"

namespace app
{
    struct StatePointers;

    void StateManual::Entry()
    {
        // The state owns the console's manual flag, so the two can never disagree.
        svc::DebugConsole::OnManualEntered();

        // VCC_EN. The LTC4125's 5 V supply is owned entirely by the PMC state
        // machine and is low at boot, so without this the console can drive
        // PIN_WPT_EN all it likes into an unpowered transmitter.
        svc::PmcSubSystem &mPmcSubSystem = svc::PmcSubSystem::Instance();
        mPmcSubSystem.mPmcPort.SendEvent(svc::PmcPort::Event_e::PMC_POWER_ON, NULL);

        // StateCharge::Entry() deliberately does not start scanning - it assumes a
        // predecessor state did. Manual mode can be entered straight from
        // StateWait, so it has to start its own, or there is no IPG telemetry and
        // the thermal/OVP warnings have nothing to report.
        svc::BleSubsystem &mBleSubsystem = svc::BleSubsystem::Instance();
        mBleSubsystem.mBlePort.SendEvent(svc::BlePort::Event_e::START_SCANNING, NULL);

        // Stop whatever the previous state left running and settle the WPT side in
        // StateIdle with fault monitoring only. This has to be an event rather than
        // a direct call: the previous state's Exit() may have queued WPT_POWER_ON
        // (StateScan) or WPT_POWER_OFF (StateCharge) that the WPT task, running at
        // lower priority, has not processed yet. Queued behind them, this runs last.
        svc::WptSubsystem &mWptSubsystem = svc::WptSubsystem::Instance();
        mWptSubsystem.mWptPort.SendEvent(svc::WptPort::Event_e::WPT_MANUAL_IDLE, NULL);

        hal::Leds &leds = hal::Leds::GetInstance();
        leds.SetLedColor(&leds.rgb_led, hal::LedPosition_e::LED1, hal::LedColor_e::MAGENTA);
        leds.TurnLedOn(&leds.rgb_led);

        // The coil stays off until the operator presses 's'.
        LOG_WARNING("Manual State: VCC rail ON, WPT idle, coil HELD OFF - press 's' to transmit\n");
    }

    void StateManual::DispatchEvent(uint32_t eventId, uint32_t optDataAddress)
    {
        SystemStateMachine *stateMachine = reinterpret_cast<SystemStateMachine *>(mStateMachine);

        StatePointers *states = stateMachine->GetStates();

        switch (static_cast<SystemPort::Event_e>(eventId))
        {
        case SystemPort::Event_e::TURN_OFF:
            // The console's 'n' command.
            LOG_WARNING("Manual State: manual mode ended, shutting down\n");
            stateMachine->ChangeState(states->pStateWait);
            break;

        case SystemPort::Event_e::BUTTON_PRESSED:
            // Button 1 is disabled in manual mode. The only ways out are the
            // console's 'n' and Button 2, which resets the MCU from its ISR.
            LOG_WARNING("Manual State: Button 1 ignored in manual mode - use 'n', or Button 2 to reset\n");
            break;

        case SystemPort::Event_e::BLE_SCAN_TIMEOUT:
            // Re-arm rather than leave the state. This is the same trick
            // StateCharge uses, and it is what keeps a manual session alive
            // indefinitely with no IPG present.
            {
                svc::BleSubsystem &mBleSubsystem = svc::BleSubsystem::Instance();
                mBleSubsystem.mBlePort.SendEvent(svc::BlePort::Event_e::START_SCANNING, NULL);
            }
            break;

        case SystemPort::Event_e::BATTERY_CHARGED:
            // ProcessNewBleData() has already sent STOP_SCANNING straight to the BLE
            // port, which also stops the scan watchdog - so no BLE_SCAN_TIMEOUT
            // would follow to re-arm it, and IPG telemetry would freeze for the rest
            // of the session. Low-PTH bench runs produce this reading routinely.
            {
                svc::BleSubsystem &mBleSubsystem = svc::BleSubsystem::Instance();
                mBleSubsystem.mBlePort.SendEvent(svc::BlePort::Event_e::START_SCANNING, NULL);
            }
            break;

        case SystemPort::Event_e::BUTTON_DFU_PRESSED:
            if (!hal::Dfu::Instance().is_dfu_active())
            {
                hal::Dfu::Instance().start_dfu_mode();
            }
            break;

            // WPT_SCAN_TIMEOUT and BLE_DEVICE_FOUND are absent on purpose: in manual
            // mode the operator decides when power starts and stops.
        }
    }

    void StateManual::Exit()
    {
        svc::DebugConsole::OnManualExited();

        // Stop the fault timer explicitly. WPT_POWER_OFF below only reaches
        // DisableWpt() when the WPT state machine is in a charging state; if the
        // operator never pressed 's' it is still in StateIdle and the event is
        // dropped, which would leave the timer running after the state is gone.
        mWptManager.StopIpgTemperaturePgoodMonitoringTimer();

        svc::WptSubsystem &mWptSubsystem = svc::WptSubsystem::Instance();
        mWptSubsystem.mWptPort.SendEvent(svc::WptPort::Event_e::WPT_POWER_OFF, NULL);

        svc::BleSubsystem &mBleSubsystem = svc::BleSubsystem::Instance();
        mBleSubsystem.mBlePort.SendEvent(svc::BlePort::Event_e::STOP_SCANNING, NULL);

        // Required. StateCharge::Exit() does not drop the rail, which is why
        // leaving that state the normal way parks VCC_EN high.
        svc::PmcSubSystem &mPmcSubSystem = svc::PmcSubSystem::Instance();
        mPmcSubSystem.mPmcPort.SendEvent(svc::PmcPort::Event_e::PMC_POWER_OFF, NULL);

        hal::Leds &leds = hal::Leds::GetInstance();
        leds.TurnLedOff(&leds.rgb_led);
    }
} // namespace app

#endif // WPT_MANUAL_DEBUG_MODE
