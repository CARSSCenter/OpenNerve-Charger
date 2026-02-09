/**
 * @name Hornet / WPT Charger
 * @file state_initialization.h
 * @brief Header file for the initialization state
 *
 *@copyright Copyright (c) 2024
 */

#ifndef STATE_INITIALIZATION_H
#define STATE_INITIALIZATION_H

#include "../../core_layer/event_driven_architecture/state_machine/eda_state_machine.h"

namespace app
{
    class StateInitialization : public eda::State
    {
    public:
        StateInitialization(eda::StateMachine *stateMachine) : State("Initialization", stateMachine) {};

        void Entry();
        void DispatchEvent(uint32_t eventId, uint32_t optDataAddress);
        void Exit();
    };
} // namespace app

#endif // STATE_INITIALIZATION_H