/**
 * @name Hornet / WPT Charger
 * @file hal_gpio.cpp
 * @brief GPIO Hardware Abstraction Layer for nrf52840
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "hal_gpio.h"

#include "nrf_gpio.h"

#include <cstdint>

// Define the static member variables
hal::gpio_call_back_t hal::Gpio::mCallBacks[40] = {}; // Allocate storage for callbacks
uint8_t hal::Gpio::mPinValues[40] = {};     // Allocate storage for pin values

namespace hal
{
    void Gpio::Init()
    {
        static bool is_initialized = 1;
        if (is_initialized == 1)
        {
            nrf_drv_gpiote_init();
            is_initialized = 0;
        }
    }

    uint8_t Gpio::Read(uint32_t pin_number)
    {
        return nrf_gpio_pin_read(pin_number);
    }

    uint8_t *Gpio::ReadMultiple(const uint32_t *pin_numbers, uint32_t num_pins)
    {
        for (uint32_t i = 0; i < num_pins; i++)
        {
            mPinValues[pin_numbers[i]] = nrf_gpio_pin_read(pin_numbers[i]);
        }
        return mPinValues;
    }

    void Gpio::Write(uint32_t pin_number, uint32_t value)
    {
        nrf_gpio_pin_write(pin_number, value);
    }

    void Gpio::WriteMultiple(const uint32_t *pin_numbers, const uint32_t *values, uint32_t num_pins)
    {
        for (uint32_t i = 0; i < num_pins; i++)
        {
            nrf_gpio_pin_write(pin_numbers[i], values[i]);
        }
    }

    void Gpio::SetCallback(HalPinEventHandler_t event_handler,
                           nrfx_gpiote_pin_t pin_num,
                           nrf_gpiote_polarity_t polarity)
    {
        mCallBacks[pin_num] = {event_handler, pin_num, polarity};
    }

    nrfx_gpiote_evt_handler_t Gpio::EventHandler(nrfx_gpiote_pin_t pin_num,
                                                 nrf_gpiote_polarity_t polarity)
    {
        auto CallBack = (mCallBacks[pin_num]).event_handler;
        CallBack();
        return NULL;
    }

    void Gpio::ConfigurePin(const gpio_config_t config)
    {
        uint32_t config_pin_number = config.pin_number;
        nrf_gpio_cfg_default(config_pin_number);
        nrf_gpio_pin_dir_t config_direction = static_cast<nrf_gpio_pin_dir_t>(config.direction);
        nrf_gpio_pin_input_t config_input = static_cast<nrf_gpio_pin_input_t>(config.input);
        nrf_gpio_pin_pull_t config_pull = static_cast<nrf_gpio_pin_pull_t>(config.pull);
        nrf_gpio_pin_drive_t config_drive = static_cast<nrf_gpio_pin_drive_t>(config.drive);
        nrf_gpio_pin_sense_t config_sense = static_cast<nrf_gpio_pin_sense_t>(config.sense);

        nrf_gpio_cfg(config_pin_number,
                     config_direction,
                     config_input,
                     config_pull,
                     config_drive,
                     config_sense);
    }

    void Gpio::ConfigurePin(uint32_t pin_num,
                            nrf_gpiote_polarity_t polarity,
                            nrf_drv_gpiote_in_config_t config,
                            HalPinEventHandler_t handler)
    {
        SetCallback(handler, pin_num, polarity);
        nrf_drv_gpiote_in_init(pin_num, &config, EventHandler(pin_num, polarity));
        nrf_drv_gpiote_in_event_enable(pin_num, true);
    }

    void Gpio::ConfigurePin(uint32_t pin_number, nrf_drv_gpiote_out_config_t config)
    {
        nrf_drv_gpiote_out_init(pin_number, &config);
    }
}
