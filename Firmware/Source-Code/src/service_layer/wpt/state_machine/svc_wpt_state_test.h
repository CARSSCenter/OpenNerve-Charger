/**
 * @name Hornet / WPT Charger
 * @file svc_wpt_state_test.h
 * @brief Header file for the wpt test state
 *
 *@copyright Copyright (c) 2024
 */

#ifndef SVC_WPT_STATE_TEST_H
#define SVC_WPT_STATE_TEST_H

#include "eda_state_machine.h"

#include "svc_wpt_manager.h"

namespace svc
{
    /// This state is used for triggering events with testing purposes
    class StateTest : public eda::State
    {
    public:
        /// Constructor sets the state name
        StateTest(eda::StateMachine *stateMachine) : State("Test", stateMachine)
        {
        }
        /// Entry action to be executed when entering the state
        void Entry();
        /// Dispatch an event into the state
        void DispatchEvent(uint32_t eventId, uint32_t optDataAddress);
        /// Exit action to be executed when exiting the state
        void Exit();

    private:
    };
} // namespace svc

#endif // SVC_WPT_STATE_TEST_H