/**
 * @name Hornet / WPT Charger
 * @file state_manual.h
 * @brief Header file for the manual (bench debug) state
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef STATE_MANUAL_H
#define STATE_MANUAL_H

#include "svc_debug_config.h"

#if WPT_MANUAL_DEBUG_MODE

#include "../../core_layer/event_driven_architecture/state_machine/eda_state_machine.h"
#include "svc_wpt_manager.h"

namespace app
{
    /// Bench-test state driven by the RTT debug console.
    ///
    /// Manual behaviour is expressed as what this state *does* rather than as
    /// exceptions carved out of the automatic states. The transitions that would
    /// end a charge session - BATTERY_CHARGED, WPT_SCAN_TIMEOUT, BLE_DEVICE_FOUND -
    /// simply have no case here, so no conditional is needed to suppress them and
    /// none can be forgotten when new automatic behaviour is added elsewhere.
    ///
    /// Entry overrides whatever the previous state was doing: any charge in
    /// progress is stopped and the WPT service state machine is driven to its idle
    /// state, so the coil is off because of where that state machine is - not
    /// because of a pause flag that something else might clear. The console's 's'
    /// command is what starts power transfer, and Button 1 is ignored.
    class StateManual : public eda::State
    {
    public:
        StateManual(eda::StateMachine *stateMachine) : State("Manual", stateMachine) {};

        void Entry();
        void DispatchEvent(uint32_t eventId, uint32_t optDataAddress);
        void Exit();

    private:
        svc::WptManager &mWptManager = svc::WptManager::Instance();
    };
}

#endif // WPT_MANUAL_DEBUG_MODE

#endif // STATE_MANUAL_H
