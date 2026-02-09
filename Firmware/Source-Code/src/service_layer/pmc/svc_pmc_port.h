/**
 * @name Hornet / WPT Charger
 * @file svc_pmc_port.h
 * @brief PmcPort class implementation
 * 
 * @copyright Copyright (c) 2024
 *
 */

#ifndef PMC_PORT_H
#define PMC_PORT_H

#include "app_port_list.h"
#include "eda_port.h"

#include <cstdint>

namespace svc
{
    class PmcPort : public eda::Port
    {
    public:
        /// List of events that can be sent to the port
        enum class Event_e : uint8_t
        {
            /** @TODO: Define more precisely which are going to be the events for PMC */
            INVALID = 0x00,
            PING_PORT = 0x01,
            INITIALIZE = 0x02,
            PMC_POWER_ON = 0x03,
            PMC_POWER_OFF = 0x04,
            PMC_START_CHARGING = 0x06,
            PMC_BATTERY_CHARGED = 0x07,
            PMC_BATTERY_CHARGING = 0x08,
            PMC_READ_FAST_CHARGER_INDICATOR = 0x09,
            PMC_READ_CHARGER_PRESENT_INDICATOR = 0x0B,
            PMC_READ_ISET = 0x0C,
            PMC_FAULT_CONDITION = 0x0D,
        };

        PmcPort();

        // \brief Helper function for sending events to the port
        ///
        /// \param[in]  eventId     Event to send to the port
        /// \param[in]  optDataAddress     Optional data associated with the eventId.
        ///                         This can be a pointer to a separate buffer,
        ///                         but buffer ownership must be managed apart
        ///                         from the port.
        static void SendEvent(Event_e eventId, uint32_t optDataAddress)
        {
            eda::Port::SendEvent(app::PortList_e::PMC_PORT, static_cast<uint32_t>(eventId), optDataAddress);
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
            eda::Port::SendEventFromISR(app::PortList_e::PMC_PORT, static_cast<uint32_t>(eventId), optDataAddress);
        }

    private:
        void ExecuteEvent(uint32_t eventId, uint32_t optDataAddress);
    };
}

#endif // PMC_PORT_H