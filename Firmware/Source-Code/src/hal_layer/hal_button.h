/**
 * @name Hornet / WPT Charger
 * @file hal_button.h
 * @brief Button Hardware Abstraction Layer for nrf52840
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef HAL_BUTTON_H
#define HAL_BUTTON_H

#include "hal_gpio.h"

#include "nrf_drv_gpiote.h"
#include "nrf_gpio.h"
#include "nrf_gpiote.h"

#define MAX_BUTTONS 3

namespace hal
{
    using ToggleCallbackFunction_t = void();
    class Button
    {
    public:
        /// Set a callback function for a button pin event
        ///
        ///@param button_pin The GPIO pin number of the button
        ///@param callback_function The function to be called when the button pin event occurs
        ///@param callback_function The function to be called when the button pin event occurs
        Button(uint32_t button_pin, ToggleCallbackFunction_t *callback_is_pressed = nullptr,
               ToggleCallbackFunction_t *callback_is_released = nullptr);

        /// Initialize the Button module
        static void Init(void);

        /// Read the state of a button
        ///
        ///@param button_pin The GPIO pin number of the button
        ///@return true if the button is pressed, false otherwise
        static bool Read(uint32_t button_pin);

    private:
        uint32_t m_button_pin;
        ToggleCallbackFunction_t *m_callback_is_pressed;
        ToggleCallbackFunction_t *m_callback_is_released;

        static Button *s_button_instances[MAX_BUTTONS];
        static uint8_t s_button_count;

        /// Helper function for configuring a pin
        ///@param pin_num Pin number
        ///@param polarity Polarity for the GPIOTE channel that triggers an interruption.
        static void ButtonHandler(nrfx_gpiote_pin_t pin_num,
                                  nrf_gpiote_polarity_t polarity);
    };
}

#endif