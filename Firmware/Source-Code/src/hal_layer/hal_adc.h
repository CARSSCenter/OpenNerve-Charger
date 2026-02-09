/**
 * @name Hornet / WPT Charger
 * @file hal_adc.h
 * @brief ADC Hardware Abstraction Layer for nrf52840
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include "hal_timer.h"

#include "nrf_drv_saadc.h"
#include "nrf_ppi.h"
#include "nrf_drv_timer.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

const int16_t ADC_RESOLUTION = 4095; // 12-bit ADC
const int16_t VCC_mV = 5000;    // 5.0V in millivolts

namespace hal
{
 /// ADC Hardware Abstraction Layer class.
// TODO: Tidy up the remaining hardware-dependent code

class Adc
{

public:
    /// Enum class for ADC error codes.
    enum class ErrorCode : uint8_t
    {
        SUCCESS,
        INVALID_CONFIG,
        FAIL
    };

    /// Struct for ADC channel configuration.
    typedef struct
    {
        nrf_saadc_input_t pin;
    } AdcChannel;

    /// Get the singleton instance of the ADC class.
    ///
    /// @return Reference to the ADC instance.
    static Adc& get_instance()
    {
        static Adc instance;
        return instance;
    };

    /// Take sample from the ADC channel.
    ///
    /// @param channel The ADC channel to sample.
    /// @param sample Pointer to the sample value.
    ///
    /// @return An error code indicating the success or failure of the operation.
    static ErrorCode take_measurement(AdcChannel* channel, int16_t* value);

private:
    // Private constructor to enforce singleton pattern
    Adc()
    {}

    Adc(Adc const&) = delete; // Delete copy constructor

    void operator=(Adc const&) = delete; // Delete copy assignment operator
};

} // namespace hal

