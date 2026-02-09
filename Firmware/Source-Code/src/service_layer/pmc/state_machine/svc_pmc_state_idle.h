/**
 * @name Hornet / WPT Charger
 * @file svc_pmc_state_idle.h
 * @brief Header file for the pmc idle state
 *
 *@copyright Copyright (c) 2024
 */

#ifndef SVC_PMC_STATE_IDLE_H
#define SVC_PMC_STATE_IDLE_H

#include "eda_state_machine.h"

#include "svc_pmc_manager.h"

namespace svc
{
    /// This state is the entry state for the pmc state machine
    class PmcStateIdle : public eda::State
    {
    public:
        /// Constructor for the Idle state
        PmcStateIdle(eda::StateMachine *stateMachine) : State("Idle", stateMachine) {};
        /// Entry action to be executed when entering the state
        void Entry();
        /// Dispatch an event into the state
        void DispatchEvent(uint32_t eventId, uint32_t optDataAddress);
        /// Exit action to be executed when exiting the state
        void Exit();

    private:

        PmcManager &mPmcManager = PmcManager::Instance();

    };
}
#endif // SVC_PMC_STATE_IDLE_H