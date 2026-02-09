/**
 * @name Hornet / WPT Charger
 * @file hal_adc.cpp
 * @brief ADC Hardware Abstraction Layer for nrf52840
 *
 * @copyright Copyright (c) 2024
 *
 */

 #include "hal_adc.h"
 
#include "eda_manager_log_config.h"
#include "hal_pinout.h"

#include "nrfx_saadc.h"

#include <array>
#include <numeric>  
#include <algorithm> 

namespace hal
{
   static void callback_test(nrfx_saadc_evt_t const* p_event)
{
    // TODO: We need to discuss how to handle this callback
}

Adc::ErrorCode Adc::take_measurement(AdcChannel* channel, int16_t* value)
{
    static nrf_saadc_value_t sample = 0;

    static constexpr nrfx_saadc_config_t config = NRFX_SAADC_DEFAULT_CONFIG;

    static nrf_saadc_channel_config_t channel_config;

    channel_config = NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(channel->pin);

    APP_ERROR_CHECK(nrfx_saadc_init(&config, callback_test));
    APP_ERROR_CHECK(nrfx_saadc_channel_init(channel->pin - 1, &channel_config));

    int32_t sum = 0;
    static constexpr uint8_t NUMBER_OF_SAMPLES = 10;
    
    std::array<int16_t, NUMBER_OF_SAMPLES> samples;

    // Fill the array with ADC samples using std::generate
    std::generate(samples.begin(), samples.end(), [&]() {
        nrfx_saadc_sample_convert(channel->pin - 1, &sample);
        return sample;
    });

    // Compute the average using std::accumulate
    int16_t average = std::accumulate(samples.begin(), samples.end(), 0) / NUMBER_OF_SAMPLES;

    // Convert the ADC count value to millivolts
    *value = static_cast<int16_t>(static_cast<uint32_t>(average) * 225 >> 6);

    nrfx_saadc_uninit();

    return ErrorCode::SUCCESS;
};
}
