/**
 * @name Hornet / WPT Charger
 * @file state_charge.h
 * @brief Header file for scanning state
 *
 * @copyright Copyright (c) 2024
 */

#ifndef STATE_SCAN_H
#define STATE_SCAN_H

#include "eda_state_machine.h"
#include "svc_wpt_manager.h"

namespace app
{
    class StateScan : public eda::State
    {
    public:
        StateScan(eda::StateMachine *stateMachine) : State("Scan", stateMachine) {};

        void Entry();
        void DispatchEvent(uint32_t eventId, uint32_t optDataAddress);
        void Exit();

    private:
        svc::WptManager &mWptManager = svc::WptManager::Instance();
    };

}

#endif // STATE_SCAN_H