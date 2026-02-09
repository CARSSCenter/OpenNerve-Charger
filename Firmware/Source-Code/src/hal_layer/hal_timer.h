/**
 * @name Hornet / WPT Charger
 * @file hal_timer.h
 * @brief Timer Hardware Abstraction Layer for nrf52840
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef HAL_TIMER_H
#define HAL_TIMER_H

#include "nrf_drv_timer.h"
#include "sdk_errors.h"

#include <cstdint>
#include <cstddef>

namespace hal
{

/// Timer HAL which wraps the hardware timer peripheral functionality.
///
/// The time interval is specified in microseconds, with minimum and
/// maximum values of 1 microseconds and about 4 minutes, respectively.
///
/// Timer accuracy is about `1/16MHz < 0.1us`.
///
/// @note Every timer peripheral in use must be enabled in the project's
/// `sdk_config.h` file (e.g., if TIMER1 is used, the `TIMER1_ENABLED`
/// option must be set to `1`).
/// @note `TIMER0` must not be used, as it is already used by the softdevice.
class Timer
{
public:
    /// Function type used by the caller to handle timer events.
    using EventHandler_t = void(void const* context);

    /// Peripheral instance number.
    enum class PeripheralNumber : uint8_t
    {
#if TIMER0_ENABLED
        TIMER_0,
#endif

#if TIMER1_ENABLED
        TIMER_1,
#endif

#if TIMER2_ENABLED
        TIMER_2,
#endif

#if TIMER3_ENABLED
        TIMER_3,
#endif

#if TIMER4_ENABLED
        TIMER_4,
#endif

        NUMBER_OF_TIMERS,
    };

    /// Timer configuration data structure.
    struct Config_t
    {
        uint32_t period; ///< Time interval between timer events, in nanoseconds. It should be a
                         ///< multiple of 128ns
        bool autostart;  ///< Whether to restart the timer after it expires.
        PeripheralNumber timerNumber;
    };

    /// Timer error code data type.
    enum class ErrorCode : uint8_t
    {
        SUCCESS, ///< Operation completed without errors.
        INIT,    ///< Initialization error.
    };

    Timer(Config_t config,
          const EventHandler_t* eventHandler = nullptr,
          const void* context = nullptr);

    /// Initialize the timer's hardware.
    ///
    /// This method must be called before using any of the timer's functionality.
    ///
    /// @return `SUCCESS` if the operation is successful.
    /// @return `INIT` if there was an initialization error.
    ErrorCode Init();

    /// Deinitialize the timer's hardware.
    void UnInit() const;

    /// Resume the timer from the current count value.
    ///
    /// When calling this method for the first time, the count value is set to 0.
    void Start() const;

    /// Stop the timer. The count value is preserved.
    void Stop() const;

    /// Reset the count value.
    void Clear() const;

    nrf_drv_timer_t* GetInstance();

private:
    static void DriverCallback(nrf_timer_event_t eventType, void* context);

    ret_code_t InitDriver();
    void SetUpTimerInCompareMode() const;
    uint32_t Timer128NsToTicks(uint32_t timer_ns) const;

    const Config_t mConfig;
    const EventHandler_t* const mEventHandler;
    nrf_drv_timer_t mDriverInstance;

    void const* mCallerContext;

    const nrf_drv_timer_t mDriverTimers[static_cast<size_t>(PeripheralNumber::NUMBER_OF_TIMERS)] = {
#if TIMER0_ENABLED
        {.p_reg = NRF_TIMER0,
         .instance_id = NRFX_TIMER0_INST_IDX,
         .cc_channel_count = TIMER0_CC_NUM},
#endif

#if TIMER1_ENABLED
        {.p_reg = NRF_TIMER1,
         .instance_id = NRFX_TIMER1_INST_IDX,
         .cc_channel_count = TIMER1_CC_NUM},
#endif

#if TIMER2_ENABLED
        {.p_reg = NRF_TIMER2,
         .instance_id = NRFX_TIMER2_INST_IDX,
         .cc_channel_count = TIMER2_CC_NUM},
#endif

#if TIMER3_ENABLED
        {.p_reg = NRF_TIMER3,
         .instance_id = NRFX_TIMER3_INST_IDX,
         .cc_channel_count = TIMER3_CC_NUM},
#endif

#if TIMER4_ENABLED
        {.p_reg = NRF_TIMER4,
         .instance_id = NRFX_TIMER4_INST_IDX,
         .cc_channel_count = TIMER4_CC_NUM},
#endif
    };
};

};

#endif
