/**
 *
 * @name Hornet / WPT Charger
 * @file eda_timer.cpp
 * @brief Timer class implementation
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "eda_timer.h"

#include "FreeRTOS.h"
#include "timers.h"

namespace eda
{
    Timer::Timer(const char *const name, uint32_t period, bool isPeriodic, CallbackFunction callback)
        : mCallback(callback)
    {
        mTimerHandle = xTimerCreateStatic(name, pdMS_TO_TICKS(period), isPeriodic, nullptr, (TimerCallbackFunction_t)mCallback, &mTimerBuffer);
    }

    TimerErrorCode Timer::Start(void)
    {
        if (mTimerHandle != NULL)
        {
            xTimerStart(mTimerHandle, 0U);
            return TimerErrorCode::SUCCESS;
        }
        else
        {
            return TimerErrorCode::TIMER_HANDLE_NULL;
        }
    }

    TimerErrorCode Timer::Start(const uint32_t period)
    {
        if (mTimerHandle != NULL)
        {
            xTimerChangePeriod(mTimerHandle, period, 0U);
            xTimerStart(mTimerHandle, 0U);
            return TimerErrorCode::SUCCESS;
        }
        else
        {
            return TimerErrorCode::TIMER_HANDLE_NULL;
        }
    }

    void Timer::Stop(void)
    {
        if (mTimerHandle != NULL)
        {
            xTimerStop(mTimerHandle, 0U);
        }
    }

    TimerErrorCode Timer::StartFromISR(void)
    {
        if (mTimerHandle != NULL)
        {
            xTimerStartFromISR(mTimerHandle, 0U);
            return TimerErrorCode::SUCCESS;
        }
        else
        {
            return TimerErrorCode::TIMER_HANDLE_NULL;
        }
    }

    TimerErrorCode Timer::StartFromISR(const uint32_t period)
    {
        if (mTimerHandle != NULL)
        {
            xTimerChangePeriodFromISR(mTimerHandle, period, 0U);
            xTimerStartFromISR(mTimerHandle, 0U);
            return TimerErrorCode::SUCCESS;
        }
        else
        {
            return TimerErrorCode::TIMER_HANDLE_NULL;
        }
    }

    void Timer::StopFromISR(void)
    {
        if (mTimerHandle != NULL)
        {
            xTimerStopFromISR(mTimerHandle, 0U);
        }
    }
}