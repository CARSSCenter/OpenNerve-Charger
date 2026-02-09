/**
 * @name Hornet / WPT Charger
 * @file eda_manager.h
 * @brief Manager class implementation
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef EDA_MANAGER_H
#define EDA_MANAGER_H

#include "eda_manager_log_config.h"

#include "nrf_drv_clock.h"

#include <cstdint>
#include <FreeRTOS.h>
#include <task.h>

namespace eda
{
    class Manager
    {
        Manager(){};

        public:

        /**
         * @brief Initialize platform peripherals needed for execution
         *
         */
        static void Initialize();

        /**
         * @brief Starts the freeRTOS scheduler
         *
         */
        static void StartEventDrivenArchitecture();

        /**
         * @brief Initialize the log
         *
         */
        static void LogInit();

        /**
         * @brief Implements the idle task hook
         *
         */
        static void IdleHook();

        /**
         * @brief Initialize the system clock
         *
         */
        static void ClockInit();

        /**
         * @brief Blocks the current execution during time_ms
         *
         * @param time_ms blocked time
         */
        static void Delay(uint16_t ticks);

    };
}

#endif