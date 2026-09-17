/**
 * @name Hornet / WPT Charger
 * @file app_state_machine.cpp
 * @brief Implementation file for the application state machine
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "app_state_machine.h"

#include "app_system.h"
#include "hal_button.h"
#include "hal_gpio.h"
#include "hal_pinout.h"
#include "hal_led.h"
#include "svc_ble_subsystem.h"
#include "svc_wpt_subsystem.h"
#include "svc_ble_port.h"
#include "svc_wpt_port.h"

#include <cstdint>

namespace app
{
    SystemStateMachine::SystemStateMachine() : StateMachine("App", &mInitialState), mInitialState(this), mStateCharge(this), mStateScan(this), mStateWait(this), mStateSlowChargeAndScan(this)
#if WPT_MANUAL_DEBUG_MODE
        , mStateManual(this)
#endif
    {
    }

    void SystemStateMachine::InitAction()
    {
        SetBleServiceCallbacks();
        SetWptServiceCallbacks();
        hal::Button::Init();
        static hal::Button onOffButton(PIN_BUTTON1, &OnOffButtonCallback, nullptr);
        static hal::Button DfuButton(PIN_BUTTON3, &DfuButtonCallback, nullptr);
        static hal::Button resetButton(PIN_BUTTON2, &ResetButtonCallback, nullptr);

        hal::Leds::Initialize();
        hal::Leds& leds = hal::Leds::GetInstance();
        leds.TurnLedOff(&leds.rgb_led);
   }

    StatePointers *SystemStateMachine::GetStates()
    {
        static StatePointers states;
        states.pInitialState = &mInitialState;
        states.pStateCharge = &mStateCharge;
        states.pStateScan = &mStateScan;
        states.pStateWait = &mStateWait;
        states.pStateSlowChargeAndScan = &mStateSlowChargeAndScan;
#if WPT_MANUAL_DEBUG_MODE
        states.pStateManual = &mStateManual;
#endif
        return &states;
    }

    void SystemStateMachine::ProcessNewBleData(uint32_t optDataAddress)
    {
        System *pSystem = &System::GetInstance();

        // Use the getter to get new data
        const svc::AdvertisementData_t &advData = svc::BleManager::GetAdvertisementData();
        svc::ChargingStatusParameters_t ChargingStatusParameters = advData.chargingStatusParameters;

        LOG_INFO("StateMachine: New IPG data received");

        LOG_INFO("Charging Parameters:");
        // Raw pin levels as the IPG sent them. Every name ending in "n" is
        // active-low: 1 = OK/absent, 0 = asserted.
        LOG_INFO("  VRECT_DETn: %d (0 = coil present)", ChargingStatusParameters.GET_VRECT_DET);
        LOG_INFO("  VRECT_OVPn: %d (1 = OK, 0 = OVP)", ChargingStatusParameters.GET_VRECT_OVP);
        LOG_INFO("  Vchg rail supply circuit power good: %d",
                 ChargingStatusParameters.GET_VCHG_RAIL_SUPPLY_CIRCUIT_POWER_GOOD);
        LOG_INFO("  CHG1 status: %d, CHG1_OVP_ERRn: %d (1 = OK, 0 = OVP)",
                 ChargingStatusParameters.GET_CHG1_STATUS, ChargingStatusParameters.GET_CHG1_OVP_ERR);
        LOG_INFO("  CHG2 status: %d, CHG2_OVP_ERRn: %d (1 = OK, 0 = OVP)",
                 ChargingStatusParameters.GET_CHG2_STATUS, ChargingStatusParameters.GET_CHG2_OVP_ERR);

        LOG_INFO("Thermal Parameters:");
        LOG_INFO("  Therm reference: %d", ChargingStatusParameters.GET_THERM_REF);
        LOG_INFO("  Therm ouput: %d", ChargingStatusParameters.GET_THERM_OUT);
        LOG_INFO("  Therm offset: %d", ChargingStatusParameters.GET_THERM_OFST);

        // BattA in the lower 16 bits, BattB in the upper 16 bits (see ParseManufacturerSpecificData).
        const uint16_t battA_mV = static_cast<uint16_t>(ChargingStatusParameters.BATTERY_VOLTAGE_MEASURED & 0xFFFF);
        const uint16_t battB_mV = static_cast<uint16_t>(ChargingStatusParameters.BATTERY_VOLTAGE_MEASURED >> 16);
        LOG_INFO("Battery voltage measured: BattA=%u mV BattB=%u mV", battA_mV, battB_mV);

        LOG_INFO("Test Information:");
        LOG_INFO("  HV Supply Enable: %d", ChargingStatusParameters.GET_TEST_INFO & 0xFF);
        LOG_INFO("  VDDS Supply Enable: %d", (ChargingStatusParameters.GET_TEST_INFO & 0xFF00) >> 8);
        LOG_INFO("  VDDA Supply Enable: %d", (ChargingStatusParameters.GET_TEST_INFO & 0xFF0000) >> 16);

        // Determine charging completion robustly across one or two batteries.
        //
        // CHG1_STATUS and CHG2_STATUS each report 0=actively charging, 1=idle/full.
        // The idle/full value (1) is not proof of a full battery: it also appears when
        // the charger IC has no input power yet, and with a deeply discharged battery
        // (seen at BattB = 2.5 V with PGOOD = 1). CHG1 reads 1 in every state seen on
        // this hardware.
        //
        // So a battery is only "done" when its measured voltage agrees. A channel reading
        // below BATTERY_PRESENT_MV is absent or not yet measured and is left out (an
        // unpopulated BattA reads 0-200 mV); if no channel has a reading at all, nothing can be concluded and charging continues (the LTC4065
        // terminates on its own and the IPG has its own thermal protection).
        //
        // Rules:
        //   - PGOOD = 0                        → charger ICs not powered; keep charging.
        //   - Any measured channel with CHG = 0
        //     or mV < BATTERY_FULL_MV          → still charging.
        //   - No measured channel              → unknown; keep charging.
        //   - Otherwise                        → all charging is complete.
        //
        // BATTERY_CHARGED is only acted on in StateCharge, whose Exit() stops scanning.
        // Scanning must not be stopped from here: in StateSlowChargeAndScan / StateScan
        // the event is ignored, and a stopped scan would leave WPT running with no
        // telemetry and no scan timeout to restart it.

        static constexpr uint16_t BATTERY_FULL_MV    = 4100; // LTC4065 float is 4.2 V; ad resolution 100 mV
        static constexpr uint16_t BATTERY_PRESENT_MV = 1000; // Below this the channel has no battery

        const bool pgood = (ChargingStatusParameters.GET_VCHG_RAIL_SUPPLY_CIRCUIT_POWER_GOOD == 1);

        const bool battA_measured = (battA_mV >= BATTERY_PRESENT_MV);
        const bool battB_measured = (battB_mV >= BATTERY_PRESENT_MV);
        const bool battA_done = (ChargingStatusParameters.GET_CHG1_STATUS == 1) && (battA_mV >= BATTERY_FULL_MV);
        const bool battB_done = (ChargingStatusParameters.GET_CHG2_STATUS == 1) && (battB_mV >= BATTERY_FULL_MV);

        const bool all_done = pgood
                              && (battA_measured || battB_measured)
                              && (!battA_measured || battA_done)
                              && (!battB_measured || battB_done);

        if (all_done)
        {
            // All measured batteries are full and charging supply confirmed good.
            LOG_INFO("StateMachine: All batteries charged (CHG1=%d CHG2=%d PGOOD=%d BattA=%u BattB=%u mV)",
                     ChargingStatusParameters.GET_CHG1_STATUS,
                     ChargingStatusParameters.GET_CHG2_STATUS,
                     pgood,
                     battA_mV,
                     battB_mV);
            svc::WptSubsystem &pWptSubsystem = svc::WptSubsystem::Instance();
            pWptSubsystem.mWptPort.SendEvent(svc::WptPort::Event_e::WPT_BATTERY_CHARGED, optDataAddress);

            pSystem->mSystemPort.SendEvent(SystemPort::Event_e::BATTERY_CHARGED, optDataAddress);
        }
        else
        {
            // At least one battery is still charging, or charger ICs not yet powered up.
            LOG_INFO("StateMachine: Charging in progress (CHG1=%d CHG2=%d PGOOD=%d BattA=%u BattB=%u mV)",
                     ChargingStatusParameters.GET_CHG1_STATUS,
                     ChargingStatusParameters.GET_CHG2_STATUS,
                     pgood,
                     battA_mV,
                     battB_mV);
            svc::WptSubsystem &pWptSubsystem = svc::WptSubsystem::Instance();
            pWptSubsystem.mWptPort.SendEvent(svc::WptPort::Event_e::WPT_BATTERY_CHARGING, optDataAddress);
        }
    }

    void SystemStateMachine::SetBleServiceCallbacks()
    {
        svc::BleSubsystem &pBleSubsystem = svc::BleSubsystem::Instance();

        uint32_t eventID = static_cast<uint32_t>(svc::BlePort::Event_e::BLE_INITIALIZED);
        pBleSubsystem.mBlePort.SetEventCallback(eventID, BleInitializedCallback);

        eventID = static_cast<uint32_t>(svc::BlePort::Event_e::DEVICE_FOUND);
        pBleSubsystem.mBlePort.SetEventCallback(eventID, DeviceFoundCallback);

        eventID = static_cast<uint32_t>(svc::BlePort::Event_e::SCAN_TIMEOUT);
        pBleSubsystem.mBlePort.SetEventCallback(eventID, ScanTimeoutCallback);
    }

    void SystemStateMachine::SetWptServiceCallbacks()
    {
        svc::WptSubsystem &pWptSubsystem = svc::WptSubsystem::Instance();

        uint32_t eventID = static_cast<uint32_t>(svc::WptPort::Event_e::WPT_CHARGE);
        pWptSubsystem.mWptPort.SetEventCallback(eventID, WptChargingCallback);

        eventID = static_cast<uint32_t>(svc::WptPort::Event_e::WPT_SCAN_TIMEOUT);
        pWptSubsystem.mWptPort.SetEventCallback(eventID, WptScanTimeoutCallback);
    }

    // BLE Service Callbacks

    void SystemStateMachine::BleInitializedCallback(uint32_t optDataAddress)
    {
        System *pSystem = &System::GetInstance();
        pSystem->mSystemPort.SendEvent(SystemPort::Event_e::BLE_INITIALIZED, optDataAddress);
    }

    void SystemStateMachine::DeviceFoundCallback(uint32_t optDataAddress)
    {
        System *pSystem = &System::GetInstance();
        ProcessNewBleData(optDataAddress);
        pSystem->mSystemPort.SendEvent(SystemPort::Event_e::BLE_DEVICE_FOUND, optDataAddress);
    }

    void SystemStateMachine::ScanTimeoutCallback(uint32_t optDataAddress)
    {
        System *pSystem = &System::GetInstance();
        pSystem->mSystemPort.SendEvent(SystemPort::Event_e::BLE_SCAN_TIMEOUT, optDataAddress);
    }

    // WPT Service Callbacks

    void SystemStateMachine::WptChargingCallback(uint32_t optDataAddress)
    {
        System *pSystem = &System::GetInstance();
        pSystem->mSystemPort.SendEvent(SystemPort::Event_e::WPT_CHARGING, optDataAddress);
    }

    void SystemStateMachine::WptScanTimeoutCallback(uint32_t optDataAddress)
    {
        System *pSystem = &System::GetInstance();
        pSystem->mSystemPort.SendEvent(SystemPort::Event_e::WPT_SCAN_TIMEOUT, optDataAddress);
    }

    // On/Off Button Callback

    void SystemStateMachine::OnOffButtonCallback()
    {
        System *pSystem = &System::GetInstance();
        pSystem->mSystemPort.SendEventFromISR(SystemPort::Event_e::BUTTON_PRESSED, NULL);
    }

    // Dfu Button Callback

    void SystemStateMachine::DfuButtonCallback()
    {
        System *pSystem = &System::GetInstance();
        pSystem->mSystemPort.SendEventFromISR(SystemPort::Event_e::BUTTON_DFU_PRESSED, NULL);
    }

    // Reset Button Callback — called from GPIOTE ISR, does not depend on FreeRTOS

    void SystemStateMachine::ResetButtonCallback()
    {
        // Disable active hardware before GPIO pins go high-Z on reset.
        // hal::Gpio::Write wraps nrf_gpio_pin_write, which is safe from ISR context.
        hal::Gpio::Write(PIN_WPT_EN, 1);     // Disable LTC4125 (active LOW)
        hal::Gpio::Write(PIN_PMC_VCC_EN, 0); // Disable VCC regulator (active HIGH)
        NVIC_SystemReset();
    }

}