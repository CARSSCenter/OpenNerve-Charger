/**
 * @name Hornet / WPT Charger
 * @file hal_button.cpp
 * @brief Button Hardware Abstraction Layer for nrf52840
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "../core_layer/event_driven_architecture/manager/eda_manager_log_config.h"

#include "hal_button.h"
#include "hal_gpio.h"

#include "nrf_gpio.h"
#include "app_error.h"

namespace hal
{
    Button *Button::s_button_instances[MAX_BUTTONS] = {nullptr};
    uint8_t Button::s_button_count = 0;

    Button::Button(uint32_t button_pin, ToggleCallbackFunction_t *callback_is_pressed,
                   ToggleCallbackFunction_t *callback_is_released)
        : m_button_pin(button_pin),
          m_callback_is_pressed(callback_is_pressed),
          m_callback_is_released(callback_is_released)
    {
        if (s_button_count < MAX_BUTTONS)
        {
            ret_code_t err_code;
            nrfx_gpiote_in_config_t pin_in_config = NRFX_GPIOTE_CONFIG_IN_SENSE_TOGGLE(true);
            pin_in_config.pull = NRF_GPIO_PIN_PULLUP;

            nrf_gpiote_polarity_t polarity = NRF_GPIOTE_POLARITY_TOGGLE;
            err_code = nrf_drv_gpiote_in_init(button_pin, &pin_in_config, ButtonHandler);
            APP_ERROR_CHECK(err_code);
            nrf_drv_gpiote_in_event_enable(button_pin, true);

            s_button_instances[s_button_count++] = this;
        }
        else
        {
            LOG_ERROR("Too many buttons. Max allowed: %d\n", MAX_BUTTONS);
        }
    }

    void Button::Init()
    {
        Gpio::Init();
    }

    bool Button::Read(uint32_t button_pin)
    {
        return Gpio::Read(button_pin) == 0;
    }

    void Button::ButtonHandler(nrfx_gpiote_pin_t pin_num,
                               nrf_gpiote_polarity_t polarity)
    {
        for (uint8_t i = 0; i < s_button_count; i++)
        {
            if (s_button_instances[i] && s_button_instances[i]->m_button_pin == pin_num)
            {
                bool is_pressed = !nrf_drv_gpiote_in_is_set(pin_num);

                if (is_pressed)
                {
                    if (s_button_instances[i]->m_callback_is_pressed)
                    {
                        s_button_instances[i]->m_callback_is_pressed();
                    }
                    LOG_INFO("Button %u pressed\n", pin_num);
                }
                else
                {
                    if (s_button_instances[i]->m_callback_is_released)
                    {
                        s_button_instances[i]->m_callback_is_released();
                    }
                    LOG_INFO("Button %u released\n", pin_num);
                }
                break;
            }
        }
    }
}