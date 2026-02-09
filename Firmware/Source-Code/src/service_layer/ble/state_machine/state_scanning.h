/**
 * @name Hornet / WPT Charger
 * @file state_scanning.h
 * @brief BLE Scanning State header file
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef STATE_SCANNING_H
#define STATE_SCANNING_H

#include "eda_state_machine.h"

namespace svc
{
    class StateScanning : public eda::State
    {
    public:
        StateScanning(eda::StateMachine *stateMachine) : State("Scanning", stateMachine) {};

        void Entry();
        void DispatchEvent(uint32_t eventId, uint32_t optDataAddress);
        void Exit();
    };
}

#endif // STATE_SCANNING_H