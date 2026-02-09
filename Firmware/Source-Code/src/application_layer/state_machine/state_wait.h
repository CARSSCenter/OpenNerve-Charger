/**
 * @name Hornet / WPT Charger
 * @file state_wait.h
 * @brief Header file for wait state
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef STATE_WAIT_H
#define STATE_WAIT_H

#include "../../core_layer/event_driven_architecture/state_machine/eda_state_machine.h"

namespace app
{

    class StateWait : public eda::State
    {
    public:
        StateWait(eda::StateMachine *stateMachine) : State("Wait", stateMachine) {};

        void Entry();
        void DispatchEvent(uint32_t eventId, uint32_t optDataAddress);
        void Exit();
    };

}

#endif // STATE_WAIT_H