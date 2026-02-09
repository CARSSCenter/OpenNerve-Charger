/**
 * @name Timer
 * @file eda_timer.h
 * @brief Timer class declaration
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef EDA_TIMER_H
#define EDA_TIMER_H

#include "FreeRTOS.h"
#include "timers.h"

#include <cstdint>

namespace eda
{
    /**
     * @brief Callback function type for the timer
     */
    typedef void (*CallbackFunction)(TimerHandle_t xTimer);

    /**
     * @brief Enum type returned by the functions
     */
    enum class TimerErrorCode
    {
        /** Success */
        SUCCESS,
        /** Timer handle is null */
        TIMER_HANDLE_NULL,
    };

    class Timer
    {
    public:
        /**
         * @brief Timer constructor
         *
         * @param name Timer name
         * @param period Timer period in milliseconds
         * @param isPeriodic Flag to indicate if the timer is periodic
         * @param memoryBuffer Memory buffer for the timer
         * @param callback Callback function for the timer
         */
        Timer(const char *const name, uint32_t period, bool isPeriodic, CallbackFunction callback);

        /**
         *  @brief Function to start the timer
         */
        TimerErrorCode Start(void);

        /**
         * @brief Function to start the timer with a different period
         */
        TimerErrorCode Start(const uint32_t period);

        /**
         * @brief Function to stop the timer
         */
        void Stop(void);

        /**
         * @brief Function to start the timer from an ISR
         */
        TimerErrorCode StartFromISR(void);

        /**
         * @brief Function to start the timer from an ISR with a different period
         */
        TimerErrorCode StartFromISR(const uint32_t period);

        /**
         * @brief Function to stop the timer from an ISR
         */
        void StopFromISR(void);

    private:
        /**
         * @brief Timer handle
         */
        TimerHandle_t mTimerHandle;

        /**
         * @brief Callback function for the timer
         */
        CallbackFunction mCallback;

        /**
         * @brief Timer buffer
         */
        StaticTimer_t mTimerBuffer;
    };
}

#endif // TIMER_H