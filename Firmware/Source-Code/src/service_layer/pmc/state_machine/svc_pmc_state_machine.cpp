/**
 * @name Hornet / WPT Charger
 * @file svc_pmc_state_machine.cpp
 * @brief PmcStateMachine class implementation
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "svc_pmc_state_machine.h"

#include "eda_manager.h"

namespace svc
{
    // PMC State Machine Initialization

    PmcStateMachine::PmcStateMachine() : StateMachine("PMC", &mStateIdle), mStateIdle(this), mStateEnable(this), mStateChargingBattery(this)
    {
    }

    PmcStatePointers *PmcStateMachine::GetStates()
    {
        static PmcStatePointers states;
        states.pStateIdle = &mStateIdle;
        states.pStateEnable = &mStateEnable;
        states.pStateChargingBattery = &mStateChargingBattery;
        return &states;
    }

}