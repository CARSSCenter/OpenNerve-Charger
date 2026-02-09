/**
 * @name Hornet / WPT Charger
 * @file svc_ble_state_machine.cpp
 * @brief BLE State Machine Service Layer
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "svc_ble_state_machine.h"
#include "../svc_ble_manager.h"
#include "../svc_ble_port.h"
#include "state_idle.h"

namespace svc
{

    BleStateMachine::BleStateMachine() : StateMachine("BleStateMachine", &mStateIdle), mStateIdle(this), mStateScanning(this)
    {
    }

    BleStatePointers *BleStateMachine::GetStates()
    {
        static BleStatePointers states;
        states.pStateIdle = &mStateIdle;
        states.pStateScanning = &mStateScanning;
        return &states;
    }

} // namespace svc