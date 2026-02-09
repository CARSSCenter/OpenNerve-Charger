/**
 * @name Hornet / WPT Charger
 * @file eda_queue.cpp
 * @brief Queue class implementation
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "eda_queue.h"

namespace eda
{
    void Queue::Create(uint32_t length, uint32_t itemSize, uint8_t * buffer)
    {
        /*TODO: Add Sanity checks */
        mQueueLength = length;
        mQueueItemSize = itemSize;
        mQueueSize = length*itemSize;
        mQueueHandle = xQueueCreateStatic(mQueueLength, mQueueItemSize, buffer, &mQueue);
    };

    uint16_t Queue::Get(void * const item)
    {
        return xQueueReceive(mQueueHandle, item, 0U);
    };

    uint16_t Queue::Put(void * const item)
    {
        return xQueueSend(mQueueHandle, item, 0U);
    };

    void Queue::PutFromISR(void * const item)
    {
        BaseType_t yieldReq = pdFALSE;
        const BaseType_t status = xQueueSendFromISR(mQueueHandle, item, &yieldReq);
        portYIELD_FROM_ISR(yieldReq);
    };
}