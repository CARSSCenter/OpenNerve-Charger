/**
 * @name Hornet / WPT Charger
 * @file svc_wpt_state_machine.cpp
 * @brief WptStateMachine class implementation
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "svc_wpt_state_machine.h"

#include "../../core_layer/event_driven_architecture/manager/eda_manager.h"

namespace svc
{
    // WPT State Machine Initialization

    WptStateMachine::WptStateMachine() : StateMachine("WPT", &mStateIdle), mStateIdle(this), mStateTest(this), mStateCharging(this), mStateSlowCharge(this)
    {
    }

    StatePointers *WptStateMachine::GetStates()
    {
        static StatePointers states;
        states.pStateIdle = &mStateIdle;
        states.pStateTest = &mStateTest;
        states.pStateSlowCharge = &mStateSlowCharge;
        states.pStateCharging = &mStateCharging;
        return &states;
    }
   
}