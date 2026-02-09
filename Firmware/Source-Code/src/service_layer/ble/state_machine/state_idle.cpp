/**
 * @name Hornet / WPT Charger
 * @file state_idle.cpp
 * @brief BLE Idle State implementation
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "state_idle.h"
#include "svc_ble_state_machine.h"
#include "svc_ble_port.h"
#include "svc_ble_manager.h"

namespace svc
{
    void BleStateIdle::Entry()
    {
        BleStateMachine *stateMachine = reinterpret_cast<BleStateMachine *>(mStateMachine);
    }

    void BleStateIdle::DispatchEvent(uint32_t eventId, uint32_t optDataAddress)
    {
        BleStateMachine *stateMachine = reinterpret_cast<BleStateMachine *>(mStateMachine);

        BleStatePointers *states = stateMachine->GetStates();

        switch (static_cast<BlePort::Event_e>(eventId))
        {

        case BlePort::Event_e::INITIALIZE:
        {
            BleManager::Init();
            BlePort::SendEvent(BlePort::Event_e::BLE_INITIALIZED, NULL);
            break;
        }

        case BlePort::Event_e::START_SCANNING:
        {
            stateMachine->ChangeState(states->pStateScanning);            
            break;
        }
        }
    }

    void BleStateIdle::Exit()
    {
        BleStateMachine *stateMachine = reinterpret_cast<BleStateMachine *>(mStateMachine);
    }
}