/**
 * @name Hornet / WPT Charger
 * @file eda_port.h
 * @brief Port class declaration
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef EDA_PORT_H
#define EDA_PORT_H

#define MAX_EVENT_ENUM_LENGTH 20

namespace eda
{
    class ActiveObject;
};

#include "../../application_layer/app_port_list.h"
#include "../active_object/eda_active_object.h"

namespace eda
{

    using EventCallback_t = void(*)(uint32_t optDataAddress);

    class Port
    {
        friend class ActiveObject;

    public:
        Port();

        /**
         * @brief Initialize the port
         * @param portID port identifier
         * @param activeObject reference to the active object
         */
        void Init(app::PortList_e portID, ActiveObject &activeObject);
        
        /**
         * @brief Set an event callback
         * @param eventID event identifier
         * @param eventCallback event callback
         */
        void SetEventCallback(uint32_t eventID, EventCallback_t eventCallback);

        /**
         * @brief Send an event to the active object
         * @param portID port identifier
         * @param eventID event identifier
         * @param optDataAddress optional data address
         */
        static void SendEvent(app::PortList_e portID, uint32_t eventID, uint32_t optDataAddress);

        /**
         * @brief Send an event to the active object from an ISR
         * @param portID port identifier
         * @param eventID event identifier
         * @param optDataAddress optional data address
         */
        static void SendEventFromISR(app::PortList_e portID, uint32_t eventID, uint32_t optDataAddress);

        /**
         * @brief ID of the port
         */
        app::PortList_e mPortID;

        /**
         * @brief Active object instance associated with the port
         */
        ActiveObject *mActiveObject;

    protected:
        /**
        * @brief Execute the callback
        * @param eventID event identifier
        */
        void ExecuteCallback(uint32_t eventID);


    private:
        /**
         * @brief Execute the event
         * @param eventID event identifier
         * @param optDataAddress optional data address
         */
        virtual void ExecuteEvent(uint32_t eventID, uint32_t optDataAddress) = 0;

        /**
         * @brief Array containing the event callbacks. The index of the array indicates the event ID.
         */
        EventCallback_t mEventCallback[MAX_EVENT_ENUM_LENGTH];
    };
}

#endif
