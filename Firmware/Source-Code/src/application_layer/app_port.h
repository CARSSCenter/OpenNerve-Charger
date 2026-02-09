/**
 * @name Hornet / WPT Charger
 * @file app_port.h
 * @brief Application port class declaration
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef APP_PORT_H
#define APP_PORT_H

#include "../core_layer/event_driven_architecture/port/eda_port.h"
#include "app_port_list.h"

namespace app
{

    class SystemPort : public eda::Port
    {
    public:
        SystemPort();

        /// List of events that can be sent to the port
        enum class Event_e : uint16_t
        {
            INVALID = 0x00,

            PING_PORT = 0x01,

            POWER_UP_SUCCESS = 0x02,

            TURN_OFF = 0x03,

            GO_TO_IDLE = 0x04,

            GO_TO_STAND_BY = 0x05,

            ERROR_EVENT = 0x06,

            DEVICE_RESET = 0x07,

            BLE_SCAN_TIMEOUT = 0x08,

            BLE_DEVICE_FOUND = 0x09,

            START_SCANNING = 0x0B,

            BATTERY_CHARGED = 0x0C,

            BLE_INITIALIZED = 0x0D,

            WPT_SCAN_TIMEOUT = 0x0E,

            WPT_CHARGING = 0x0F,

            BUTTON_PRESSED = 0x10,

            BUTTON_DFU_PRESSED = 0x11,
        };

        static void SendEvent(Event_e eventID, uint32_t optDataAddress)
        {
            eda::Port::SendEvent(PortList_e::SYSTEM_PORT, static_cast<uint32_t>(eventID), static_cast<uint32_t>(optDataAddress));
        }

        static void SendEventFromISR(Event_e eventID, uint32_t optDataAddress)
        {
            eda::Port::SendEventFromISR(PortList_e::SYSTEM_PORT, static_cast<uint32_t>(eventID), static_cast<uint32_t>(optDataAddress));
        }

    private:
        void ExecuteEvent(uint32_t eventID, uint32_t optDataAddress);
    };

}

#endif // APP_PORT_H
