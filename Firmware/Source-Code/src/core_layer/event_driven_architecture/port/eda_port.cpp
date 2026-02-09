/**
 * @name Hornet / WPT Charger
 * @file eda_port.cpp
 * @brief Port class implementation
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "eda_port.h"
#include "eda_manager.h"

namespace eda
{
    static const int32_t c_port_list_size_elements = 10;
    // Array containing the address of all the ports successfully instantiated
    static Port *mActivePortsList[static_cast<size_t>(c_port_list_size_elements)] = {};

    Port::Port()
    {
        mPortID = app::PortList_e::INVALID_PORT;
        mActiveObject = NULL;
    }

    void Port::Init(app::PortList_e portID, ActiveObject &activeObject)
    {
        if ((app::PortList_e::INVALID_PORT != portID) && (static_cast<int32_t>(portID) <= c_port_list_size_elements))
        {
            mPortID = portID;
            mActivePortsList[static_cast<int32_t>(portID)] = this;
            mActiveObject = &activeObject;
        }
    };

    void Port::SetEventCallback(uint32_t eventID, EventCallback_t eventCallback)
    {

        mEventCallback[eventID] = eventCallback;

    };

    void Port::SendEvent(app::PortList_e portID, uint32_t eventID, uint32_t optDataAddress)
    {
        if ((app::PortList_e::INVALID_PORT != portID) && (static_cast<int32_t>(portID) <= c_port_list_size_elements))
        {
            mActivePortsList[static_cast<int32_t>(portID)]->mActiveObject->SendEvent(*mActivePortsList[static_cast<int32_t>(portID)], eventID, optDataAddress);
        }
        else if ((NULL == mActivePortsList[static_cast<int32_t>(portID)]) || (NULL == mActivePortsList[static_cast<int32_t>(portID)]->mActiveObject))
        {
            // TODO: Handle uninitialized port
        };
    };

    void Port::SendEventFromISR(app::PortList_e portID, uint32_t eventID, uint32_t optDataAddress)
    {
        if ((app::PortList_e::INVALID_PORT != portID) && (static_cast<int32_t>(portID) <= c_port_list_size_elements))
        {
            mActivePortsList[static_cast<int32_t>(portID)]->mActiveObject->SendEventFromISR(*mActivePortsList[static_cast<int32_t>(portID)], eventID, optDataAddress);
        }
        else if ((NULL == mActivePortsList[static_cast<int32_t>(portID)]) || (NULL == mActivePortsList[static_cast<int32_t>(portID)]->mActiveObject))
        {
            // TODO: Handle uninitialized port
        };
    };

    void Port::ExecuteCallback(uint32_t eventID)
    {
        if (NULL != mEventCallback[eventID])
        {
            mEventCallback[eventID](NULL);
            LOG_DEBUG("Callback executed for event %d in port %d", eventID, mPortID);
        }
        else
        {
            LOG_DEBUG("No callback defined for event %d in port %d", eventID, mPortID);  
        }
    };


}