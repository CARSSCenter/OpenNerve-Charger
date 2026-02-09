/**
 * @name Hornet / WPT Charger
 * @file eda_queue.h
 * @brief Queue class declaration
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef EDA_QUEUE_H
#define EDA_QUEUE_H

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
//#include "eda_manager.h" //not defined yet


namespace eda
{
  class Queue
  {

      public:
          /// @brief Queue creation
          /// @param length
          /// @param itemSize
          void Create(uint32_t length, uint32_t itemSize, uint8_t * buffer);
          /// @brief Unqueues an item
          /// @param item
          /// @return
          uint16_t Get(void * const item);
          /// @brief Enqueues an item
          /// @param item
          /// @return
          uint16_t Put(void * const item);
          /// @brief Enqueues an item from an ISR context
          void PutFromISR(void * const item);

      /// @brief Handle of a queue
          xQueueHandle mQueueHandle;
      /// @brief Statically allocated queue
          StaticQueue_t mQueue;
      /// @brief Item count
          uint32_t mQueueLength;
      /// @brief Item size
          uint32_t mQueueItemSize;
      /// @brief Total size of the queue
          uint32_t mQueueSize;
  };
}
#endif

