/**
 * @name Hornet / WPT Charger
 * @file eda_active_object.cpp
 * @brief Active Object class implementation
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "eda_active_object.h"

#include <cstdint>

namespace eda
{
    ActiveObject::ActiveObject() : mTaskHandle(),
                                   mQueueHandle(),
                                   mTaskControlBlock(),
                                   mQueue(){};

    void ActiveObject::InitTask(eda::ActiveObjectPriorities_e priority, const char *const pTaskName)
    {
        mTaskHandle = xTaskCreateStatic((TaskFunction_t)&ProcessEvents, pTaskName, stack_size, this, static_cast<UBaseType_t>(priority), reinterpret_cast<StackType_t *const>(&taskStack), &mTaskControlBlock);
        // Todo: Handle null taskHandle
        mQueueHandle = xQueueCreateStatic(queue_length, queue_item_size, taskQueueMemory, &mQueue);
        // Todo: Handle null queueHandle
    }

    void ActiveObject::SendEvent(Port &port, uint32_t eventId, uint32_t optDataAddress)
    {
        Event_t event{&port, eventId, optDataAddress};
        const BaseType_t status = xQueueSend(mQueueHandle, &event, 0U);
    }

    void ActiveObject::SendEventFromISR(Port &port, uint32_t eventId, uint32_t optDataAddress)
    {
        Event_t event{&port, eventId, optDataAddress};
        BaseType_t yieldReq = pdFALSE;

        const BaseType_t status = xQueueSendFromISR(mQueueHandle, &event, &yieldReq);
        portYIELD_FROM_ISR(yieldReq);
    }

    void ActiveObject::ProcessEvents(void *pActiveObject)
    {
        // Sanity casting
        ActiveObject *const activeObject = reinterpret_cast<ActiveObject *>(pActiveObject);
        // TODO: Add logging when available

        while (true)
        {
            Event_t event;
            const BaseType_t status = xQueueReceive(activeObject->mQueueHandle, &event, portMAX_DELAY);
            if (pdPASS == status)
            {
                if (NULL == event.port)
                {
                    // TODO handle null port
                }
                else
                {
                    event.port->ExecuteEvent(event.eventId, event.optDataAddress);
                }
            }
        }
    }
}
