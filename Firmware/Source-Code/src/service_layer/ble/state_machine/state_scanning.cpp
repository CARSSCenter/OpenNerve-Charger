/**
 * @name Hornet / WPT Charger
 * @file state_scanning.cpp
 * @brief BLE State Machine Service Layer
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "state_scanning.h"
#include "svc_ble_state_machine.h"
#include "svc_ble_port.h"
#include "svc_ble_manager.h"


namespace svc
{
    void StateScanning::Entry()
    {
        BleStateMachine *stateMachine = reinterpret_cast<BleStateMachine *>(mStateMachine);
        BleManager::StartScanning();
    }

    void StateScanning::DispatchEvent(uint32_t eventId, uint32_t optDataAddress)
    {
        BleStateMachine *stateMachine = reinterpret_cast<BleStateMachine *>(mStateMachine);

        BleStatePointers *states = stateMachine->GetStates();

        switch (static_cast<BlePort::Event_e>(eventId))
        {
            case BlePort::Event_e::STOP_SCANNING:
            {
                stateMachine->ChangeState(states->pStateIdle);
                break;
            }
            case BlePort::Event_e::SCAN_TIMEOUT:
            {
                stateMachine->ChangeState(states->pStateIdle);
                break;
            }
        }
    }

    void StateScanning::Exit()
    {
        BleStateMachine *stateMachine = reinterpret_cast<BleStateMachine *>(mStateMachine);
        BleManager::StopScanning();
    }
}