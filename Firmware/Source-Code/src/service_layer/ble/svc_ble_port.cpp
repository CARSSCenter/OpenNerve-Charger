/** * 
 * @name Hornet / WPT Charger
 * @file svc_ble_port.cpp
 * @brief BLE Service Layer Port
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "svc_ble_port.h"
#include "../../application_layer/app_system.h"
#include "svc_ble_subsystem.h"


namespace svc
{

    BlePort::BlePort() 
    {
    }

    void BlePort::ExecuteEvent(uint32_t eventId, uint32_t optDataAddress)
    {
        BleSubsystem::Instance().DispatchEvent(eventId, optDataAddress);
        BlePort::ExecuteCallback(eventId);
    }

}