/**
 * @name Hornet / WPT Charger
 * @file svc_wpt_state_slow_charge.h
 * @brief Header file for the wpt slow charge state
 *
 *@copyright Copyright (c) 2024
 */

#ifndef SVC_WPT_STATE_SLOW_CHARGE_H
#define SVC_WPT_STATE_SLOW_CHARGE_H

#include "eda_state_machine.h"

#include "svc_wpt_manager.h"

namespace svc
{
    /// This state is the slow charge state for the wpt state machine
    class StateSlowCharge : public eda::State
    {
    public:
        /// Constructor for the SlowCharge state
        StateSlowCharge(eda::StateMachine *stateMachine) : State("Slow Charge", stateMachine) {};
        /// Entry action to be executed when entering the state
        void Entry();
        /// Dispatch an event into the state
        void DispatchEvent(uint32_t eventId, uint32_t optDataAddress);
        /// Exit action to be executed when exiting the state
        void Exit();

    private:
        WptManager &mWptManager = WptManager::Instance();
    };
} // namespace svc
#endif // SVC_WPT_STATE_SLOW_CHARGE_H