/**
 * @name Hornet / WPT Charger
 * @file hal_buzzer.cpp
 * @brief Buzzer Hardware Abstraction Layer for nrf52840
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "hal_buzzer.h"
#include "hal_gpio.h"

static nrf_drv_pwm_t PWM_BUZZER = NRF_DRV_PWM_INSTANCE(0);

namespace hal
{
    Buzzer::Buzzer()
    {
    }

    nrf_drv_pwm_t Buzzer::mPwmInstance;
    nrf_pwm_values_individual_t Buzzer::mSeqValues[] = {0, 0, 0, 0};
    nrf_pwm_sequence_t const Buzzer::mSequence =
        {
            .values = {.p_individual = mSeqValues},
            .length = NRF_PWM_VALUES_LENGTH(mSeqValues),
            .repeats = 0,
            .end_delay = 0};

    void Buzzer::pwmInit(nrf_drv_pwm_t *pwm, uint16_t frequency, uint16_t intensity)
    {
        mFrequency = frequency;
        mIntensity = intensity;
        uint16_t topValue = mBaseFrecuency / frequency;

        hal::Gpio::gpio_config_t buzzerPinConfig = {
            .pin_number = PIN_BUZZER,
            .direction = hal::Gpio::gpio_pin_dir_t::NRF_GPIO_PIN_DIR_OUTPUT,
            .input = hal::Gpio::gpio_pin_input_t::NRF_GPIO_PIN_INPUT_DISCONNECT,
            .pull = hal::Gpio::gpio_pin_pull_t::NRF_GPIO_PIN_NOPULL,
            .drive = hal::Gpio::gpio_pin_drive_t::NRF_GPIO_PIN_S0S1,
            .sense = hal::Gpio::gpio_pin_sense_t::NRF_GPIO_PIN_NOSENSE};

        hal::Gpio::ConfigurePin(buzzerPinConfig);

        nrf_drv_pwm_config_t pwmConfig =
            {
                .output_pins =
                    {
                        PIN_BUZZER,
                        NRF_DRV_PWM_PIN_NOT_USED,
                        NRF_DRV_PWM_PIN_NOT_USED,
                        NRF_DRV_PWM_PIN_NOT_USED,
                    },
                .irq_priority = APP_IRQ_PRIORITY_LOWEST,
                .base_clock = NRF_PWM_CLK_1MHz,
                .count_mode = NRF_PWM_MODE_UP,
                .top_value = topValue,
                .load_mode = NRF_PWM_LOAD_INDIVIDUAL,
                .step_mode = NRF_PWM_STEP_AUTO,
            };

        if (nrf_drv_pwm_init(pwm, &pwmConfig, Buzzer::handler) != NRF_SUCCESS)
        {
            nrf_drv_pwm_uninit(&mPwmInstance);
            nrf_drv_pwm_init(pwm, &pwmConfig, Buzzer::handler);
        }
    }

    void Buzzer::pwmUninit(nrf_drv_pwm_t *pwm)
    {
        nrf_drv_pwm_uninit(pwm);
    }

    void Buzzer::init()
    {
        mPwmInstance = PWM_BUZZER;
    }

    void Buzzer::handler(nrf_drv_pwm_evt_type_t event_type)
    {
        static uint8_t sequenceIndex = 0;

        if (event_type == NRF_DRV_PWM_EVT_FINISHED)
        {
            if (sequenceIndex < currentMelody.length)
            {
                Buzzer::playSequence(Buzzer::currentMelody.notes[sequenceIndex]);
                sequenceIndex++;
            }
            else
            {
                nrf_drv_pwm_uninit(&mPwmInstance);
                sequenceIndex = 0;
            }
        }
    }

    void Buzzer::playMelody(melody_t melody)
    {
        currentMelody = melody;
        Buzzer::playSequence(currentMelody.notes[0]);
    }

    void Buzzer::playSequence(sound_t note)
    {
        Buzzer::pwmInit(&mPwmInstance, note.frequency, note.intensity);

        uint16_t topValue = mBaseFrecuency / note.frequency;

        uint16_t dutyCycle = topValue - (topValue * note.intensity) / 100;

        mSeqValues->channel_0 = dutyCycle;

        nrf_pwm_sequence_t const mSequence =
            {
                .values = {.p_individual = mSeqValues},
                .length = NRF_PWM_VALUES_LENGTH(mSeqValues),
                .repeats = 0,
                .end_delay = 0};

        currentSound = note;
        nrf_drv_pwm_simple_playback(&mPwmInstance, &Buzzer::mSequence, 1, NRF_DRV_PWM_FLAG_STOP);
    }

}