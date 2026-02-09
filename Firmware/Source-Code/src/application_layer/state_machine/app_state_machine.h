/**
 * @name Hornet / WPT Charger
 * @file app_state_machine.h
 * @brief Header file for the application state machine
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef APP_STATE_MACHINE_H
#define APP_STATE_MACHINE_H

#include "eda_state_machine.h"

#include "state_charge.h"
#include "state_scan.h"
#include "state_wait.h"
#include "state_slow_charge_and_scan.h"
#include "state_initialization.h"
#include "hal_button.h"

namespace app
{

    struct StatePointers
    {
        StateInitialization *pInitialState;
        StateCharge *pStateCharge;
        StateScan *pStateScan;
        StateWait *pStateWait;
        StateSlowChargeAndScan *pStateSlowChargeAndScan;
    };

    class SystemStateMachine : public eda::StateMachine
    {
    public:
        /// Constructor of the SystemStateMachine class.
        SystemStateMachine();

        /// Action to be executed when the state machine is initialized.
        void InitAction() override;

        /// Get the pointers to the state machine states.
        StatePointers *GetStates();

    private:
        StateInitialization mInitialState;
        StateCharge mStateCharge;
        StateScan mStateScan;
        StateWait mStateWait;
        StateSlowChargeAndScan mStateSlowChargeAndScan;

        /// Processes the new BLE data
        ///
        /// @param optDataAddress The optional data address
        static void ProcessNewBleData(uint32_t optDataAddress);

        /// Sets the BLE service callbacks
        static void SetBleServiceCallbacks(void);

        /// Sets the WPT service callbacks
        static void SetWptServiceCallbacks(void);

        /// Callback for the BLE initialization event
        ///
        /// @param optDataAddress The optional data address
        static void BleInitializedCallback(uint32_t optDataAddress);

        /// Callback for the BLE Device Found event
        ///
        /// @param optDataAddress The optional data address
        static void DeviceFoundCallback(uint32_t optDataAddress);

        /// Callback for the BLE Scan Timeout event
        ///
        /// @param optDataAddress The optional data address
        static void ScanTimeoutCallback(uint32_t optDataAddress);

        /// Callback for the WPT Charging event
        ///
        /// @param optDataAddress The optional data address
        static void WptChargingCallback(uint32_t optDataAddress);

        /// Callback for the WPT Scan Timeout event
        ///
        /// @param optDataAddress The optional data address
        static void WptScanTimeoutCallback(uint32_t optDataAddress);

        /// Callback for the Button Press event
        static void OnOffButtonCallback();

        /// Callback for the DFU Button Press event
        static void DfuButtonCallback();
    };
}

#endif // APP_STATE_MACHINE_H