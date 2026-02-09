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
#include "hal_led.h"
#include "svc_ble_subsystem.h"
#include "svc_wpt_subsystem.h"
#include "svc_ble_port.h"
#include "svc_wpt_port.h"

#include <cstdint>

namespace app
{
    SystemStateMachine::SystemStateMachine() : StateMachine("App", &mInitialState), mInitialState(this), mStateCharge(this), mStateScan(this), mStateWait(this), mStateSlowChargeAndScan(this)
    {
    }

    void SystemStateMachine::InitAction()
    {
        SetBleServiceCallbacks();
        SetWptServiceCallbacks();
        hal::Button::Init();
        static hal::Button onOffButton(PIN_BUTTON1, &OnOffButtonCallback, nullptr);
        static hal::Button DfuButton(PIN_BUTTON3, &DfuButtonCallback, nullptr);

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
        LOG_INFO("  Vrect detection: %d", ChargingStatusParameters.GET_VRECT_DET);
        LOG_INFO("  Vrect OVP: %d", ChargingStatusParameters.GET_VRECT_OVP);
        LOG_INFO("  Vchg rail supply circuit power good: %d",
                 ChargingStatusParameters.GET_VCHG_RAIL_SUPPLY_CIRCUIT_POWER_GOOD);
        LOG_INFO("  CHG1 status: %d, OVP error: %d",
                 ChargingStatusParameters.GET_CHG1_STATUS, ChargingStatusParameters.GET_CHG1_OVP_ERR);
        LOG_INFO("  CHG2 status: %d, OVP error: %d",
                 ChargingStatusParameters.GET_CHG2_STATUS, ChargingStatusParameters.GET_CHG2_OVP_ERR);

        LOG_INFO("Thermal Parameters:");
        LOG_INFO("  Therm reference: %d", ChargingStatusParameters.GET_THERM_REF);
        LOG_INFO("  Therm ouput: %d", ChargingStatusParameters.GET_THERM_OUT);
        LOG_INFO("  Therm offset: %d", ChargingStatusParameters.GET_THERM_OFST);

        LOG_INFO("Battery voltage measured: %ld mV", ChargingStatusParameters.BATTERY_VOLTAGE_MEASURED);

        LOG_INFO("Test Information:");
        LOG_INFO("  HV Supply Enable: %d", ChargingStatusParameters.GET_TEST_INFO & 0xFF);
        LOG_INFO("  VDDS Supply Enable: %d", (ChargingStatusParameters.GET_TEST_INFO & 0xFF00) >> 8);
        LOG_INFO("  VDDA Supply Enable: %d", (ChargingStatusParameters.GET_TEST_INFO & 0xFF0000) >> 16);

        if (ChargingStatusParameters.GET_CHG1_STATUS == 0)
        {
            // IPG Battery Charging
            svc::WptSubsystem &pWptSubsystem = svc::WptSubsystem::Instance();
            pWptSubsystem.mWptPort.SendEvent(svc::WptPort::Event_e::WPT_BATTERY_CHARGING, optDataAddress);
        }
        else if (ChargingStatusParameters.GET_CHG1_STATUS == 1)
        {
            // IPG Battery Charged
            svc::WptSubsystem &pWptSubsystem = svc::WptSubsystem::Instance();
            pWptSubsystem.mWptPort.SendEvent(svc::WptPort::Event_e::WPT_BATTERY_CHARGED, optDataAddress);

            svc::BleSubsystem &pBleSubsystem = svc::BleSubsystem::Instance();
            pBleSubsystem.mBlePort.SendEvent(svc::BlePort::Event_e::STOP_SCANNING, optDataAddress);

            pSystem->mSystemPort.SendEvent(SystemPort::Event_e::BATTERY_CHARGED, optDataAddress);
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

}