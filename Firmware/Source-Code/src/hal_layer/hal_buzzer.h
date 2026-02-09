/**
 * @name Hornet / WPT Charger
 * @file hal_buzzer.h
 * @brief Buzzer Hardware Abstraction Layer for nrf52840
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef HAL_BUZZER_H
#define HAL_BUZZER_H

#include "hal_pinout.h"

#include "nrf_drv_pwm.h"
#include "nrf_pwm.h"

#include <stdint.h>

namespace hal
{
    typedef struct
    {
        uint16_t intensity;
        uint16_t frequency; // in Hz
        uint16_t duration;  // in ms
    } sound_t;

    typedef struct
    {
        sound_t *notes;
        uint8_t length;
    } melody_t;

    class Buzzer
    {
    public:
        Buzzer();

        static void init(void);

        static void playMelody(melody_t melody);

        static uint8_t counter;

    private:
        static void pwmInit(nrf_drv_pwm_t *pwm, uint16_t frequency, uint16_t intensity);
        static void pwmUninit(nrf_drv_pwm_t *pwm);
        static void handler(nrf_drv_pwm_evt_type_t event_type);
        static void playSequence(sound_t note);

        static melody_t currentMelody;
        static sound_t currentSound;

        static uint16_t mFrequency;
        static uint16_t mIntensity;

        static nrf_pwm_sequence_t const mSequence;
        static nrf_pwm_values_individual_t mSeqValues[];

        static const uint32_t mBaseFrecuency = 1000000U;
        static nrf_drv_pwm_t mPwmInstance;
    };
}

#endif