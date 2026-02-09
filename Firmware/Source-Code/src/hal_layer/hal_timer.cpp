/**
 * @name Hornet / WPT Charger
 * @file hal_timer.cpp
 * @brief Timer Hardware Abstraction Layer for nrf52840
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "hal_timer.h"

#include "nrf_drv_timer.h"
#include "sdk_errors.h"

namespace hal
{

    Timer::Timer(Config_t config, const EventHandler_t *eventHandler, const void *context)
        : mConfig(config), mEventHandler(eventHandler)
    {
        mCallerContext = context;
        mDriverInstance = mDriverTimers[static_cast<size_t>(config.timerNumber)];
    }

    Timer::ErrorCode Timer::Init()
    {
        ret_code_t errCode = InitDriver();
        if (errCode != NRF_SUCCESS)
        {
            return ErrorCode::INIT;
        }

        SetUpTimerInCompareMode();

        return ErrorCode::SUCCESS;
    }

    void Timer::UnInit() const
    {
        nrf_drv_timer_uninit(&mDriverInstance);
    }

    void Timer::Start() const
    {
        nrf_drv_timer_is_enabled(&mDriverInstance) ? nrf_drv_timer_resume(&mDriverInstance)
                                                   : nrf_drv_timer_enable(&mDriverInstance);
    }

    void Timer::Stop() const
    {
        nrf_drv_timer_pause(&mDriverInstance);
    }

    void Timer::Clear() const
    {
        nrf_drv_timer_clear(&mDriverInstance);
    }

    nrf_drv_timer_t *Timer::GetInstance()
    {
        return &mDriverInstance;
    }

    void Timer::DriverCallback(nrf_timer_event_t eventType, void *context)
    {
        if (eventType != NRF_TIMER_EVENT_COMPARE0)
        {
            // An event different than `NRF_TIMER_EVENT_COMPARE0 should never happen
            return;
        }

        auto instance = static_cast<Timer *>(context);

        if (instance->mEventHandler == nullptr)
        {
            return;
        }

        instance->mEventHandler(instance->mCallerContext);
    }

    ret_code_t Timer::InitDriver()
    {
        nrf_drv_timer_config_t config =
            NRF_DRV_TIMER_DEFAULT_CONFIG; /// 16MHz, Timer mode: timer, Bit Width: Timer bit width 8 bit
        config.p_context = static_cast<void *>(this);

        ret_code_t errCode = nrf_drv_timer_init(&mDriverInstance, &config, DriverCallback);
        return errCode;
    }

    void Timer::SetUpTimerInCompareMode() const
    {
        const uint32_t timeIntervalTicks = Timer128NsToTicks(mConfig.period);

        const nrf_timer_short_mask_t shortMask = mConfig.autostart ? NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK
                                                                   : NRF_TIMER_SHORT_COMPARE0_STOP_MASK;

        nrf_drv_timer_extended_compare(&mDriverInstance,
                                       NRF_TIMER_CC_CHANNEL0,
                                       timeIntervalTicks,
                                       shortMask,
                                       true);
    }

    uint32_t Timer::Timer128NsToTicks(uint32_t timer_ns) const
    {
        // To be used only when the frequency is set to 16MHz
        uint32_t ticks = timer_ns > 128u ? timer_ns >> 6u : 1u; /// ticks ~ timer_ns*2 / 128
        return ticks;
    }

};
