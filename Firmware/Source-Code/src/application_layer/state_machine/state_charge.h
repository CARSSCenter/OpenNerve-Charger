/**
 * @name Hornet / WPT Charger
 * @file state_charge.h
 * @brief Header file for charging state
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef STATE_CHARGE_H
#define STATE_CHARGE_H

#include "../../core_layer/event_driven_architecture/state_machine/eda_state_machine.h"

namespace app
{

    class StateCharge : public eda::State
    {
    public:
        StateCharge(eda::StateMachine *stateMachine) : State("Charge", stateMachine) {};

        void Entry();
        void DispatchEvent(uint32_t eventId, uint32_t optDataAddress);
        void Exit();
    };

}

#endif // STATE_CHARGE_H
