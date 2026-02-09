/**
 * @name Hornet / WPT Charger
 * @file svc_ble_subsystem.cpp
 * @brief BLE Service Layer Subsystem
 * 
 * @copyright Copyright (c) 2024
 */

#include "svc_ble_subsystem.h"

#include "svc_ble_port.h"
//TODO: ADD APP SYSTEM .h FILE
#include "../../core_layer/event_driven_architecture/manager/eda_manager.h"

#include <cstdint>

namespace svc
{

    BleSubsystem &BleSubsystem::Instance()
    {
        static BleSubsystem instance;
        return instance;
    }

    BleSubsystem::BleSubsystem() : mBlePort(), mBleActiveObject(), mBleStateMachine()
    {
    }

    void BleSubsystem::Init()
    {
        LOG_INFO("BleSubsytem: Initialized \n")
        mBleActiveObject.InitTask(eda::ActiveObjectPriorities_e::sd_ble_task, "Ble");
        mBlePort.Init(app::PortList_e::BLE_PORT, mBleActiveObject);
        mBleStateMachine.Init();
    }

    void BleSubsystem::DispatchEvent(uint32_t eventId, uint32_t optDataAddress)
    {
        mBleStateMachine.DispatchEvent(eventId, optDataAddress);
    }
}