/**
 * @name Hornet / WPT Charger
 * @file state_slow_charge_and_scan.h
 * @brief Header file for the slow charge and scan state
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef STATE_SLOW_CHARGE_AND_SCAN_H
#define STATE_SLOW_CHARGE_AND_SCAN_H

#include "eda_state_machine.h"
#include "svc_wpt_manager.h"

namespace app
{

    class StateSlowChargeAndScan : public eda::State
    {
    public:
        StateSlowChargeAndScan(eda::StateMachine *stateMachine) : State("Slow Charge And Scan", stateMachine) {};

        void Entry();
        void DispatchEvent(uint32_t eventId, uint32_t optDataAddress);
        void Exit();

    private:
        svc::WptManager &mWptManager = svc::WptManager::Instance();
    };

}

#endif // STATE_SLOW_CHARGE_AND_SCAN_H