/**
 * @name Hornet / WPT Charger
 * @file hal_led.cpp
 * @brief LED Hardware Abstraction Layer for nrf52840
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "hal_led.h"

#include "hal_gpio.h"
#include "hal_pinout.h"

#include "../core_layer/event_driven_architecture/manager/eda_manager.h"

#include "nrf_delay.h"

namespace hal
{
    // Static members definition
    nrf_pwm_values_common_t Leds::s_sequence[LEDS_SEQUENCE_LENGTH_TOTAL] = {};
    Leds Leds::s_instance;
    bool Leds::s_initialized = false;

    Leds& Leds::GetInstance()
    {
        return s_instance;
    }

    void Leds::Initialize()
    {
        if (!s_initialized) {
            GetInstance().Init();
            s_initialized = true;
        }
    }

    void Leds::Uninitialize()
    {
        if (s_initialized) {
            GetInstance().UnInit();
            s_initialized = false;
        }
    }

    Leds::Leds() : m_pwm_driver(NRFX_PWM_INSTANCE(0))
    {
    }

    void Leds::TurnLedOn(RgbLed_t *led)
    {
        led->state = LedState_e::ON;
        Show();
    }

    void Leds::TurnLedOff(RgbLed_t *led)
    {
        led->state = LedState_e::OFF;
        Clear(led);
        Show();
    }

    LedState_e Leds::GetLedState(RgbLed_t led)
    {
        return led.state;
    }

    void Leds::SetLedIntensity(RgbLed_t *led, LedIntensity_e intensity)
    {
        led->intensity = intensity;
    }

    void Leds::SetLedColor(RgbLed_t *led, LedPosition_e position, LedColor_e color)
    {
        switch (color)
        {
        case LedColor_e::RED:
            ConfigureLedColor(led, position, 100, 0, 0);
            break;
        case LedColor_e::GREEN:
            ConfigureLedColor(led, position, 0, 100, 0);
            break;
        case LedColor_e::BLUE:
            ConfigureLedColor(led, position, 0, 0, 100);
            break;
        case LedColor_e::YELLOW:
            ConfigureLedColor(led, position, 100, 100, 0);
            break;
        case LedColor_e::CYAN:
            ConfigureLedColor(led, position, 100, 0, 100);
            break;
        case LedColor_e::MAGENTA:
            ConfigureLedColor(led, position, 0, 100, 100);
            break;
        case LedColor_e::WHITE:
            ConfigureLedColor(led, position, 100, 100, 100);
            break;
        default:
            break;
        }
    }

    void Leds::Init(void)
    {
        hal::Gpio::gpio_config_t ledPinConfig = {
            .pin_number = PIN_LEDS,
            .direction = hal::Gpio::gpio_pin_dir_t::NRF_GPIO_PIN_DIR_OUTPUT,
            .input = hal::Gpio::gpio_pin_input_t::NRF_GPIO_PIN_INPUT_DISCONNECT,
            .pull = hal::Gpio::gpio_pin_pull_t::NRF_GPIO_PIN_NOPULL,
            .drive = hal::Gpio::gpio_pin_drive_t::NRF_GPIO_PIN_S0S1,
            .sense = hal::Gpio::gpio_pin_sense_t::NRF_GPIO_PIN_NOSENSE};

        hal::Gpio::ConfigurePin(ledPinConfig);

        nrfx_pwm_config_t const CONFIG =
            {
                .output_pins =
                    {
                        PIN_LEDS,
                        NRFX_PWM_PIN_NOT_USED,
                        NRFX_PWM_PIN_NOT_USED,
                        NRFX_PWM_PIN_NOT_USED},
                .irq_priority = NRFX_PWM_DEFAULT_CONFIG_IRQ_PRIORITY,
                .base_clock = NRF_PWM_CLK_16MHz,
                .count_mode = NRF_PWM_MODE_UP,
                .top_value = LEDS_PERIOD_TICKS,
                .load_mode = NRF_PWM_LOAD_COMMON,
                .step_mode = NRF_PWM_STEP_AUTO,
            };

        nrfx_pwm_init(&m_pwm_driver, &CONFIG, nullptr);
    }

    void Leds::UnInit(void)
    {
        // Turn off all LEDs
        TurnLedOff(&rgb_led);
        nrfx_pwm_uninit(&m_pwm_driver);
    }

    void Leds::ConfigureLedColor(RgbLed_t *led, LedPosition_e position, uint8_t red, uint8_t green, uint8_t blue)
    {
        // Casting from enum to uint8_t for intensity calculation
        float scaling_factor = static_cast<float>(static_cast<uint8_t>(led->intensity)) / 100.0f;
        
        // Explicit and correct casting from enum to uint8_t
        uint8_t s_position = static_cast<uint8_t>(position);
        
        // Ensure values are in correct range with proper casting
        uint8_t scaled_red = static_cast<uint8_t>(red * scaling_factor);
        uint8_t scaled_green = static_cast<uint8_t>(green * scaling_factor);
        uint8_t scaled_blue = static_cast<uint8_t>(blue * scaling_factor);

        // Encode each color bit as PWM sequence
        for (uint8_t j = 0; j < 8; j++)
        {
            s_sequence[s_position * LEDS_SEQUENCE_LENGTH_PER_LED + j] = ((scaled_green << j) & 0x80) ? LEDS_CODED_ONE : LEDS_CODED_ZERO;       // green
            s_sequence[s_position * LEDS_SEQUENCE_LENGTH_PER_LED + (j + 8)] = ((scaled_red << j) & 0x80) ? LEDS_CODED_ONE : LEDS_CODED_ZERO;   // red
            s_sequence[s_position * LEDS_SEQUENCE_LENGTH_PER_LED + (j + 16)] = ((scaled_blue << j) & 0x80) ? LEDS_CODED_ONE : LEDS_CODED_ZERO; // blue
        }
    }

    void Leds::Show(void)
    {
        nrf_pwm_sequence_t sequence = {};

        sequence.values.p_common = s_sequence;
        sequence.length = LEDS_SEQUENCE_LENGTH_TOTAL;
        sequence.repeats = 0;
        sequence.end_delay = 0;

        // Ensure PWM is stopped before starting new sequence
        while (!nrfx_pwm_is_stopped(&m_pwm_driver))
        {
            nrfx_pwm_stop(&m_pwm_driver, true);
        }
        nrfx_pwm_simple_playback(&m_pwm_driver, &sequence, 1, NRFX_PWM_FLAG_STOP);
        // This delay is necessary to ensure that the PWM information reaches the last LED in the chain.
        // The data transmission time is approximately 1.25 microseconds.
        // Considering the number of LEDs and the bit data for each color, we require about 75 microseconds.
        nrf_delay_us(75);
    }

    void Leds::Clear(RgbLed_t *led)
    {
        // Turn off all LEDs by setting all colors to zero
        ConfigureLedColor(led, LedPosition_e::LED1, 0, 0, 0);
        ConfigureLedColor(led, LedPosition_e::LED2, 0, 0, 0);
        ConfigureLedColor(led, LedPosition_e::LED3, 0, 0, 0);
    }

    void Leds::ClearPosition(RgbLed_t *led, LedPosition_e position)
    {
        ConfigureLedColor(led, position, 0, 0, 0);
    }

    void Leds::TestLeds()
    {
        // Array of colors to test
        LedColor_e colors[] = {
            LedColor_e::RED,
            LedColor_e::GREEN,
            LedColor_e::BLUE,
            LedColor_e::YELLOW,
            LedColor_e::CYAN,
            LedColor_e::MAGENTA,
            LedColor_e::WHITE,
        };

        // Test all LEDs
        for (int led_position = static_cast<int>(LedPosition_e::LED1);
             led_position <= static_cast<int>(LedPosition_e::LED3);
             led_position++)
        {
            for (LedColor_e color : colors)
            {
                SetLedColor(&rgb_led, static_cast<LedPosition_e>(led_position), color);
                TurnLedOn(&rgb_led);
                nrf_delay_ms(250); // 250 milliseconds to see the color
            }
        }

        // Turn off all LEDs
        TurnLedOff(&rgb_led);
    }

    void Leds::LedScanOn(bool enable)
    {
        rgb_led.intensity = LedIntensity_e::HIGH;

        if (enable) {
            SetLedColor(&rgb_led, LedPosition_e::LED1, LedColor_e::BLUE);
        } else {
            ClearPosition(&rgb_led, LedPosition_e::LED1);
        }

        TurnLedOn(&rgb_led); // hoy solo hace Show()
    }

    void Leds::LedCharging(bool enable)
    {
        rgb_led.intensity = LedIntensity_e::HIGH;

        if (enable) {
            SetLedColor(&rgb_led, LedPosition_e::LED1, LedColor_e::YELLOW);
        } else {
            ClearPosition(&rgb_led, LedPosition_e::LED1);
        }

        TurnLedOn(&rgb_led);
    }

    void Leds::LedChargingSlow(bool enable)
    {
        rgb_led.intensity = LedIntensity_e::HIGH;

        if (enable) {
            SetLedColor(&rgb_led, LedPosition_e::LED1, LedColor_e::WHITE);
        } else {
            ClearPosition(&rgb_led, LedPosition_e::LED1);
        }

        TurnLedOn(&rgb_led);
    }

    void Leds::LedCharged(bool enable)
    {
        rgb_led.intensity = LedIntensity_e::HIGH;

        if (enable) {
            SetLedColor(&rgb_led, LedPosition_e::LED2, LedColor_e::GREEN);
        } else {
            ClearPosition(&rgb_led, LedPosition_e::LED2);
        }

        TurnLedOn(&rgb_led);
        nrf_delay_us(100);
    }
}