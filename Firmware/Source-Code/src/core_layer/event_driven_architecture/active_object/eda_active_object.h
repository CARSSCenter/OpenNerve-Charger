/**
 * @name Hornet / WPT Charger
 * @file eda_active_object.h
 * @brief Active Object class declaration
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef EDA_ACTIVE_OBJECT_H
#define EDA_ACTIVE_OBJECT_H

#include "eda_active_object_priorities.h"
#include "../port/eda_port.h"

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include <cstdint>

namespace eda
{
    class Port;
};

namespace eda
{
    /**
     * @brief Active Object composed by a freeRTOS task and queue
     */
    class ActiveObject
    {
        friend class Port;

    public:
        /**
         * @brief Default constructor for ActiveObject
         */
        ActiveObject();

        /**
         * @brief Initialize the task with the given priority and task name
         *
         * @param priority The priority of the task
         * @param pTaskName The name of the task
         */
        void InitTask(eda::ActiveObjectPriorities_e priority, const char *const pTaskName);

        /**
         * @brief The data passed to a task when an event occurs
         */
        struct Event_t
        {
            Port *port;              
            uint32_t eventId;        
            uint32_t optDataAddress; 
        };

        // Active Object task and queue parameters
        static constexpr uint32_t queue_length = 20U; 
        static constexpr uint32_t queue_item_size = sizeof(Event_t);
        static constexpr uint32_t queue_size = (queue_length * queue_item_size); 
        static constexpr uint32_t stack_size = 4U * configMINIMAL_STACK_SIZE;    

        /**
         * @brief Statically allocate memory for queue
         */
        uint8_t taskQueueMemory[queue_size];

        /**
         * @brief Memory region for Task stack
         */
        StackType_t taskStack[stack_size];

        /**
         * @brief The method executed by each task in its loop
         *
         * @param pActiveObject Pointer to the ActiveObject instance
         */
        static void ProcessEvents(void *pActiveObject);

        /**
         * @brief Send an event to the task's event queue.
         *
         * @param port The port instance where the event will be sent
         * @param eventId The id of the event, defined by the port
         * @param optDataAddress Optional data defined by the event, commonly a pointer to an object
         */
        void SendEvent(Port &port, uint32_t eventId, uint32_t optDataAddress);

        /**
         * @brief Send an event to the task's event queue from an ISR.
         *
         * @param port The port instance where the event will be sent
         * @param eventId The id of the event, defined by the port
         * @param optDataAddress Optional data defined by the event, commonly a pointer to an object
         */
        void SendEventFromISR(Port &port, uint32_t eventId, uint32_t optDataAddress);

        /**
         * @brief Handle of a task
         */
        xTaskHandle mTaskHandle;

        /**
         * @brief Handle of a queue
         */
        xQueueHandle mQueueHandle;

        /**
         * @brief Statically allocated control block for the task
         */
        StaticTask_t mTaskControlBlock;

        /**
         * @brief Statically allocated queue
         */
        StaticQueue_t mQueue;
    };
}

#endif