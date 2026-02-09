/**
 * @name Hornet / WPT Charger
 * @file svc_ble_state_machine.h
 * @brief BLE State Machine Service Layer
 *
 * @copyright Copyright (c) 2024
 */

#ifndef SVC_BLE_STATE_MACHINE_H
#define SVC_BLE_STATE_MACHINE_H

#include "../../core_layer/event_driven_architecture/state_machine/eda_state_machine.h"
#include "svc_ble_state_machine.h"
#include "../svc_ble_manager.h"
#include "state_idle.h"
#include "state_scanning.h"

namespace svc
{
    struct BleStatePointers
    {
        BleStateIdle *pStateIdle;
        StateScanning *pStateScanning; 
    };

    class BleStateMachine : public eda::StateMachine
    {
    public:
        BleStateMachine();

        BleStatePointers *GetStates();

    private:
        // TODO: Define the ble_event_t structure and the rest of the types

        BleStateIdle mStateIdle;
        StateScanning mStateScanning;        

    };
} 

#endif // SVC_BLE_STATE_MACHINE_H