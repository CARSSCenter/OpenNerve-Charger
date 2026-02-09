/**
 * @name Hornet / WPT Charger
 * @file svc_ble_port.h
 * @brief BLE Service Layer Port
 *
 * @copyright Copyright (c) 2024
 */

#ifndef SVC_BLE_PORT_H
#define SVC_BLE_PORT_H

#include "eda_port.h"

namespace svc
{
    class BlePort : public eda::Port
    {
    public:
        BlePort();

        enum class Event_e : uint8_t
        {
            INVALID,
            PING_PORT,
            INITIALIZE,
            BLE_INITIALIZED,
            START_SCANNING,
            SCANNING_STARTED,
            STOP_SCANNING,
            SCAN_TIMEOUT,
            DEVICE_FOUND,
        };

        static void SendEvent(Event_e eventID, uint32_t optDataAddress)
        {
            eda::Port::SendEvent(app::PortList_e::BLE_PORT, static_cast<uint32_t>(eventID), optDataAddress);
        }
        static void SendEventFromISR(Event_e eventID, uint32_t optDataAddress)
        {
            eda::Port::SendEventFromISR(app::PortList_e::BLE_PORT, static_cast<uint32_t>(eventID), optDataAddress);
        }

    private:
        void ExecuteEvent(uint32_t eventID, uint32_t optDataAddress);
    };
}

#endif // SVC_BLE_PORT_H