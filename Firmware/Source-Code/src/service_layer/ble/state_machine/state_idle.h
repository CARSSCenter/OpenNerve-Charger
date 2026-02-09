/**
 * @name Hornet / WPT Charger
 * @file state_idle.h
 * @brief BLE Idle State header file
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef STATE_IDLE_H
#define STATE_IDLE_H

#include "../../../core_layer/event_driven_architecture/state_machine/eda_state_machine.h"

namespace svc
{
    class BleStateIdle : public eda::State
    {
    public:
        BleStateIdle(eda::StateMachine *stateMachine) : State("BleIdle", stateMachine) {};

        void Entry();
        void DispatchEvent(uint32_t eventId, uint32_t optDataAddress);
        void Exit();
    };
}

#endif // STATE_IDLE_H