/**
 * @name Hornet / WPT Charger
 * @file app_port_list.h
 * @brief Ports managed by the application
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef APP_PORT_LIST_H
#define APP_PORT_LIST_H

#include <cstdint>

namespace app
{
    enum class PortList_e : uint8_t
    {
        INVALID_PORT                    = 0x00,
        SYSTEM_PORT                     = 0x01,
        WPT_PORT                        = 0x02,
        BLE_PORT                        = 0x03,
        PMC_PORT                        = 0x04,
    };
}

#endif