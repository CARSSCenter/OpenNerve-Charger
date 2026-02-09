/**
 * @name Hornet / WPT Charger
 * @file app_port.cpp
 * @brief Application port class implementation
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "app_port.h"
#include "app_system.h"

namespace app
{

    SystemPort::SystemPort()
    {
    }

    void SystemPort::ExecuteEvent(uint32_t eventID, uint32_t optDataAddress)
    {
        System::GetInstance().mSystemStateMachine.DispatchEvent(eventID, optDataAddress);        
    }

} // namespace app