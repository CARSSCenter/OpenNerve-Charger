/**
 * @name Hornet / WPT Charger
 * @file svc_wpt_port.h
 * @brief WptPort class implementation
 * 
 * @copyright Copyright (c) 2024
 *
 */

#ifndef WPT_PORT_H
#define WPT_PORT_H

#include "../../core_layer/event_driven_architecture/port/eda_port.h"
#include "../../application_layer/app_port_list.h"

#include <cstdint>

namespace svc
{
    class WptPort : public eda::Port
    {
    public:
        /// List of events that can be sent to the port
        enum class Event_e : uint8_t
        {
            /** @TODO: Define more precisely which are going to be the events for WPT */
            INVALID = 0x00,
            PING_PORT = 0x01,
            INITIALIZE = 0x02,
            WPT_POWER_ON = 0x03,
            WPT_LOAD_DETECTED = 0x04,
            WPT_CHARGE = 0x05,
            WPT_STOP_SCAN = 0x06,
            WPT_POWER_OFF = 0x07,
            WPT_FAULT_CONDITION = 0x08,
            WPT_BATTERY_CHARGING = 0x09,
            WPT_BATTERY_CHARGED = 0x0B,
            WPT_SLOW_CHARGE = 0x0C,
            WPT_SCAN_TIMEOUT = 0x0D,
            WPT_ADJUST_POWER = 0x0E
        };

        WptPort();

        // \brief Helper function for sending events to the port
        ///
        /// \param[in]  eventId     Event to send to the port
        /// \param[in]  optDataAddress     Optional data associated with the eventId.
        ///                         This can be a pointer to a separate buffer,
        ///                         but buffer ownership must be managed apart
        ///                         from the port.
        static void SendEvent(Event_e eventId, uint32_t optDataAddress)
        {
            eda::Port::SendEvent(app::PortList_e::WPT_PORT, static_cast<uint32_t>(eventId), optDataAddress);
        }

        /// \brief Helper function for sending events to the port from an ISR
        ///
        /// \param[in]  eventId     Event to send to the port
        /// \param[in]  optDataAddress     Optional data associated with the eventId.
        ///                         This can be a pointer to a separate buffer,
        ///                         but buffer ownership must be managed apart
        ///                         from the port.
        static void SendEventFromISR(Event_e eventId, uint32_t optDataAddress)
        {
            eda::Port::SendEventFromISR(app::PortList_e::WPT_PORT, static_cast<uint32_t>(eventId), optDataAddress);
        }

    private:
        void ExecuteEvent(uint32_t eventId, uint32_t optDataAddress);
    };
}

#endif // WPT_PORT_H